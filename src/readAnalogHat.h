#ifndef READ_ANALOG_HAT_H
#define READ_ANALOG_HAT_H

#define LOW_PASS_FILTER_ALPHA 0.1f
#define DEADZONE_CENTER 350
#define DEADZONE_END 100

/**
 * @param pin ADC引脚
 * @param raw_value 原始值（滤波历史值用于参考）
 * @return 返回值范围：-255 ~ 255
 */
int radAnalogHat(const char pin, float *raw_value);

#endif
