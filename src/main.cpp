/***********************************************************************************************************************************************************

ESP32_C3 使用TMC2209驱动42步进电机实现电推脚控  分支：main

        使用TMC2209驱动板驱动步进电机。
        motor部分作为接收机，通过ESP NOW接收来自脚控的控制信号。


************************************************************************************************************************************************************/

#include "my_analog_hat.h"
#include "my_button.hpp"
#include <AccelStepper.h>
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define buttonPinLeft 22
#define buttonPinRight 23
#define TMC2209enPin 32
#define TMC2209dirPin 12
#define TMC2209stepPin 14

#define PULSE_WIDTH 5
#define MOTOR_SPEED_PIN 3

int buttonArr[] = { buttonPinLeft, buttonPinRight };

bool isPressedLeft, isPressedRight;

PressedButton myButtons(buttonArr, sizeof(buttonArr), 0);

void generateStepPulse() {
  int val       = analogRead(MOTOR_SPEED_PIN);
  int stepDelay = map(val, 0, 4095, 500, 1000);
  digitalWrite(TMC2209stepPin, HIGH);
  delayMicroseconds(PULSE_WIDTH);
  digitalWrite(TMC2209stepPin, LOW);
  delayMicroseconds(stepDelay);
}

void stepper() {
  isPressedLeft  = (digitalRead(buttonPinLeft) == HIGH);
  isPressedRight = (digitalRead(buttonPinRight) == HIGH);

  if (isPressedLeft && !isPressedRight) {
    digitalWrite(TMC2209dirPin, LOW);
    generateStepPulse();
  } else if (!isPressedLeft && isPressedRight) {
    digitalWrite(TMC2209dirPin, HIGH);
    generateStepPulse();
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(TMC2209enPin, OUTPUT);
  pinMode(TMC2209dirPin, OUTPUT);
  pinMode(TMC2209stepPin, OUTPUT);

  digitalWrite(TMC2209enPin, LOW);
  digitalWrite(TMC2209dirPin, LOW);
  digitalWrite(TMC2209stepPin, LOW);

  myButtons.ButtonInit();
}

void loop() {
  stepper();
}
