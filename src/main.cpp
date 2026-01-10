#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define DEBUG 0
#define TEST 0

#define MOTOR_PIN 2     // 左电机引脚
#define DIR_PIN 3       // 电机方向引脚
#define MOTOR_CHANNEL 4 // 电机PWM通道
#define RESOLUTION 12   // 电机PWM精度
#define FREQUENCY 30000 // 电机频率

/*----------------------------------------------- ESP NOW-----------------------------------------------*/

// uint8_t FootPadAddress[] = { 0x9c, 0x13, 0x9e, 0x52, 0x6e, 0x80 }; // 测试板   54:32:04:73:e1:d0
uint8_t FootPadAddress[] = { 0x08, 0xa6, 0xf7, 0x1b, 0xb2, 0xcc };

// 创建ESP NOW通讯实例
esp_now_peer_info_t peerInfo;

// 接收的数据
struct FootPad {
  bool stepData[4] = {}; // 0、左转，1、右转，2、电推，3、功能
  int  stepSpeed;        // 步进电机转速
};
FootPad footPad;

volatile bool esp_now_connected, sendSucceed, recvSucceed;
unsigned long lastRecvTime = 0;
#define RECV_TIMEOUT 500 // 接收超时时间，单位毫秒

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
  if (!recvSucceed) recvSucceed = true;
}

void MotorTask(void* parameter) {
  while (1) {
    footPad.stepData[3] ? digitalWrite(DIR_PIN, LOW) : digitalWrite(DIR_PIN, HIGH);
    if (footPad.stepData[2]) {
      ledcWrite(MOTOR_CHANNEL, footPad.stepSpeed);
    } else {
      ledcWrite(MOTOR_CHANNEL, 0);
    }
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}

/*-------------------------------------------------------------------------------------------------------*/

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_now_init();
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);
  memcpy(peerInfo.peer_addr, FootPadAddress, 6);
  peerInfo.channel = 1;
  esp_now_add_peer(&peerInfo);

  pinMode(DIR_PIN, OUTPUT);

  ledcSetup(MOTOR_CHANNEL, FREQUENCY, RESOLUTION);
  ledcAttachPin(MOTOR_PIN, MOTOR_CHANNEL);
  ledcWrite(MOTOR_CHANNEL, 0);

  xTaskCreate(MotorTask, "MotorTask", 1024 * 10, NULL, 1, NULL);
}

void loop() {
#if DEBUG
  Serial.print(" 油门: ");
  Serial.print(footPad.stepData[2]);
  Serial.print(" 转向: ");
  Serial.println(footPad.stepData[3]);
  vtaskdelay(100 / portTICK_PERIOD_MS);
#endif
}