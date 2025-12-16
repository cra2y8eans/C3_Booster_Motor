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

// // 连接状态
// enum EspNowConnectionState {
//   INITIALIZING,          // 初始化中
//   INITIALIZED,           // 初始化成功
//   INITIALIZATION_FAILED, // 初始化失败
//   ADD_PEER_SUCCESS,      // 添加peer成功
//   ADD_PEER_FAILED,       // 添加peer失败
//   SEND_FAILED,           // 数据发送失败
//   DISCONNECTED,          // 断开连接
//   CONNECTED              // 连接成功
// };
// EspNowConnectionState connectionState = INITIALIZING;

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
#define ONHAND 5
#define SWITCH_DEBOUNCE_DELAY 20 // 按键消抖延时，单位毫秒

/*----------------------------------------------- 步进电机 -----------------------------------------------*/

#define TMC2209_DIRECTION 4
#define TMC2209_STEP 0
#define TMC2209_EN 10
#define TMC2209_MS1 21
#define TMC2209_MS2 2
#define TMC2209_MS3 1
#define AUTO_DISABLE_DELAY 60000 // 超时休眠，单位毫秒

unsigned long lastOperationTime = 0;
volatile bool isSleeped         = true; // 电机是否休眠；默认休眠。休眠时TMC2209_EN为高电平

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
int               red    = myRGB.Color(255, 0, 0);  // 红色
int               green  = myRGB.Color(0, 255, 0);  // 绿色
int               blue   = myRGB.Color(0, 0, 255);  // 蓝色
int               yellow = myRGB.Color(255, 40, 0); // 黄色

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
 * @brief     适用于单个颜色闪烁
 * @param     times:    闪烁次数
 * @param     duration: 持续时间，单位毫秒
 * @param     interval: 每次闪烁的间隔时间，单位毫秒
 * @param     color:    颜色值
 */
void rgbBlink(int times, int duration, int interval, int color) {
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
// 多色闪烁
void mutipleColorBlink(int colors[], int colorNum, int duration, int interval) {
  for (int i = 0; i < colorNum; i++) {
    myRGB.clear();
    myRGB.setPixelColor(0, colors[i]);
    myRGB.show();
    vTaskDelay(duration / portTICK_PERIOD_MS);
    myRGB.clear();
    myRGB.show();
    vTaskDelay(interval / portTICK_PERIOD_MS);
  }
}

/** ESP NOW 初始化
 * @brief     ESP NOW初始化函数
 * @return    返回值：0-连接和发送成功，1-初始化失败，2-添加peer失败，3-发送失败
 */
uint8_t esp_now_initialization() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW初始化失败");
    return 1;
  }
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);
  memcpy(peerInfo.peer_addr, FootPadAddress, 6);
  peerInfo.channel = 1;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("添加ESP-NOW peer失败");
    return 2;
  }
  if (esp_now_send(FootPadAddress, (uint8_t*)&footPad, sizeof(footPad)) == ESP_OK) {
    Serial.println("ESP-NOW发送成功");
    return 0;
  } else {
    Serial.println("ESP-NOW发送失败");
    return 3;
  }
}

/** 重试发送函数
 * @brief     重试发送ESP-NOW数据
 * @param     colors: 闪烁颜色数组
 * @return    true-成功，false-失败
 */
bool retry_esp_now_send(int colors[], int colorNum) {
  for (int i = 0; i < 60; i++) {
    // 状态指示：红灯闪烁表示正在尝试连接
    myRGB.clear();
    myRGB.setPixelColor(0, red);
    myRGB.show();
    vTaskDelay(500 / portTICK_PERIOD_MS);
    myRGB.clear();
    myRGB.show();
    vTaskDelay(500 / portTICK_PERIOD_MS);
    if (esp_now_send(FootPadAddress, (uint8_t*)&footPad, sizeof(footPad)) == ESP_OK) {
      // 成功处理
      esp_now_connected = true;
      sendSucceed       = true;
      mutipleColorBlink(colors, colorNum, LONG_FLASH_DURATION, LONG_FLASH_INTERVAL);
      buzzer(1, LONG_BEEP_DURATION, LONG_BEEP_INTERVAL);
      Serial.printf("第%d次重试成功\n", i + 1);
      return true;
    }
  }
  // 所有重试都失败
  esp_now_connected = false;
  sendSucceed       = false;
  buzzer(3, SHORT_BEEP_DURATION, SHORT_BEEP_INTERVAL);
  Serial.println("所有重试都失败，连接建立失败");
  return false;
}

/** 重试添加peer函数
 * @brief     重试添加ESP-NOW对等节点
 * @return    true-成功，false-失败
 */
