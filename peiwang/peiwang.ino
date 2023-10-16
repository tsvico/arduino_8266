#define BLINKER_WIFI                    //接入方式：wifi
#define BLINKER_MIOT_OUTLET              //接入小爱插座库
#define BLINKER_PRINT Serial            //串口协议库
#include <WiFiManager.h>                //wifimanager库
#include <Blinker.h>                    //官方库

char auth[] = "x";           //设备key号


int relay = 0; //继电器io0
int key = 2; //本地轻触开关的引脚，io2与gnd接时动作
String device = "开关";                           // 设备名称
String msgOn = device + "已打开";
String msgOff = device + "已关闭";


//使用web配置网络
WiFiManager wifiManager;
char devname[] = "8266_AutoConnectAP";   //定义WIFI名称和主机名
void WebConfig() {
  Serial.println("Entered config mode...");
  // 自动连接WiFi,参数是连接ESP8266时的WiFi名称
  wifiManager.setConfigPortalTimeout(60);  //设置连接超时时间60s
  wifiManager.autoConnect(devname);
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


//*******新建组件对象
BlinkerButton Button1("btn-abc");              //设置app按键的键名

//*******app上按下按键即会执行该函数app里按键有2种模式3钟不同样式，下面写出所有的开关状态。
void button1_callback(const String & state)        //用state来存储组键的值按键 : "tap"(点按); "pre"(长按); "pup"(释放)开关 : "on"(打开); "off"(关闭)
{
  BLINKER_LOG("app操作了!: ", state);                //串口打印

  if (digitalRead(relay) == LOW) {            //如果state是on状态
    dev_off();
  }
  else if (digitalRead(relay) == HIGH) {     //如果state是off状态
    dev_open();
  }
}
//*******如果小爱有对设备进行操作就执行下面
void miotPowerState(const String & state)
{
  BLINKER_LOG("小爱语音操作!");              //串口打印

  if (state == BLINKER_CMD_ON) {
    dev_open();
    BlinkerMIOT.powerState("on");
    BLINKER_LOG(msgOn);
    BlinkerMIOT.print();
  }
  else if (state == BLINKER_CMD_OFF) {
    dev_off();
    BlinkerMIOT.powerState("off");
    BLINKER_LOG(msgOff);
    BlinkerMIOT.print();
  }
}

void miotQuery(int32_t queryCode)       //小爱设备查询的回调函数
{
  BLINKER_LOG("MIOT Query codes: ", queryCode);

  switch (queryCode)
  {
    case BLINKER_CMD_QUERY_ALL_NUMBER :
      BLINKER_LOG("MIOT Query All");
      BlinkerMIOT.powerState(relay ? "on" : "off");
      BlinkerMIOT.print();
      break;
    case BLINKER_CMD_QUERY_POWERSTATE_NUMBER :
      BLINKER_LOG("MIOT Query Power State");
      BlinkerMIOT.powerState(relay ? "on" : "off");
      BlinkerMIOT.print();
      break;

    default :
      BlinkerMIOT.powerState(relay ? "on" : "off");
      BlinkerMIOT.print();
      break;
  }
}

void dataRead(const String & data)
{
  BLINKER_LOG("Blinker readString: ", data);

  Blinker.vibrate();

  uint32_t BlinkerTime = millis();

  Blinker.print("millis", BlinkerTime);
}


//*******app定时向设备发送心跳包, 设备收到心跳包后会返回设备当前状态
void heartbeat()
{
  BLINKER_LOG("状态同步!");
  if (digitalRead(relay) == LOW)
  {
    Button1.print("on");
    Button1.color("red");
    Button1.text(msgOn);
  }
  else
  {
    Button1.print("off");
    Button1.color("gray");
    Button1.text(msgOff);
  }
}

void dev_open() {
  BLINKER_LOG(msgOn);                         //串口打印
  digitalWrite(relay, LOW);
  Button1.color("red");                  //设置app按键是红色
  Button1.text(msgOn);
  Button1.print("on");                      //反馈回按键状态是开
}

void dev_off() {
  BLINKER_LOG(msgOff);                           //串口打印
  digitalWrite(relay, HIGH);
  Button1.color("gray");                     //设置app按键是灰色
  Button1.text(msgOff);
  Button1.print("off");                         //反馈回按键状态是关
}
//如果本地开关有动作执行下面手动模式
void sdms()
{
  if (digitalRead(relay) == HIGH && digitalRead(key) == LOW)
  {
    Blinker.delay(400);  //延时400ms，最少需150ms
    if (digitalRead(key) == HIGH)
    {
      dev_open();
    }
  }
  if (digitalRead(relay) == LOW && digitalRead(key) == LOW)
  {
    Blinker.delay(400);
    if (digitalRead(key) == HIGH)
    {
      dev_off();
    }
  }
}
void setup()
{
  // 初始化串口
  Serial.begin(115200);
  delay(10);
  BLINKER_DEBUG.stream(Serial);
  BLINKER_DEBUG.debugAll();

  pinMode(relay, OUTPUT);
  digitalWrite(relay, HIGH);        //默认上电关  LOW 时接通继电器。
  pinMode(key, INPUT_PULLUP);       //输入上拉

  // 初始化blinker
  WebConfig();  //配置网络
  Blinker.begin(auth, WiFi.SSID().c_str(), WiFi.psk().c_str()); //初始化blinker
  Button1.attach(button1_callback);        //app按键回调
  Blinker.attachData(dataRead);  //状态回调
  Blinker.attachHeartbeat(heartbeat);              //app定时向设备发送心跳包, 设备收到心跳包后会返回设备当前状态进行语音操作和app操作同步。

  BlinkerMIOT.attachPowerState(miotPowerState);              //小爱语音操作注册函数

  BlinkerMIOT.attachQuery(miotQuery);//小爱设备查询的回调函数
}

void loop() {
  Blinker.run();
  // sdms();//本地开关扫描
}
