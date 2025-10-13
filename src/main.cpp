/***********************************************************************************************************************************************************

ESP32_C3 使用TMC2209驱动42步进电机实现电推脚控  分支：main

        使用TMC2209驱动板驱动步进电机。
        motor部分作为接收机，通过ESP NOW接收来自脚控的控制信号。


************************************************************************************************************************************************************/

#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define DEBUG 0

/*----------------------------------------------- ESP NOW-----------------------------------------------*/

uint8_t FootPadAddress[6];

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

bool esp_now_connected;

/*----------------------------------------------- 操控模式 -----------------------------------------------*/

#define ONFOOT 4
#define ONHAND 5

enum Mode {
  HAND_MODE,
  FOOT_MODE,
  CRUISE_MODE,
};
Mode mode;

/*----------------------------------------------- 步进电机 -----------------------------------------------*/

#define MOS_STEP 10
#define TMC2209_DIRCTION 0
#define TMC2209_STEP 1
#define TMC2209_EN 2

/*----------------------------------------------- 蜂鸣器 -----------------------------------------------*/

#define BUZZER 7
#define LONG_BEEP_DURATION 1000
#define SHORT_BEEP_DURATION 200
#define LONG_BEEP_INTERVAL 300
#define SHORT_BEEP_INTERVAL 100

/*----------------------------------------------- WS2812 -----------------------------------------------*/

#define WS2812_PIN 3

Adafruit_NeoPixel myRGB(1, WS2812_PIN, NEO_GRB + NEO_KHZ800);

uint32_t red    = myRGB.Color(255, 0, 0);   // 红色
uint32_t green  = myRGB.Color(0, 255, 0);   // 绿色
uint32_t blue   = myRGB.Color(0, 0, 255);   // 蓝色
uint32_t yellow = myRGB.Color(255, 255, 0); // 黄色

/*----------------------------------------------- 电机 -----------------------------------------------*/

#define THROTTLE 6

/*----------------------------------------------- 自定义函数 -----------------------------------------------*/

// 数据发出去之后的回调函数
void OnDataSent(const uint8_t* mac_addr, esp_now_send_status_t status) {
  // 如果发送成功
  if (status == ESP_NOW_SEND_SUCCESS) {
    esp_now_connected = true;
#if DEBUG
    Serial.println("数据发送成功");
#endif
  } else {
    esp_now_connected = false;
#if DEBUG
    Serial.println("数据发送失败");
#endif
  }
}

// 收到消息后的回调
void OnDataRecv(const uint8_t* mac, const uint8_t* incomingData, int len) {
  memcpy(&footPad, incomingData, sizeof(footPad));
}

/**  蜂鸣器
 * @brief     蜂鸣器通用函数
 * @param     times: 鸣叫次数
 * @param     duration: 持续时间，单位毫秒
 * @param     reverse: 每次鸣叫的间隔时间，单位毫秒
 */
void buzzer(uint8_t times, int duration, int interval) {
  for (int i = 0; i < times; i++) {
    digitalWrite(BUZZER, HIGH);
    vTaskDelay(duration / portTICK_PERIOD_MS);
    digitalWrite(BUZZER, LOW);
    vTaskDelay(interval / portTICK_PERIOD_MS);
  }
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
    myRGB.clear();
    myRGB.setPixelColor(0, red);
    myRGB.show();
    buzzer(3, LONG_BEEP_DURATION, LONG_BEEP_INTERVAL);
    // 重连
    esp_now_init();                                // 初始化ESP NOW
    esp_now_register_send_cb(OnDataSent);          // 注册发送成功的回调函数
    esp_now_register_recv_cb(OnDataRecv);          // 注册接受数据后的回调函数
    memcpy(peerInfo.peer_addr, FootPadAddress, 6); // 设置配对设备的MAC地址并储存，参数为拷贝地址、拷贝对象、数据长度
    peerInfo.channel = 1;                          // 设置通信频道
    esp_now_add_peer(&peerInfo);                   // 添加通信对象

    myRGB.clear();
    vTaskDelay(3000 / portTICK_PERIOD_MS);
  }
// 初始化成功
#if DEBUG
  Serial.println("ESP NOW 初始化成功");
#endif
  myRGB.clear();
  myRGB.setPixelColor(0, red);
  myRGB.show();
  vTaskDelay(500 / portTICK_PERIOD_MS);
  myRGB.clear();
  myRGB.setPixelColor(0, green);
  myRGB.show();
  vTaskDelay(500 / portTICK_PERIOD_MS);
  myRGB.clear();
  myRGB.setPixelColor(0, blue);
  myRGB.show();
  vTaskDelay(500 / portTICK_PERIOD_MS);
  myRGB.clear();
}

