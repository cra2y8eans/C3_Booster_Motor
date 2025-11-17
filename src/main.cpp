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
#define TEST 0

/*----------------------------------------------- ESP NOW-----------------------------------------------*/

// uint8_t FootPadAddress[] = { 0x9c, 0x13, 0x9e, 0x52, 0x6e, 0x80 }; // 测试板
uint8_t FootPadAddress[] = { 0x08, 0xa6, 0xf7, 0x1b, 0xb2, 0xcc };

// 创建ESP NOW通讯实例
esp_now_peer_info_t peerInfo;

// 模式
enum Mode {
  HAND_MODE,    // 手动模式
  FOOT_MODE,    // 脚控模式
  CRUISE_MODE,  // 巡航模式
  STANDBY_MODE, // 待机模式
};
Mode currentMode = HAND_MODE; // 默认为手动模式
Mode lastMode    = HAND_MODE;

// 发送的数据
struct Booster {
  Mode mode;
};
Booster booster;

// 接收的数据
struct FootPad {
  bool stepData[4] = {}; // 0、左转，1、右转，2、电推，3、功能
  int  stepSpeed;        // 步进电机转速
};
FootPad footPad;

volatile bool esp_now_connected, sendSucceed;
unsigned long lastRecvTime = 0;
#define RECV_TIMEOUT 500 // 接收超时时间，单位毫秒

/*----------------------------------------------- 操控模式 -----------------------------------------------*/

#define ONFOOT 8
#define ONHAND 9
#define SWITCH_DEBOUNCE_DELAY 20 // 按键消抖延时，单位毫秒

/*----------------------------------------------- 步进电机 -----------------------------------------------*/

#define MOS_STEP 5
#define TMC2209_DIRCTION 4
#define TMC2209_STEP 0
#define TMC2209_EN 10
#define TMC2209_MS1 21
#define TMC2209_MS2 2
#define TMC2209_MS3 1
#define AUTO_DISABLE_DELAY 60000 // 超时休眠，单位毫秒

unsigned long lastOperationTime;

/*----------------------------------------------- 蜂鸣器 -----------------------------------------------*/

#define BUZZER 3
#define LONG_BEEP_DURATION 1000
#define SHORT_BEEP_DURATION 200
#define LONG_BEEP_INTERVAL 300
#define SHORT_BEEP_INTERVAL 100

/*----------------------------------------------- WS2812 -----------------------------------------------*/

#define WS2812_PIN 7
#define MAX_BRIGHTNESS 255
#define MIN_BRIGHTNESS 0
#define STANDARD_BRIGHTNESS 100
#define SHORT_FLASH_DURATION 200
#define SHORT_FLASH_INTERVAL 200
#define LONG_FLASH_DURATION 500
#define LONG_FLASH_INTERVAL 500

uint16_t brightness; // 动态亮度

Adafruit_NeoPixel myRGB(1, WS2812_PIN, NEO_GRB + NEO_KHZ800);
uint32_t          red    = myRGB.Color(255, 0, 0);  // 红色
uint32_t          green  = myRGB.Color(0, 255, 0);  // 绿色
uint32_t          blue   = myRGB.Color(0, 0, 255);  // 蓝色
uint32_t          yellow = myRGB.Color(255, 40, 0); // 黄色

/*----------------------------------------------- 电机 -----------------------------------------------*/

#define THROTTLE 6

/*----------------------------------------------- 自定义函数 -----------------------------------------------*/

// 数据发出去之后的回调函数
void OnDataSent(const uint8_t* mac_addr, esp_now_send_status_t status) {
  // 如果发送成功
  if (status == ESP_NOW_SEND_SUCCESS) {
    if (!sendSucceed) sendSucceed = true;
  } else {
    sendSucceed = false;
  }
}

// 收到消息后的回调
void OnDataRecv(const uint8_t* mac, const uint8_t* incomingData, int len) {
  memcpy(&footPad, incomingData, sizeof(footPad));
  lastRecvTime = millis();
}

/**  蜂鸣器
 * @brief     蜂鸣器通用函数
 * @param     times:    鸣叫次数
 * @param     duration: 持续时间，单位毫秒
 * @param     interval: 每次鸣叫的间隔时间，单位毫秒
 */
