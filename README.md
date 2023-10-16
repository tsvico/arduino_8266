# arduino 8266 相关代码

## kaiguan （esp01s 模块 + 继电器）

点灯基础用法，增加了 udp 开关设备

## peiwang（esp01s 模块 + 继电器）

使用点灯，增加自动配网相关代码

使用 https://github.com/tzapu/WiFiManager

## 8266_bafa_mqtt（esp01s 模块 + 继电器）

使用巴法，使用 mqtt 协议

使用 https://github.com/tzapu/WiFiManager

## smartPlug（小黄鱼买的联想插座）

点灯控制联想 smartPlug 设备 https://www.mydigit.cn/thread-271903-1-1.html
除自动配网外增加自动更新

## smartPlug_bafa（小黄鱼买的联想插座）

同上，使用巴法控制设备，支持巴法 ota ，慎用 ota❗，测试过程中把 8266 搞坏了