bool retry_add_peer() {
  Serial.println("开始重试添加peer...");
  for (int i = 0; i < 60; i++) {
    if (esp_now_add_peer(&peerInfo) == ESP_OK) {
      Serial.println("\n重新添加peer成功");
      return true;
    }
    vTaskDelay(50 / portTICK_PERIOD_MS);
    Serial.print(".");
  }
  Serial.println("\n重试添加peer失败");
  return false;
}

/** 重试初始化函数
 * @brief     重试初始化ESP-NOW
 * @return    true-成功，false-失败
 */
bool retry_esp_now_init() {
  Serial.println("开始重试ESP-NOW初始化...");
  for (int i = 0; i < 60; i++) {
    if (esp_now_init() == ESP_OK) {
      // 重新注册回调函数
      esp_now_register_send_cb(OnDataSent);
      esp_now_register_recv_cb(OnDataRecv);
      // 重新配置peer信息
      memcpy(peerInfo.peer_addr, FootPadAddress, 6);
      peerInfo.channel = 1;
      Serial.println("\nESP-NOW重新初始化成功");
      return true;
    }
    vTaskDelay(50 / portTICK_PERIOD_MS);
    Serial.print(".");
  }
  Serial.println("\n重试初始化失败");
  return false;
}

// ESP NOW连接函数
void esp_now_connect() {
  int     colors[] = { red, green, blue };
  int     colorNum = 3;
  uint8_t result   = esp_now_initialization();
  switch (result) {
  case 0: // 初始化成功
    esp_now_connected = true;
    sendSucceed       = true;
    mutipleColorBlink(colors, colorNum, LONG_FLASH_DURATION, LONG_FLASH_INTERVAL);
    buzzer(1, LONG_BEEP_DURATION, LONG_BEEP_INTERVAL);
    Serial.println("ESP-NOW连接成功");
    break;
  case 1: // 初始化失败
    if (retry_esp_now_init()) {
      // 初始化成功后添加peer
      if (esp_now_add_peer(&peerInfo) == ESP_OK) {
        // 发送测试数据
        retry_esp_now_send(colors, colorNum);
      } else {
        Serial.println("重试后添加peer仍失败");
        esp_now_connected = false;
        buzzer(3, SHORT_BEEP_DURATION, SHORT_BEEP_INTERVAL);
      }
    } else {
      // 初始化重试失败
      esp_now_connected = false;
      buzzer(3, SHORT_BEEP_DURATION, SHORT_BEEP_INTERVAL);
    }
    break;
  case 2: // 添加peer失败
    if (retry_add_peer()) {
      // 添加peer成功后发送测试数据
      retry_esp_now_send(colors, colorNum);
    } else {
      // 添加peer重试失败
      esp_now_connected = false;
      buzzer(3, SHORT_BEEP_DURATION, SHORT_BEEP_INTERVAL);
    }
    break;
  case 3: // 发送失败
    retry_esp_now_send(colors, colorNum);
    break;
  default:
    break;
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
#if DEBUG
  const char* modeNames[] = { "手动模式", "脚控模式", "巡航模式", "待机模式" };
  Serial.print("模式切换函数：");
  Serial.println(modeNames[newMode]);
#endif
}

// 步进
void stepper_pulse(int stepSpeed) {
  digitalWrite(TMC2209_STEP, HIGH);
  delayMicroseconds(stepSpeed);
  digitalWrite(TMC2209_STEP, LOW);
  delayMicroseconds(stepSpeed);
}

// 统一的步进电机控制函数
void stepper_control(bool turnLeft, bool turnRight, bool dirReverse, int stepSpeed) {
  if (!turnLeft && turnRight) {
    // 右转
    if (isSleeped) digitalWrite(TMC2209_EN, LOW); // 如果电机处于休眠状态，则先唤醒
    digitalWrite(TMC2209_DIRECTION, dirReverse ? LOW : HIGH);
    lastOperationTime = millis();
    stepper_pulse(stepSpeed);
  } else if (turnLeft && !turnRight) {
    // 左转
    if (isSleeped) digitalWrite(TMC2209_EN, LOW);
    digitalWrite(TMC2209_DIRECTION, dirReverse ? HIGH : LOW);
    lastOperationTime = millis();
    stepper_pulse(stepSpeed);
  }

  // 自动休眠检查
  if (millis() - lastOperationTime > AUTO_DISABLE_DELAY) {
    digitalWrite(TMC2209_EN, HIGH);
    isSleeped = true;
#if DEBUG
    Serial.println("步进电机自动休眠");
#endif
  } else {
    isSleeped = false;
  }
}

// esp now连接监测任务
void esp_now_connection(void* pvParameter) {
  while (1) {
    unsigned long currentTime = millis();
    esp_now_connected         = (currentTime - lastRecvTime <= RECV_TIMEOUT);
#if DEBUG
    static unsigned long lastDebugTime = 0;
    if (currentTime - lastDebugTime > 2000) { // 每2秒打印一次，避免刷屏
      if (esp_now_connected && sendSucceed) {
        Serial.println("连接检测任务：ESP NOW 接收发送正常！");
      } else if (esp_now_connected && !sendSucceed) {
        Serial.println("连接检测任务：ESP NOW 接收成功，但发送异常！");
      } else if (!esp_now_connected && sendSucceed) {
        Serial.println("连接检测任务：ESP NOW 接收超时，但发送成功！");
      } else if (!esp_now_connected && !sendSucceed) {
        Serial.println("连接检测任务：ESP NOW 彻底断线！");
      }
      lastDebugTime = currentTime;
    }
#endif
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

// 模式更新和发送
void modeChange(void* pvParameter) {
  while (1) {
    // 如果ESP NOW连接正常，则读取当前模式，否则返回待机模式
    if (esp_now_connected) {
      currentMode = readCurrentModeWithDebounce();
    } else {
      currentMode = STANDBY_MODE; // 如果断线，返回待机模式
#if DEBUG
      static unsigned long lastDebugTime = 0;
      if (millis() - lastDebugTime > 2000) { // 每2秒打印一次，避免刷屏
        Serial.println("模式更新和发送：ESP NOW 断线，返回待机模式");
        lastDebugTime = millis();
      }
#endif
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

// 电推脚控
void motor(void* pvParameter) {
  while (1) {
    int  stepSpeed  = map(footPad.stepSpeed, 0, 4095, 4000, 1000);
    bool turnLeft   = footPad.stepData[0];
    bool turnRight  = footPad.stepData[1];
    bool motor      = footPad.stepData[2];
    bool dirReverse = footPad.stepData[3]; // 反向

    switch (currentMode) {
    case FOOT_MODE: // 脚控模式
      digitalWrite(THROTTLE, motor ? LOW : HIGH);
      stepper_control(turnLeft, turnRight, dirReverse, stepSpeed);
      break;
    case CRUISE_MODE: // 巡航模式
      digitalWrite(THROTTLE, HIGH);
      stepper_control(turnLeft, turnRight, dirReverse, stepSpeed);
      break;
    case HAND_MODE:
    case STANDBY_MODE: // 手控模式和待机模式
      digitalWrite(THROTTLE, HIGH);
      digitalWrite(TMC2209_EN, HIGH);
      break;
    }
    vTaskDelay(1); // 让出CPU时间
  }
}

/*-------------------------------------------------------------------------------------------------------*/

void setup() {
  Serial.begin(115200);
  pinMode(ONFOOT, INPUT);
  pinMode(ONHAND, INPUT_PULLUP);

  pinMode(THROTTLE, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  pinMode(TMC2209_DIRECTION, OUTPUT);
  pinMode(TMC2209_STEP, OUTPUT);
  pinMode(TMC2209_EN, OUTPUT);
  pinMode(TMC2209_MS1, OUTPUT);
  pinMode(TMC2209_MS2, OUTPUT);
  pinMode(TMC2209_MS3, OUTPUT);

  pinMode(WS2812_PIN, OUTPUT);

  digitalWrite(THROTTLE, LOW);
  digitalWrite(TMC2209_EN, HIGH);

  /*
    MS1  MS2  MS3      步进模式
     0   0   0          全步进
     1   0   0          半步进
     0   1   0          1/4微步
     1   1   0          1/8微步
     1   1   1          1/16微步
  */
  digitalWrite(TMC2209_MS1, 0); // 1微步
  digitalWrite(TMC2209_MS2, 0); // 1微步
  digitalWrite(TMC2209_MS3, 0); // 1微步

  myRGB.begin();
  myRGB.setBrightness(STANDARD_BRIGHTNESS);
  myRGB.clear();
  esp_now_connect();
  lastMode = readCurrentModeWithDebounce();
  modeChangeOperation(lastMode); // 上电时根据按键状态设置初始模式和灯光

  xTaskCreate(modeChange, "modeChange", 1024 * 2, NULL, 1, NULL);
  xTaskCreate(motor, "motor", 1024 * 3, NULL, 2, NULL);
  xTaskCreate(esp_now_connection, "esp_now_connection", 1024 * 1, NULL, 1, NULL);
#if DEBUG
  Serial.println("setup:电推初始化完成");
#endif
}

void loop() {
#if DEBUG
  if (esp_now_connected)
    Serial.printf("模式：%d，左转：%d，右转：%d，电推：%d，反向：%d，步进电机转速：%d\n", currentMode, footPad.stepData[0], footPad.stepData[1], footPad.stepData[2], footPad.stepData[3], footPad.stepSpeed);
  delay(500);
#endif

#if TEST
  static bool isPrinted = false;
  if (esp_now_connected && isPrinted) {
    Serial.println("连接");
    isPrinted = false;
  }
  if (!isPrinted && !esp_now_connected) {
    Serial.println("掉线");
    isPrinted = true;
  }
  delay(100);
#endif
}