void buzzer(uint8_t times, int duration, int interval) {
  if (times == 1) interval = 0; // 如果只鸣叫一次则不间隔
  for (int i = 0; i < times; i++) {
    digitalWrite(BUZZER, HIGH);
    vTaskDelay(duration / portTICK_PERIOD_MS);
    digitalWrite(BUZZER, LOW);
    vTaskDelay(interval / portTICK_PERIOD_MS);
  }
}

/**  指示灯
 * @brief     RGB通用函数
 * @param     times:    闪烁次数
 * @param     duration: 持续时间，单位毫秒
 * @param     interval: 每次闪烁的间隔时间，单位毫秒
 * @param     color:    颜色值
 */
void rgbBlink(uint8_t times, int duration, int interval, uint32_t color) {
  if (times == 1) interval = 0; // 如果只闪烁一次则不间隔
  for (int i = 0; i < times; i++) {
    myRGB.clear();
    myRGB.setPixelColor(0, color); // led编号和颜色，编号从0开始。
    myRGB.show();
    vTaskDelay(duration / portTICK_PERIOD_MS);
    myRGB.clear();
    myRGB.show();
    vTaskDelay(interval / portTICK_PERIOD_MS);
  }
}

// ESP NOW
void esp_now_connect() {
  WiFi.mode(WIFI_STA); // 设置wifi为STA模式
  WiFi.begin();
  if (esp_now_init() == ESP_OK) {
    // 初始化成功
    esp_now_register_send_cb(OnDataSent); // 注册发送成功的回调函数
    esp_now_register_recv_cb(OnDataRecv); // 注册接受数据后的回调函数
    // 注册通信频道
    memcpy(peerInfo.peer_addr, FootPadAddress, 6); // 设置配对设备的MAC地址并储存，参数为拷贝地址、拷贝对象、数据长度
    peerInfo.channel = 1;                          // 设置通信频道
    esp_now_add_peer(&peerInfo);                   // 添加通信对象
    // 指示灯提示
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
    // 报警
    myRGB.clear();
    myRGB.setPixelColor(0, red);
    myRGB.show();
    // 尝试重试3次
    bool reconnect_3_times = false;
    while (!reconnect_3_times) {
      for (int i = 0; i < 3; i++) {
        buzzer(1, LONG_BEEP_DURATION, LONG_BEEP_INTERVAL);
        esp_now_init();                                // 初始化ESP NOW
        esp_now_register_send_cb(OnDataSent);          // 注册发送成功的回调函数
        esp_now_register_recv_cb(OnDataRecv);          // 注册接受数据后的回调函数
        memcpy(peerInfo.peer_addr, FootPadAddress, 6); // 设置配对设备的MAC地址并储存，参数为拷贝地址、拷贝对象、数据长度
        peerInfo.channel = 1;                          // 设置通信频道
        esp_now_add_peer(&peerInfo);                   // 添加通信对象
        vTaskDelay(5000 / portTICK_PERIOD_MS);         // 延时5秒
      }
      // 如果3次重试都失败，则退出循环
      reconnect_3_times = true;
      esp_now_connected = false;
    }
  }
}

// 消抖读取当前模式
Mode readCurrentModeWithDebounce() {
  int readHand_1 = digitalRead(ONHAND);
  int readFoot_1 = digitalRead(ONFOOT);
  vTaskDelay(SWITCH_DEBOUNCE_DELAY / portTICK_PERIOD_MS); // 延时20ms，消抖
  int readHand_2 = digitalRead(ONHAND);
  int readFoot_2 = digitalRead(ONFOOT);

  if (readHand_1 != readHand_2 || readFoot_1 != readFoot_2) return lastMode; // 如果两次读取的值不一样，说明有抖动，返回上次的模式
  if (readHand_1 == LOW && readFoot_1 == HIGH) return HAND_MODE;             // 手控模式
  if (readHand_1 == HIGH && readFoot_1 == LOW) return FOOT_MODE;             // 脚控模式
  if (readHand_1 == HIGH && readFoot_1 == HIGH) return CRUISE_MODE;          // 巡航模式

  return HAND_MODE; // 默认返回手动模式
}

/** 模式切换
 * @brief    模式切换相关硬件的操作
 * @param    newMode: 切换到的新模式
 */
