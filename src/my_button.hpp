#pragma once

#include <Arduino.h>

/*--------------------------------------------------------- 第一版 ----------------------------------------------------------------------*/

// class PressedButton {

//   private:
//   int           _debounceDelay     = 50;      // 去抖延迟
//   int           _longPressDuration = 1000;    // 长按持续时间（毫秒）
//   int           _repeatDelay       = 500;     // 连续短按持续时间（毫秒）
//   int           _button_pin;                  // 按钮引脚号
//   int buttonArray[5]={};
//   int           buttonState          = HIGH;  // 初始按键状态
//   int           lastButtonState      = HIGH;  // 上一次的按键状态
//   unsigned long lastDebounceTime     = 0;     // 上一次去抖的时间
//   unsigned long pressStartTime       = 0;     // 按键按下的起始时间
//   bool          longPressTriggered   = false; // 是否已经触发了长按
//   bool          repeatPressTriggered = false;

//   public:
//   // 变量

//   // 函数声明
//   PressedButton(int pin);
//   void ButtonInit(); // 引脚初始化
//   int  GetPin();
//   void SetPin(int pin);
//   void SetDebounceDelay(int time);
//   void SetLongPressDuration(int time);
//   void SetRepeatDelay(int time);
//   void ShortPress(void (*ptr)());
//   void LongPress(void (*ptr)());
//   void RepeatPress(void (*ptr)());
// };

// PressedButton::PressedButton(int pin) {
//   _button_pin = pin;
// }

// void PressedButton::ButtonInit() {
//   pinMode(_button_pin, INPUT_PULLUP);
// }

// int PressedButton::GetPin() {
//   return _button_pin;
// }

// void PressedButton::SetPin(int pin) {
//   _button_pin = pin;
// }

// void PressedButton::SetDebounceDelay(int time) {
//   _debounceDelay = time;
// }

// void PressedButton::SetLongPressDuration(int time) {
//   _longPressDuration = time;
// }

// void PressedButton::SetRepeatDelay(int time) {
//   _repeatDelay = time;
// }

// void PressedButton::ShortPress(void (*ptr)()) {
//   int reading = digitalRead(_button_pin); // 读取按键状态
//   // 去抖处理
//   if (reading != lastButtonState) {
//     lastDebounceTime = millis();
//   }
//   if (millis() - lastDebounceTime > _debounceDelay) {
//     // 更新按键状态
//     if (reading != buttonState) {
//       buttonState = reading;
//       // 检测按键按下
//       if (buttonState == LOW) {
//         pressStartTime       = millis();
//         longPressTriggered   = false;
//         repeatPressTriggered = false;
//       } else {
//         if (!longPressTriggered && !repeatPressTriggered) {
//           // 按键释放,短按触发
//           // Serial.println("Short press");
//           ptr();
//         }
//         longPressTriggered   = false;
//         repeatPressTriggered = false;
//       }
//     }
//   }
//   lastButtonState = reading;
// }

// void PressedButton::LongPress(void (*ptr)()) {
//   int reading = digitalRead(_button_pin); // 读取按键状态
//   // 去抖处理
//   if (reading != lastButtonState) {
//     lastDebounceTime = millis();
//   }
//   if (millis() - lastDebounceTime > _debounceDelay) {
//     // 检测长按
//     if (!longPressTriggered && buttonState == LOW && millis() - pressStartTime > _longPressDuration) {
//       longPressTriggered = true;
//       // Serial.println("Long press");
//       ptr();
//     }
//     repeatPressTriggered = false;
//   }
//   lastButtonState = reading;
// }

// void PressedButton::RepeatPress(void (*ptr)()) {
//   int reading = digitalRead(_button_pin); // 读取按键状态
//   if (reading != lastButtonState) {
//     lastDebounceTime = millis();
//   }
//   if (millis() - lastDebounceTime > _debounceDelay) {
//     if (!repeatPressTriggered && buttonState == LOW && millis() - pressStartTime > _repeatDelay) {
//       repeatPressTriggered = true;
//       // Serial.println("Repeat press");
//       ptr();
//     }
//   }
//   lastButtonState = reading;
// }

