#pragma once
#include <stdint.h>
/* Bounded restoring square root, including zero and full-scale IMU input. */
static inline uint32_t sensor_isqrt(uint32_t value){
    uint32_t result=0,bit=1u<<30;
    while(bit>value)bit>>=2;
    while(bit){if(value>=result+bit){value-=result+bit;result=(result>>1)+bit;}else result>>=1;bit>>=2;}
    return result;
}
static inline uint16_t sensor_magnitude(int16_t x,int16_t y,int16_t z){
    uint32_t square=(uint32_t)((int32_t)x*x)+(uint32_t)((int32_t)y*y)+(uint32_t)((int32_t)z*z);
    return (uint16_t)(sensor_isqrt(square)/256);
}
