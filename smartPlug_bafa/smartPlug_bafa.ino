/*
   智能语言控制控制，支持天猫、小爱、小度、google Assistent同时控制
*/
#include <ESP8266WiFi.h>
#include <ESP8266httpUpdate.h>
#include <WiFiManager.h>                //wifimanager库

//名称卧室台灯
#define server_ip "bemfa.com" //巴法云服务器地址默认即可
#define server_port "8344" //服务器端口，tcp创客云端口8344
#define BOOT_AFTER_UPDATE false

//********************需要修改的部分*******************//

String UID = "x";  //用户私钥，可在控制台获取,修改为自己的UID
String TOPIC = "socket1001";         //主题名字，可在控制台新建
const int LED_Pin = 12;              //单片机LED引脚值，D2是NodeMcu引脚命名方式，其他esp8266型号将D2改为自己的引脚
const int plug = 5;
const int button = 4;
boolean buttonStatue = true;
//固件链接，在巴法云控制台复制、粘贴到这里即可
String upUrl = "http://bin.bemfa.com/b/xx.bin";

/************************微信推送*********************/
String type = "2";    // 1表示是预警消息，2表示设备提醒消息
String device = "插座";                           // 设备名称
String msgOn = "插座已打开，请查看设备";       //发送的消息
String msgOff = "插座已关闭";       //发送的消息
int delaytime = 0;                                          //为了防止被设备“骚扰”，可设置贤者时间，单位是秒，如果设置了该值，在该时间内不会发消息到微信，设置为0立即推送。
//String ApiUrl = "https://go.bemfa.com/v1/sendwechat";        //默认 api 网址
String ApiUrl = "http://api.bemfa.com/api/wechat/v1/";
HTTPClient http;  //初始化http

//**************************************************//



//**************************************************//
//最大字节数
#define MAX_PACKETSIZE 512
//设置心跳值30s
#define KEEPALIVEATIME 30*1000
//tcp客户端相关初始化，默认即可
WiFiClient TCPclient;
String TcpClient_Buff = "";//初始化字符串，用于接收服务器发来的数据
unsigned int TcpClient_BuffIndex = 0;
unsigned long TcpClient_preTick = 0;
unsigned long preHeartTick = 0;//心跳
unsigned long preTCPStartTick = 0;//连接
bool preTCPConnected = false;
//相关函数初始化
//连接WIFI
void doWiFiTick();
void startSTA();

//TCP初始化连接
void doTCPClientTick();
void startTCPClient();
void sendtoTCPServer(String p);

//led控制函数，具体函数内容见下方
void turnOnLed();
void turnOffLed();



/*
   发送数据到TCP服务器
*/
void sendtoTCPServer(String p) {
  if (!TCPclient.connected())
  {
    Serial.println("Client is not readly");
    return;
  }
  TCPclient.print(p);
}


/*
   初始化和服务器建立连接
*/
void startTCPClient() {
  if (TCPclient.connect(server_ip, atoi(server_port))) {
    Serial.print("\nConnected to server:");
    Serial.printf("%s:%d\r\n", server_ip, atoi(server_port));

    String tcpTemp = ""; //初始化字符串
    tcpTemp = "cmd=1&uid=" + UID + "&topic=" + TOPIC + "\r\n"; //构建订阅指令
    sendtoTCPServer(tcpTemp); //发送订阅指令
    tcpTemp = ""; //清空
    /*
      //如果需要订阅多个主题，可再次发送订阅指令
      tcpTemp = "cmd=1&uid="+UID+"&topic="+主题2+"\r\n"; //构建订阅指令
      sendtoTCPServer(tcpTemp); //发送订阅指令
      tcpTemp="";//清空
    */

    preTCPConnected = true;
    preHeartTick = millis();
    TCPclient.setNoDelay(true);
  }
  else {
    Serial.print("Failed connected to server:");
    Serial.println(server_ip);
    TCPclient.stop();
    preTCPConnected = false;
  }
  preTCPStartTick = millis();
}


/*
   检查数据，发送心跳
*/
void doTCPClientTick() {
  //检查是否断开，断开后重连
  if (WiFi.status() != WL_CONNECTED) return;
  if (!TCPclient.connected()) {//断开重连
    if (preTCPConnected == true) {
      preTCPConnected = false;
      preTCPStartTick = millis();
      Serial.println();
      Serial.println("TCP Client disconnected.");
      TCPclient.stop();
    }
    else if (millis() - preTCPStartTick > 1 * 1000) //重新连接
      startTCPClient();
  }
  else
  {
    if (TCPclient.available()) {//收数据
      char c = TCPclient.read();
      TcpClient_Buff += c;
      TcpClient_BuffIndex++;
      TcpClient_preTick = millis();

      if (TcpClient_BuffIndex >= MAX_PACKETSIZE - 1) {
        TcpClient_BuffIndex = MAX_PACKETSIZE - 2;
        TcpClient_preTick = TcpClient_preTick - 200;
      }
      preHeartTick = millis();
    }
    if (millis() - preHeartTick >= KEEPALIVEATIME) { //保持心跳
      preHeartTick = millis();
      Serial.println("--Keep alive:");
      sendtoTCPServer("ping\r\n"); //发送心跳，指令需\r\n结尾，详见接入文档介绍
    }
  }
  if ((TcpClient_Buff.length() >= 1) && (millis() - TcpClient_preTick >= 200))
  {
    TCPclient.flush();
    Serial.print("Rev string: ");
    TcpClient_Buff.trim(); //去掉首位空格
    Serial.println(TcpClient_Buff); //打印接收到的消息
    String getTopic = "";
    String getMsg = "";
    if (TcpClient_Buff.length() > 15) { //注意TcpClient_Buff只是个字符串，在上面开头做了初始化 String TcpClient_Buff = "";
      //此时会收到推送的指令，指令大概为 cmd=2&uid=xxx&topic=light002&msg=off
      int topicIndex = TcpClient_Buff.indexOf("&topic=") + 7; //c语言字符串查找，查找&topic=位置，并移动7位，不懂的可百度c语言字符串查找
      int msgIndex = TcpClient_Buff.indexOf("&msg=");//c语言字符串查找，查找&msg=位置
      getTopic = TcpClient_Buff.substring(topicIndex, msgIndex); //c语言字符串截取，截取到topic,不懂的可百度c语言字符串截取
      getMsg = TcpClient_Buff.substring(msgIndex + 5); //c语言字符串截取，截取到消息
      Serial.print("topic:------");
      Serial.println(getTopic); //打印截取到的主题值
      Serial.print("msg:--------");
      Serial.println(getMsg);   //打印截取到的消息值
    }
    if (getMsg  == "on") {     //如果收到指令on==打开灯
      turnOnLed();
    } else if (getMsg == "off") { //如果收到指令off==关闭灯
      turnOffLed();
    } else if (getMsg == "update") { //如果收到指令update
      updateBin();//执行升级函数
    }

    TcpClient_Buff = "";
    TcpClient_BuffIndex = 0;
  }
}