/*--------------------------------------------------------- 第二版 ----------------------------------------------------------------------*/

// class PressedButton {
//   private:
//   int           _debounceDelay     = 50;      // 去抖延迟
//   int           _longPressDuration = 1000;    // 长按持续时间（毫秒）
//   int           _repeatDelay       = 500;     // 连续短按持续时间（毫秒）
//   int           _button_pin;                  // 按钮引脚号
//   int           buttonArray[10]      = {};    // 最大支持10个按钮
//   int           VoltageLevel         = 0;     // 默认引脚电平为低
//   int           buttonState          = HIGH;  // 初始按键状态
//   int           lastButtonState      = HIGH;  // 上一次的按键状态
//   unsigned long lastDebounceTime     = 0;     // 上一次去抖的时间
//   unsigned long pressStartTime       = 0;     // 按键按下的起始时间
//   bool          longPressTriggered   = false; // 是否已经触发了长按
//   bool          repeatPressTriggered = false;

//   public:
//   PressedButton(int user_input_arr[], int size, int level);
//   void ButtonInit();
//   void SetDebounceDelay(int time);
//   void SetLongPressDuration(int time);
//   void SetRepeatDelay(int time);
//   void ButtonIdentfly(void (*ptr1)(), void (*ptr2)(), void (*ptr3)());
// };

// /*
//  * 用户将按钮引脚数组作为参数传入，对buttonArray进行赋值
//  * 如用户使用的按钮数不足10个，则从-1开始递减补足剩余元素空位
//  */
// PressedButton::PressedButton(int user_input_arr[], int size, int level) {
//   int pinNum, pinNumUserInput, diffrence;
//   pinNum          = (sizeof(buttonArray) / sizeof(buttonArray[0]));
//   pinNumUserInput = (size / sizeof(user_input_arr[0]));
//   diffrence       = pinNum - pinNumUserInput;
//   for (int i = 0; i < pinNumUserInput; i++) {
//     buttonArray[i] = user_input_arr[i];
//   }
//   if (diffrence > 0) {
//     for (int i = 0; i < diffrence; i++) {
//       buttonArray[pinNumUserInput + i] = (i + 1) * -1;
//     }
//   }
//   VoltageLevel = level;
// }

// void PressedButton::ButtonInit() {
//   for (int i = 0; i < 10; i++) {
//     if (VoltageLevel == 0) {
//       pinMode(buttonArray[i], INPUT_PULLDOWN);
//     } else {
//       pinMode(buttonArray[i], INPUT_PULLUP);
//     }
//   }
// }

// void PressedButton::SetDebounceDelay(int time) {
//   _debounceDelay = time;
// }

// void PressedButton::SetLongPressDuration(int time) {
//   _longPressDuration = time;
// }

// void PressedButton::SetRepeatDelay(int time) {
//   _repeatDelay = time;
// }

// void PressedButton::ButtonIdentfly(void (*ptr_short)(), void (*ptr_long)(), void (*ptr_repeat)()) {
//   for (int i = 0; i < (sizeof(buttonArray) / (sizeof(buttonArray[0]))); i++) {
//     if (digitalRead(buttonArray[i]) == LOW) {
//       _button_pin = buttonArray[i];
//     }
//   }
//   int reading = digitalRead(_button_pin); // 读取按键状态
//   // 去抖处理
//   if (reading != lastButtonState) {
//     lastDebounceTime = millis();
//   }
//   if (millis() - lastDebounceTime > _debounceDelay) {
//     // 更新按键状态
//     if (reading != buttonState) {
//       buttonState = reading;
//       // 检测按键按下
//       if (buttonState == LOW) {
//         pressStartTime       = millis();
//         longPressTriggered   = false;
//         repeatPressTriggered = false;
//       } else { // 按键释放
//         if (!longPressTriggered && !repeatPressTriggered) {
//           // 短按触发
//           ptr_short();
//         }
//         longPressTriggered   = false;
//         repeatPressTriggered = false;
//       }
//     }
//     // 检测长按
//     if (!longPressTriggered && buttonState == LOW && millis() - pressStartTime > _longPressDuration) {
//       longPressTriggered = true;
//       ptr_long();
//     }
//     // 检测连续按下
//     if (!repeatPressTriggered && buttonState == LOW && millis() - pressStartTime > _repeatDelay) {
//       repeatPressTriggered = true;
//       ptr_repeat();
//     }
//   }
//   lastButtonState = reading;
// }

