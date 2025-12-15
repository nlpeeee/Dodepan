#ifndef I2C_MUTEX_H
#define I2C_MUTEX_H

#include "pico/stdlib.h"
#include "pico/mutex.h"

#ifdef __cplusplus
extern "C" {
#endif

// Mutex for I2C1 bus (shared by SSD1306 display and MPU6050 IMU)
// This prevents bus collisions when multiple operations try to access
// the same I2C bus simultaneously.

void i2c1_mutex_init(void);
void i2c1_mutex_enter(void);
bool i2c1_mutex_try_enter(void);  // Returns true if lock acquired, false if busy
void i2c1_mutex_exit(void);

#ifdef __cplusplus
}
#endif

#endif
