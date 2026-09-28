#ifndef IMU_H
#define IMU_H
#include "pico/stdlib.h"

#ifdef __cplusplus
extern "C" {
#endif

// Accelerometer/gyroscope data structure for Dodepan
typedef struct {
    bool available;
    uint8_t acceleration; // Also used to calculate velocity for Midi
    uint16_t deviation_x;
    uint8_t deviation_y;
    uint8_t tilt_zone;    // 0-8: 3x3 grid index for chord voicing (row*3 + col)
} Imu_data;

void imu_init();
void imu_task(Imu_data * data);

// Get current tilt zone (defined in main.cpp)
uint8_t get_tilt_zone();

#ifdef __cplusplus
}
#endif

#endif