void modeChangeOperation(Mode newMode) {
  myRGB.clear();
  switch (newMode) {
  case HAND_MODE:
    myRGB.setPixelColor(0, green);
    buzzer(1, SHORT_BEEP_DURATION, SHORT_BEEP_INTERVAL);
    break;
  case FOOT_MODE:
    myRGB.setPixelColor(0, blue);
    buzzer(1, SHORT_BEEP_DURATION, SHORT_BEEP_INTERVAL);
    break;
  case CRUISE_MODE:
    myRGB.setPixelColor(0, yellow);
    buzzer(1, SHORT_BEEP_DURATION, SHORT_BEEP_INTERVAL);
    break;
  case STANDBY_MODE:
    myRGB.setPixelColor(0, red);
    buzzer(3, SHORT_BEEP_DURATION, SHORT_BEEP_INTERVAL);
    break;
  default:
    break;
  }
  myRGB.show();
}

// 模式更新和发送
void modeChange(void* pvParameter) {
  while (1) {
    // 如果ESP NOW连接正常，则读取当前模式，否则返回待机模式
    if (esp_now_connected) {
      currentMode = readCurrentModeWithDebounce();
    } else {
      currentMode = STANDBY_MODE; // 如果断线，返回待机模式
    }
    // 如果当前模式和上次模式不一样，则更新模式和执行模式切换操作
    if (currentMode != lastMode) {
      booster.mode = currentMode; // 更新模式
      modeChangeOperation(currentMode);
      lastMode = currentMode;
    }
    esp_now_send(FootPadAddress, (uint8_t*)&booster, sizeof(booster)); // 发送模式数据给脚控
    vTaskDelay(25 / portTICK_PERIOD_MS);                               // 已有35ms的debounce时间，如果模式没有变化，则再延时5ms，将频率设定在25hz左右，避免CPU占用过高
  }
}

