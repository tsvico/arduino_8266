/*
  智能语言控制控制，支持同时天猫、小爱、小度、google Assistent控制
  也同时支持web控制、小程序控制、app控制，定时控制等
  QQ群：566565915
  项目示例：通过发送on或off控制开关
  官网：bemfa.com
*/

#include <ESP8266WiFi.h>               //默认，加载WIFI头文件
#include "PubSubClient.h"              //默认，加载MQTT库文件
#include <ESP8266httpUpdate.h>
#include <WiFiManager.h>                //wifimanager库

//********************需要修改的部分*******************//
#define ID_MQTT  "x"     //用户私钥，控制台获取
String uid = "x";  //用户私钥，同上
const char*  topic = "socket001";        //主题名字，可在巴法云控制台自行创建，名称随意
const int LED_Pin = 12;              //单片机LED引脚值，D2是NodeMcu引脚命名方式，其他esp8266型号将D2改为自己的引脚
const int plug = 5;
const int button = 4;
#define BOOT_AFTER_UPDATE false

//固件链接，在巴法云控制台复制、粘贴到这里即可
String upUrl = "http://bin.bemfa.com/b/x=socket001.bin";
//**************************************************//

/************************微信推送*********************/
String type = "2";    // 1表示是预警消息，2表示设备提醒消息
String device = "插座";                           // 设备名称
String msgOn = "插座已打开，请查看设备";       //发送的消息
String msgOff = "插座已关闭";       //发送的消息
int delaytime = 0;                                          //为了防止被设备“骚扰”，可设置贤者时间，单位是秒，如果设置了该值，在该时间内不会发消息到微信，设置为0立即推送。
String ApiUrl = "http://api.bemfa.com/api/wechat/v1/";
HTTPClient http;  //初始化http

//**************************************************//

const char* mqtt_server = "bemfa.com"; //默认，MQTT服务器
const int mqtt_server_port = 9501;      //默认，MQTT服务器
WiFiClient espClient;
PubSubClient client(espClient);

//灯光函数及引脚定义
void turnOnLed();
void turnOffLed();
void updateBin();

void handlePreOtaUpdateCallback() {
  Update.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("CUSTOM Progress: %u%%\r", (progress / (total / 100)));
  });
}

//使用web配置网络
WiFiManager wifiManager;
char devname[] = "Lenovo_configAP";   //定义WIFI名称和主机名

void setup_wifi() {
  delay(10);
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

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Topic:");
  Serial.println(topic);
  String msg = "";
  for (int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }
  Serial.print("Msg:");
  Serial.println(msg);
  if (msg == "on") {//如果接收字符on，亮灯
    turnOnLed();//开灯函数
  } else if (msg == "off") {//如果接收字符off，亮灯
    turnOffLed();//关灯函数
  } else if (msg = "update") {
    updateBin();//执行升级函数
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(ID_MQTT)) {
      Serial.println("connected");
      Serial.print("subscribe:");
      Serial.println(topic);
      //订阅主题，如果需要订阅多个主题，可发送多条订阅指令client.subscribe(topic2);client.subscribe(topic3);
      client.subscribe(topic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
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
void doHttpStick(String msg){  //微信消息推送函数
  String postData;
  //Post Data
  postData = "uid="+uid+"&type=" + type +"&time="+delaytime+"&device="+device+"&msg="+msg;
  http.begin(espClient,ApiUrl);              //Specify request destination
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");    //Specify content-type header
  int httpCode = http.POST(postData);   //Send the request
  String payload = http.getString();    //Get the response payload
  Serial.println(httpCode);   //Print HTTP return code
  Serial.println(payload);    //Print request response payload
  http.end();  //Close connection
  Serial.println("send success");  
  }
//=======================================================================


void setup() {
  Serial.begin(115200);
  pinMode(LED_Pin, OUTPUT);
  pinMode(plug, OUTPUT);
  digitalWrite(LED_Pin, LOW);
  digitalWrite(plug, HIGH);
  pinMode(button, INPUT);
  Serial.println("Beginning...");
  
  setup_wifi();           //设置wifi的函数，连接wifi
  client.setServer(mqtt_server, mqtt_server_port);//设置mqtt服务器
  client.setCallback(callback); //mqtt消息处理

}
void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  buttonCheck();
}

//打开灯泡
void turnOnLed() {
  Serial.println("Turn ON");
  digitalWrite(LED_Pin, LOW);
  //Serial.println("LEd down");
  digitalWrite(plug, HIGH);
  //Serial.println("plug down");
  doHttpStick(msgOn); //发送打开通知
}
//关闭灯泡
void turnOffLed() {
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