/*--------------------------------------------------------- AI改进版 ----------------------------------------------------------------------*/

class PressedButton {
  private:
  int           _debounceDelay     = 50;   // 去抖延迟
  int           _longPressDuration = 1000; // 长按持续时间（毫秒）
  int           _repeatDelay       = 500;  // 连续短按持续时间（毫秒）
  int*          buttonArray;               // 动态分配的按钮引脚数组
  int           VoltageLevel = 0;          // 默认引脚电平为低
  int           _button_pin;
  int           buttonState          = HIGH;  // 初始按键状态
  int           lastButtonState      = HIGH;  // 上一次的按键状态
  unsigned long lastDebounceTime     = 0;     // 上一次去抖的时间
  unsigned long pressStartTime       = 0;     // 按键按下的起始时间
  bool          longPressTriggered   = false; // 是否已经触发了长按
  bool          repeatPressTriggered = false;
  int           buttonCount; // 按钮数量

  public:
  PressedButton(int user_input_arr[], int size, int level);
  ~PressedButton(); // 析构函数
  void ButtonInit();
  void SetDebounceDelay(int time);
  void SetLongPressDuration(int time);
  void SetRepeatDelay(int time);
  void ButtonIdentfly(void (*ptr_short)(), void (*ptr_long)() /*,void (*ptr_repeat)()*/);
};

// 构造函数
PressedButton::PressedButton(int user_input_arr[], int size, int level) {
  buttonCount = size / sizeof(user_input_arr[0]); // 使用用户输入的按钮数量
  buttonArray = new int[buttonCount];             // 动态分配按钮数组

  for (int i = 0; i < buttonCount; i++) {
    buttonArray[i] = user_input_arr[i];
  }

  VoltageLevel = level;
}

// 析构函数
PressedButton::~PressedButton() {
  delete[] buttonArray; // 释放动态分配的内存
}

void PressedButton::ButtonInit() {
  for (int i = 0; i < buttonCount; i++) {
    if (VoltageLevel == 0) {
      pinMode(buttonArray[i], INPUT_PULLDOWN);
    } else {
      pinMode(buttonArray[i], INPUT_PULLUP);
    }
  }
}

void PressedButton::SetDebounceDelay(int time) {
  _debounceDelay = time;
}

void PressedButton::SetLongPressDuration(int time) {
  _longPressDuration = time;
}

void PressedButton::SetRepeatDelay(int time) {
  _repeatDelay = time;
}

void PressedButton::ButtonIdentfly(void (*ptr_short)(), void (*ptr_long)() /*,void (*ptr_repeat)()*/) {
  for (int i = 0; i < buttonCount; i++) {
    if (digitalRead(buttonArray[i]) != VoltageLevel) {
      _button_pin = buttonArray[i];
    }
  }
  int reading = digitalRead(_button_pin); // 读取按键状态
  // 去抖处理
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  if (millis() - lastDebounceTime > _debounceDelay) {
    // 更新按键状态
    if (reading != buttonState) {
      buttonState = reading;
      // 检测按键按下
      if (buttonState == LOW) {
        pressStartTime       = millis();
        longPressTriggered   = false;
        repeatPressTriggered = false;
      } else { // 按键释放
        if (!longPressTriggered && !repeatPressTriggered) {
          // 短按触发
          ptr_short();
        }
        longPressTriggered   = false;
        repeatPressTriggered = false;
      }
    }
    // 检测长按
    if (!longPressTriggered && buttonState == LOW && millis() - pressStartTime > _longPressDuration) {
      longPressTriggered = true;
      ptr_long();
    }
    // // 检测连续按下
    // if (!repeatPressTriggered && buttonState == LOW && millis() - pressStartTime > _repeatDelay) {
    //   repeatPressTriggered = true;
    //   ptr_repeat();
    // }
  }
  lastButtonState = reading;
}
