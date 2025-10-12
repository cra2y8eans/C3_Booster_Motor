/***********************************************************************************************************************************************************

ESP32_C3 使用TMC2209驱动42步进电机实现电推脚控  分支：main

        使用TMC2209驱动板驱动步进电机。
        motor部分作为接收机，通过ESP NOW接收来自脚控的控制信号。


************************************************************************************************************************************************************/

#include "my_analog_hat.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define DEBUG 0
#define ONFOOT 4
#define ONHAND 5
#define THROTTLE 6
#define BUZZER 7
#define MOS_STEP 10
#define TMC2209_DIRCTION 0
#define TMC2209_STEP 1
#define TMC2209_EN 2
#define WS2812_PIN 3

uint8_t contorlMode = 0;

uint8_t FootPadAddress[6];
// uint8_t crazybeans[]={};
// uint8_t moyang[]={};

// 创建ESP NOW通讯实例
esp_now_peer_info_t peerInfo;

// 发送的数据
struct Booster {
};
Booster booster;

// 接收的数据
struct FootPad {
  bool stepData[4] = {}; // 0、左转，1、右转，2、电推，3、功能
  int  stepSpeed;        // 步进电机转速
};

FootPad footPad;

/*----------------------------------------------- 自定义函数 -----------------------------------------------*/

// 数据发出去之后的回调函数
void OnDataSent(const uint8_t* mac_addr, esp_now_send_status_t status) {
  // 如果发送成功
  if (status == ESP_NOW_SEND_SUCCESS) {

  } else {
  }
}

// 收到消息后的回调
void OnDataRecv(const uint8_t* mac, const uint8_t* incomingData, int len) {
  memcpy(&footPad, incomingData, sizeof(footPad));
}

// ESP NOW
void esp_now_connect() {
  WiFi.mode(WIFI_STA); // 设置wifi为STA模式
  WiFi.begin();
  esp_now_init();                       // 初始化ESP NOW
  esp_now_register_send_cb(OnDataSent); // 注册发送成功的回调函数
  esp_now_register_recv_cb(OnDataRecv); // 注册接受数据后的回调函数

  // 注册通信频道
  memcpy(peerInfo.peer_addr, FootPadAddress, 6); // 设置配对设备的MAC地址并储存，参数为拷贝地址、拷贝对象、数据长度
  peerInfo.channel = 1;                          // 设置通信频道
  esp_now_add_peer(&peerInfo);                   // 添加通信对象

  // 如果初始化失败则重连
  while (esp_now_init() != ESP_OK) {
#if DEBUG
    Serial.println("ESP NOW 初始化失败，正在重连...");
#endif
    // 报警
    digitalWrite(BUZZER, HIGH);
    delay(1000);
    digitalWrite(BUZZER, LOW);
    delay(1000);
    // 重连
    esp_now_init();                                // 初始化ESP NOW
    esp_now_register_send_cb(OnDataSent);          // 注册发送成功的回调函数
    esp_now_register_recv_cb(OnDataRecv);          // 注册接受数据后的回调函数
    memcpy(peerInfo.peer_addr, FootPadAddress, 6); // 设置配对设备的MAC地址并储存，参数为拷贝地址、拷贝对象、数据长度
    peerInfo.channel = 1;                          // 设置通信频道
    esp_now_add_peer(&peerInfo);                   // 添加通信对象
  }
// 初始化成功，发送测试数据
#if DEBUG
  Serial.println("ESP NOW 初始化成功");
#endif
  digitalWrite(BUZZER, HIGH);
  delay(100);
  digitalWrite(BUZZER, LOW);
  delay(80);
  digitalWrite(BUZZER, HIGH);
  delay(100);
  digitalWrite(BUZZER, LOW);
  delay(80);
  digitalWrite(BUZZER, HIGH);
  delay(100);
  digitalWrite(BUZZER, LOW);
  delay(80);
}

// 蜂鸣器
void buzzer(void* pt) { }

// 模式切换
void modeChange(void* pt) {
  TickType_t       xLastWakeTime = xTaskGetTickCount();
  const TickType_t xPeriod       = pdMS_TO_TICKS(50); // 频率 20Hz → 周期为 1/20 = 0.05 秒 = 50 毫秒
  while (1) {
    if (digitalRead(ONFOOT) == HIGH && digitalRead(ONHAND) == HIGH) { // 中间档位：巡航模式
      contorlMode = 2;
      // LED指示灯
    } else if (digitalRead(ONFOOT) == HIGH && digitalRead(ONHAND) == LOW) { // 右边档位：手动模式
      contorlMode = 1;
      // LED指示灯
    } else if (digitalRead(ONFOOT) == LOW && digitalRead(ONHAND) == HIGH) { // 左边档位：脚控模式
      contorlMode = 3;
      // LED指示灯
    } else {
      contorlMode = 0;
    }
    vTaskDelayUntil(&xLastWakeTime, xPeriod);
  }
}