//  模式切换判断
void modeChange(void* pt) {
  TickType_t       xLastWakeTime = xTaskGetTickCount();
  const TickType_t xPeriod       = pdMS_TO_TICKS(50); // 频率 20Hz → 周期为 1/20 = 0.05 秒 = 50 毫秒
  while (1) {
    bool statusHand = digitalRead(ONHAND);
    bool statusFoot = digitalRead(ONFOOT);
    // 两个都是高电平，开关在中间档，巡航模式
    if (statusHand == HIGH && statusFoot == HIGH) {   // 如果引脚电平符合巡航模式
      vTaskDelay(20 / portTICK_PERIOD_MS);            // 延时20ms，消抖
      if (statusHand == HIGH && statusFoot == HIGH) { // 再次确认引脚电平符合续航模式
        mode = CRUISE_MODE;
        myRGB.clear();
        myRGB.setPixelColor(0, yellow);
        myRGB.show();
        buzzer(1, SHORT_BEEP_DURATION, SHORT_BEEP_DURATION);
      }
    }
    // 手动模式
    if (statusHand == HIGH && statusFoot == LOW) {
      vTaskDelay(20 / portTICK_PERIOD_MS);
      if (statusHand == HIGH && statusFoot == LOW) {
        mode = HAND_MODE;
        myRGB.clear();
        myRGB.setPixelColor(0, green);
        myRGB.show();
        buzzer(1, SHORT_BEEP_DURATION, SHORT_BEEP_DURATION);
      }
    }
    // 脚控模式
    if (statusHand == LOW && statusFoot == HIGH) {
      vTaskDelay(20 / portTICK_PERIOD_MS);
      if (statusHand == LOW && statusFoot == HIGH) {
        mode = FOOT_MODE;
        myRGB.clear();
        myRGB.setPixelColor(0, blue);
        myRGB.show();
        buzzer(1, SHORT_BEEP_DURATION, SHORT_BEEP_DURATION);
      }
    }
    vTaskDelayUntil(&xLastWakeTime, xPeriod);
  }
}

// 电推脚控
void motor(void* pt) {

  TickType_t       xLastWakeTime = xTaskGetTickCount();
  const TickType_t xPeriod       = pdMS_TO_TICKS(12.5); // 频率 80Hz → 周期为 1/80 = 0.0125 秒 = 12.5 毫秒
  while (1) {
    int  stepSpeed = map(footPad.stepSpeed, 0, 4095, 100, 400);
    bool left      = footPad.stepData[0];
    bool right     = footPad.stepData[1];
    bool motor     = footPad.stepData[2];
    bool reverse   = footPad.stepData[3]; // 反向

    switch (mode) {
    // 脚控模式,使能TMC2209，导通MOS管
    case FOOT_MODE:
      digitalWrite(THROTTLE, motor ? LOW : HIGH);
      digitalWrite(TMC2209_EN, LOW);
      digitalWrite(MOS_STEP, HIGH);

      if (left == 0 && right == 1) {                          // 左转按钮按下
        digitalWrite(TMC2209_DIRCTION, reverse ? HIGH : LOW); // 反向为真，左转变右转，输出高电平，顺时针旋转。反之输出低电平，逆时针旋转
        digitalWrite(TMC2209_STEP, HIGH);
        delayMicroseconds(stepSpeed);
        digitalWrite(TMC2209_STEP, LOW);
        delayMicroseconds(stepSpeed);
      }

      if (left == 1 && right == 0) {                          // 右转按钮按下
        digitalWrite(TMC2209_DIRCTION, reverse ? LOW : HIGH); // 反向为假，右转变左转，输出低电平，逆时针旋转。反之输出高电平，顺时针旋转
        digitalWrite(TMC2209_STEP, HIGH);
        delayMicroseconds(stepSpeed);
        digitalWrite(TMC2209_STEP, LOW);
        delayMicroseconds(stepSpeed);
      }
      break;

      // 巡航模式
    case CRUISE_MODE:
      digitalWrite(THROTTLE, HIGH);
      digitalWrite(TMC2209_EN, LOW);
      digitalWrite(MOS_STEP, HIGH);

      if (left == 0 && right == 1) {                          // 左转按钮按下
        digitalWrite(TMC2209_DIRCTION, reverse ? HIGH : LOW); // 反向为真，左转变右转，输出高电平，顺时针旋转。反之输出低电平，逆时针旋转
        digitalWrite(TMC2209_STEP, HIGH);
        delayMicroseconds(stepSpeed);
        digitalWrite(TMC2209_STEP, LOW);
        delayMicroseconds(stepSpeed);
      }

      if (left == 1 && right == 0) {                          // 右转按钮按下
        digitalWrite(TMC2209_DIRCTION, reverse ? LOW : HIGH); // 反向为假，右转变左转，输出低电平，逆时针旋转。反之输出高电平，顺时针旋转
        digitalWrite(TMC2209_STEP, HIGH);
        delayMicroseconds(stepSpeed);
        digitalWrite(TMC2209_STEP, LOW);
        delayMicroseconds(stepSpeed);
      }
      break;

      // 手控模式
    case HAND_MODE:
      // 关闭步进电机控制
      digitalWrite(TMC2209_EN, HIGH);
      digitalWrite(MOS_STEP, LOW);
      // 电机常开
      digitalWrite(THROTTLE, HIGH);
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

  myRGB.begin();
  myRGB.setBrightness(100);
  myRGB.clear();
  esp_now_connect();

  xTaskCreate(modeChange, "modeChange", 1024 * 1, NULL, 1, NULL);
  xTaskCreate(motor, "motor", 1024 * 2, NULL, 1, NULL);
}

void loop() {
}
