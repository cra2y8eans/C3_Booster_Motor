#ifndef MALT
#define MALT
#include <Arduino.h>

typedef enum
{
    joyLHori,  // 左摇杆水平轴
    joyLVert,  // 左摇杆垂直轴
    joyLPress, // 左摇杆按下

    joyRHori,  // 右摇杆水平轴
    joyRVert,  // 右摇杆垂直轴
    joyRPress, // 右摇杆按下

} joyAxis_t; // 摇杆轴向

void setupAnalogHat();

int16_t getAnalogHat(joyAxis_t axis);

#endif