// esp now连接监测任务
void esp_now_connection(void* pvParameter) {
  while (1) {
    unsigned long currentTime = millis();
    esp_now_connected         = (currentTime - lastRecvTime <= RECV_TIMEOUT);
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

// 电推脚控
void motor(void* pvParameter) {
  while (1) {
    int                  stepSpeed  = map(footPad.stepSpeed, 0, 4095, 3000, 600);
    bool                 turnLeft   = footPad.stepData[0];
    bool                 turnRight  = footPad.stepData[1];
    bool                 motor      = footPad.stepData[2];
    bool                 dirReverse = footPad.stepData[3]; // 反向
    static unsigned long lastOperationTime;
    switch (currentMode) {
    // 脚控模式,使能TMC2209，导通MOS管
    case FOOT_MODE:
      digitalWrite(THROTTLE, motor ? LOW : HIGH); // 输入上拉，踩油门输出低电平
      // 左转
      if (!turnLeft && turnRight) {
        digitalWrite(MOS_STEP, HIGH);
        digitalWrite(TMC2209_EN, LOW);
        lastOperationTime = millis();
        digitalWrite(TMC2209_DIRCTION, dirReverse ? LOW : HIGH); // 高电平顺时针旋转，大齿则逆时针旋转
        // digitalWrite(TMC2209_DIRCTION, HIGH);
        digitalWrite(TMC2209_STEP, HIGH);
        delayMicroseconds(stepSpeed);
        digitalWrite(TMC2209_STEP, LOW);
        delayMicroseconds(stepSpeed);
      } else
        // 右转
        if (turnLeft && !turnRight) {
          digitalWrite(MOS_STEP, HIGH);
          digitalWrite(TMC2209_EN, LOW);
          lastOperationTime = millis();
          digitalWrite(TMC2209_DIRCTION, dirReverse ? HIGH : LOW); // 低电平逆时针旋转，大齿则顺时针旋转
          // digitalWrite(TMC2209_DIRCTION, LOW);
          digitalWrite(TMC2209_STEP, HIGH);
          delayMicroseconds(stepSpeed);
          digitalWrite(TMC2209_STEP, LOW);
          delayMicroseconds(stepSpeed);
        }
      if (millis() - lastOperationTime > AUTO_DISABLE_DELAY) {
        digitalWrite(TMC2209_EN, HIGH);
        digitalWrite(MOS_STEP, LOW);
      }
      break;

    // 巡航模式
    case CRUISE_MODE:
      digitalWrite(THROTTLE, HIGH);
      // 左转
      if (!turnLeft && turnRight) {
        digitalWrite(MOS_STEP, HIGH);
        digitalWrite(TMC2209_EN, LOW);
        lastOperationTime = millis();
        digitalWrite(TMC2209_DIRCTION, dirReverse ? LOW : HIGH); // 高电平顺时针旋转，大齿则逆时针旋转
        // digitalWrite(TMC2209_DIRCTION, HIGH);
        digitalWrite(TMC2209_STEP, HIGH);
        delayMicroseconds(stepSpeed);
        digitalWrite(TMC2209_STEP, LOW);
        delayMicroseconds(stepSpeed);
      } else
        // 右转
        if (turnLeft && !turnRight) {
          digitalWrite(MOS_STEP, HIGH);
          digitalWrite(TMC2209_EN, LOW);
          lastOperationTime = millis();
          digitalWrite(TMC2209_DIRCTION, dirReverse ? HIGH : LOW); // 低电平逆时针旋转，大齿则顺时针旋转
          // digitalWrite(TMC2209_DIRCTION, LOW);
          digitalWrite(TMC2209_STEP, HIGH);
          delayMicroseconds(stepSpeed);
          digitalWrite(TMC2209_STEP, LOW);
          delayMicroseconds(stepSpeed);
        }
      if (millis() - lastOperationTime > AUTO_DISABLE_DELAY) {
        digitalWrite(TMC2209_EN, HIGH);
        digitalWrite(MOS_STEP, LOW);
      }
      break;

    // 手控模式
    case HAND_MODE:
      digitalWrite(THROTTLE, HIGH);   // 电机常开
      digitalWrite(TMC2209_EN, HIGH); // 关闭步进电机控制板
      digitalWrite(MOS_STEP, LOW);    // 关闭步进电机电源
      break;

    // 待机模式
    case STANDBY_MODE:
      digitalWrite(THROTTLE, HIGH);   // 电机常开
      digitalWrite(TMC2209_EN, HIGH); // 关闭步进电机控制
      digitalWrite(MOS_STEP, LOW);
      break;
    default:
      break;
    }
  }
}

/*-------------------------------------------------------------------------------------------------------*/

void setup() {
  Serial.begin(115200);

  pinMode(ONFOOT, INPUT); // 上电毛刺，外部上拉
  pinMode(ONHAND, INPUT); // boot引脚，外部上拉

  pinMode(THROTTLE, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(MOS_STEP, OUTPUT);

  pinMode(TMC2209_DIRCTION, OUTPUT);
  pinMode(TMC2209_STEP, OUTPUT);
  pinMode(TMC2209_EN, OUTPUT);
  pinMode(TMC2209_MS1, OUTPUT);
  pinMode(TMC2209_MS2, OUTPUT);
  pinMode(TMC2209_MS3, OUTPUT);

  pinMode(WS2812_PIN, OUTPUT);

  digitalWrite(THROTTLE, LOW);
  digitalWrite(MOS_STEP, LOW);
  digitalWrite(TMC2209_EN, HIGH);

  /*
    MS1  MS2  MS3      步进模式
     0   0   0          全步进
     1   0   0          半步进
     0   1   0          1/4微步
     1   1   0          1/8微步
     1   1   1          1/16微步
  */
  digitalWrite(TMC2209_MS1, 0);
  digitalWrite(TMC2209_MS2, 0);
  digitalWrite(TMC2209_MS3, 0);

  myRGB.begin();
  myRGB.setBrightness(STANDARD_BRIGHTNESS);
  myRGB.clear();
  esp_now_connect();
  lastMode = readCurrentModeWithDebounce();
  modeChangeOperation(lastMode); // 上电时根据按键状态设置初始模式和灯光

  xTaskCreate(modeChange, "modeChange", 1024 * 3, NULL, 1, NULL);
  xTaskCreate(motor, "motor", 1024 * 3, NULL, 1, NULL);
  xTaskCreate(esp_now_connection, "esp_now_connection", 1024 * 1, NULL, 1, NULL);
}

void loop() {
}