// 电推脚控
void motor(void* pt) {

  TickType_t       xLastWakeTime = xTaskGetTickCount();
  const TickType_t xPeriod       = pdMS_TO_TICKS(20); // 频率 50Hz → 周期为 1/50 = 0.02 秒 = 20 毫秒
  while (1) {
    int  stepSpeed = map(footPad.stepSpeed, 0, 4095, 100, 400);
    bool left      = footPad.stepData[0];
    bool right     = footPad.stepData[1];
    bool motor     = footPad.stepData[2];
    bool function  = footPad.stepData[3];
    switch (contorlMode) {
    case 0: // 待机模式
      digitalWrite(TMC2209_EN, HIGH);
      digitalWrite(MOS_STEP, LOW);
      digitalWrite(THROTTLE, LOW);
      break;
    case 3: // 脚控模式
            // 使能TMC2209，导通MOS管
      digitalWrite(TMC2209_EN, LOW);
      digitalWrite(MOS_STEP, HIGH);
      // digitalWrite(THROTTLE, LOW);
      if (motor == 0) {
        digitalWrite(THROTTLE, HIGH);
      } else {
        digitalWrite(THROTTLE, LOW);
      }
      // 左转
      if (left == 0 && right == 1 && function == 1) {
        digitalWrite(TMC2209_DIRCTION, LOW); // HIGH为顺时针，LOW为逆时针
        digitalWrite(TMC2209_STEP, HIGH);
        delayMicroseconds(stepSpeed);
        digitalWrite(TMC2209_STEP, LOW);
        delayMicroseconds(stepSpeed);
      } else if (left == 0 && right == 1 && function == 0) { // 反向
        digitalWrite(TMC2209_DIRCTION, HIGH);                // HIGH为顺时针，LOW为逆时针
        digitalWrite(TMC2209_STEP, HIGH);
        delayMicroseconds(stepSpeed);
        digitalWrite(TMC2209_STEP, LOW);
        delayMicroseconds(stepSpeed);
      }
      // 右转
      if (left == 1 && right == 0 && function == 1) {
        digitalWrite(TMC2209_DIRCTION, HIGH); // HIGH为顺时针，LOW为逆时针
        digitalWrite(TMC2209_STEP, HIGH);
        delayMicroseconds(stepSpeed);
        digitalWrite(TMC2209_STEP, LOW);
        delayMicroseconds(stepSpeed);
      } else if (left == 1 && right == 0 && function == 0) { //  反向
        digitalWrite(TMC2209_DIRCTION, LOW);                 // HIGH为顺时针，LOW为逆时针
        digitalWrite(TMC2209_STEP, HIGH);
        delayMicroseconds(stepSpeed);
        digitalWrite(TMC2209_STEP, LOW);
        delayMicroseconds(stepSpeed);
      }
      break;
    case 2: // 巡航模式
      digitalWrite(THROTTLE, HIGH);
      // 左转
      if (left == 0 && right == 1 && function == 1) {
        digitalWrite(TMC2209_DIRCTION, LOW); // HIGH为顺时针，LOW为逆时针
        digitalWrite(TMC2209_STEP, HIGH);
        delayMicroseconds(stepSpeed);
        digitalWrite(TMC2209_STEP, LOW);
        delayMicroseconds(stepSpeed);
      } else if (left == 0 && right == 1 && function == 0) { // 反向
        digitalWrite(TMC2209_DIRCTION, HIGH);                // HIGH为顺时针，LOW为逆时针
        digitalWrite(TMC2209_STEP, HIGH);
        delayMicroseconds(stepSpeed);
        digitalWrite(TMC2209_STEP, LOW);
        delayMicroseconds(stepSpeed);
      }
      // 右转
      if (left == 1 && right == 0 && function == 1) {
        digitalWrite(TMC2209_DIRCTION, HIGH); // HIGH为顺时针，LOW为逆时针
        digitalWrite(TMC2209_STEP, HIGH);
        delayMicroseconds(stepSpeed);
        digitalWrite(TMC2209_STEP, LOW);
        delayMicroseconds(stepSpeed);
      } else if (left == 1 && right == 0 && function == 0) { //  反向
        digitalWrite(TMC2209_DIRCTION, LOW);                 // HIGH为顺时针，LOW为逆时针
        digitalWrite(TMC2209_STEP, HIGH);
        delayMicroseconds(stepSpeed);
        digitalWrite(TMC2209_STEP, LOW);
        delayMicroseconds(stepSpeed);
      }
      break;
    case 1: // 手控模式
      digitalWrite(THROTTLE, HIGH);
      // 关闭步进电机控制
      digitalWrite(TMC2209_EN, HIGH);
      digitalWrite(MOS_STEP, LOW);
      break;
    default:
      break;
    }
    vTaskDelayUntil(&xLastWakeTime, xPeriod);
  }
}
/*-------------------------------------------------------------------------------------------------------*/

void setup() {
  Serial.begin(115200);
  esp_now_connect();

  pinMode(ONFOOT, INPUT_PULLUP);
  pinMode(ONHAND, INPUT_PULLUP);

  pinMode(THROTTLE, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(MOS_STEP, OUTPUT);

  pinMode(TMC2209_DIRCTION, OUTPUT);
  pinMode(TMC2209_STEP, OUTPUT);
  pinMode(TMC2209_EN, OUTPUT);

  pinMode(WS2812_PIN, OUTPUT);

  digitalWrite(THROTTLE, LOW);
  digitalWrite(MOS_STEP, LOW);
  digitalWrite(TMC2209_EN, HIGH);

  xTaskCreate(buzzer, "buzzer", 1024 * 1, NULL, 1, NULL);
  xTaskCreate(modeChange, "modeChange", 1024 * 1, NULL, 1, NULL);
  xTaskCreate(motor, "motor", 1024 * 2, NULL, 1, NULL);
}

void loop() {
}
