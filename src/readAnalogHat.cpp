#include "Arduino.h"
#include "readAnalogHat.h"

#define ADC_MAX_VALUE 4096
#define ADC_MIN_VALUE 0
#define ADC_MID_VALUE ADC_MAX_VALUE / 2
#define ADC_MAX_OUTPUT 255

float lowPassFilter(float currentValue, float previousValue)
{
    float alpha = LOW_PASS_FILTER_ALPHA;
    return alpha * currentValue + (1 - alpha) * previousValue;
}

short analogHatFilter(uint16_t rawValue)
{
    const int mid = ADC_MID_VALUE;              // 摇杆中间原始值
    const int deadzoneCenter = DEADZONE_CENTER; // 中间虚位死区阈值
    const int deadzoneEnd = DEADZONE_END;       // 两端死区阈值
    const int maxOutput = ADC_MAX_OUTPUT;       // 目标最大输出值

    int offset = (int)rawValue - mid;

    // 中间死区过滤：中间虚位部分直接返回0
    if (abs(offset) <= deadzoneCenter)
        return 0;

    // 两端死区过滤：两端死区范围内直接返回最大值
    if (rawValue <= ADC_MIN_VALUE + deadzoneEnd)
        return -maxOutput;
    if (rawValue >= ADC_MAX_VALUE - deadzoneEnd)
        return maxOutput;

    int direction = (offset > 0) ? 1 : -1;              // 确定方向
    int effectiveOffset = abs(offset) - deadzoneCenter; // 实际偏移
    int maxEffectiveOffset;                             // 有效最大偏移量
    maxEffectiveOffset = direction == 1
                             ? ADC_MAX_VALUE - (mid + deadzoneCenter) // RIGHT
                             : mid - deadzoneCenter;                  // LEFT

    /** 缩放范围&限制输出范围 */
    int scaledValue = (effectiveOffset * maxOutput) / maxEffectiveOffset;
    scaledValue = (scaledValue > maxOutput) ? maxOutput : scaledValue;
    return uint16_t(direction * scaledValue);
}

int radAnalogHat(const char pin, float *raw_value)
{
    *raw_value = lowPassFilter(analogRead(pin), *raw_value);
    return analogHatFilter(*raw_value);
}
