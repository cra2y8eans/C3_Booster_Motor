#include "Arduino.h"
#include "my_analog_hat.h"
#include "readAnalogHat.h"

static const TickType_t xFrequency = 1; // 采样率1000HZ

//  -1 表示没有引脚
int16_t pins[6] = {
    0,  // 左摇杆水平轴
    1,  // 左摇杆垂直轴
    -1, // 左摇杆按下
    -1, // 右摇杆水平轴
    -1, // 右摇杆垂直轴
    -1  // 右摇杆按下
}; // 引脚数组

int raw_value[6]; // 原始值数组

int16_t fillter_value[6]; // 滤波值数组

void taskUpdateAnalogHat(void *arg)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();

    while (1)
    {
        for (int i = 0; i < 6; i++)
            if (pins[i] != -1)
                fillter_value[i] = radAnalogHat(pins[i], (float *)&raw_value[i]);
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

void setupAnalogHat()
{
    for (int i = 0; i < 6; i++)
        if (pins[i] != -1)
        {
            pinMode(pins[i], INPUT_PULLDOWN);
            analogSetPinAttenuation(pins[i], ADC_11db);
        }

    // 创建任务
    auto ret = xTaskCreate(taskUpdateAnalogHat, "taskUpdateAnalogHat", 1024, NULL, 1, NULL);
    ESP_ERROR_CHECK(ret != pdPASS ? ESP_FAIL : ESP_OK); // 任务创建失败
}

int16_t getAnalogHat(joyAxis_t axis)
{
    if (axis < 0 || axis >= 6)
        return 0;
    return fillter_value[axis];
}