void handlePreOtaUpdateCallback() {
  Update.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("CUSTOM Progress: %u%%\r", (progress / (total / 100)));
  });
}

//使用web配置网络
WiFiManager wifiManager;
char devname[] = "Lenovo_configAP";   //定义WIFI名称和主机名
void WebConfig() {
  Serial.println("Entered config mode...");
  // 自动连接WiFi,参数是连接ESP8266时的WiFi名称
  wifiManager.setConfigPortalTimeout(60);  //设置连接超时时间60s
  wifiManager.autoConnect(devname);
  wifiManager.setPreOtaUpdateCallback(handlePreOtaUpdateCallback);
  std::vector<const char *> menu = { "wifi", "info", "param", "update", "close", "sep", "erase", "restart", "exit"};
  wifiManager.setMenu(menu);
  WiFi.hostname(devname);  //设置主机名
  // WiFi连接成功后将通过串口监视器输出连接成功信息
  Serial.println("");
  Serial.print("ESP8266 Connected to ");
  Serial.println(WiFi.SSID());              // WiFi名称
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());           // IP
}


//打开灯泡
void turnOnLed() {
  //doHttpStick(msgOn); //发送打开通知
  Serial.println("Turn ON");
  digitalWrite(LED_Pin, LOW);
  //Serial.println("LEd down");
  digitalWrite(plug, HIGH);
  //Serial.println("plug down");
  doHttpStick(msgOn); //发送打开通知

}
//关闭灯泡
void turnOffLed() {
  doHttpStick(msgOff); //发送关闭通知
  Serial.println("Turn OFF");
  digitalWrite(LED_Pin, HIGH);
  digitalWrite(plug, LOW);
  doHttpStick(msgOff); //发送关闭通知
}
//按钮检测执行关闭或打开
void buttonCheck()
{
  if (digitalRead(button) == LOW)
  {
    delay(10);
    if (digitalRead(button) == LOW)
    {
      digitalWrite(LED_Pin, !digitalRead(LED_Pin));
      digitalWrite(plug, !digitalRead(plug));
      while (digitalRead(button) == LOW);
      //      buttonStatue = !buttonStatue;
      //      if(buttonStatue == true)
      //      {
      //        turnOnLed();
      //      }
      //      else
      //      {
      //        turnOffLed();
      //      }
    }
  }
}
void updateBin() {
  Serial.println("start update");
  WiFiClient UpdateClient;
  ESPhttpUpdate.rebootOnUpdate(BOOT_AFTER_UPDATE);
  t_httpUpdate_return ret = ESPhttpUpdate.update(UpdateClient, upUrl);
  switch (ret) {
    case HTTP_UPDATE_FAILED:      //当升级失败
      Serial.println("[update] Update failed.");
      break;
    case HTTP_UPDATE_NO_UPDATES:  //当无升级
      Serial.println("[update] Update no Update.");
      break;
    case HTTP_UPDATE_OK:         //当升级成功
      Serial.println("[update] Update ok.");
      break;
  }
}

//******微信消息推送函数********//
void doHttpStick(String msg) { //微信消息推送函数
  String postData;
  //Post Data
  postData = "uid=" + UID + "&type=" + type + "&time=" + delaytime + "&device=" + device + "&msg=" + msg;
  http.begin(TCPclient, ApiUrl);             //Specify request destination
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");    //Specify content-type header
  int httpCode = http.POST(postData);   //Send the request
  String payload = http.getString();    //Get the response payload
  Serial.println(httpCode);   //Print HTTP return code
  Serial.println(payload);    //Print request response payload
  http.end();  //Close connection
  Serial.println("send success");
}
//=======================================================================

// 初始化，相当于main 函数
void setup() {
  Serial.begin(115200);
  pinMode(LED_Pin, OUTPUT);
  pinMode(plug, OUTPUT);
  digitalWrite(LED_Pin, LOW);
  digitalWrite(plug, HIGH);
  pinMode(button, INPUT);
  Serial.println("Beginning...");

  WebConfig();  //配置网络
}

//循环
void loop() {
  doTCPClientTick();
  buttonCheck();
}
