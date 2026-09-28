/* IMU GY-521 - MPU6050 accelerometer and gyroscope */

#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "MPU6050.h"
#include <config.h>
#include "imu.h"
#include "sensor_math.h"
#include "i2c_mutex.h"

mpu6050_t mpu6050;

// The lack of an FPU on the Pico makes it hard to process IMU data on the fly
// while also running the synth and i/o. We'll have to resort to fixed-point math.
// The default resolution of the IMU is too high for this application anyway.
// Pico 2 does have an FPU but can use the same simplified math.

#define FIXED_POINT_BITS 16
#define FIXED_POINT_SCALE (1 << FIXED_POINT_BITS)

#define PEAK_HOLD_WINDOW        10

typedef struct {
    int16_t peak;
    int16_t window[PEAK_HOLD_WINDOW];
    int8_t window_index;
    int8_t reset_value;
    int8_t reset_counter;
} peak_hold_state_t;

peak_hold_state_t peak_hold;

void peak_hold_init(peak_hold_state_t *state) {
    state->peak = 0;
    for (int i = 0; i < PEAK_HOLD_WINDOW; i++) {
        state->window[i] = 0;
    }
    state->window_index = 0;
    state->reset_value = VELOCITY_HOLD_SAMPLES;
    state->reset_counter = 0;
}

void peak_hold_update(peak_hold_state_t *state, int16_t value) {
    state->window[state->window_index] = value;
    state->window_index = (state->window_index + 1) % PEAK_HOLD_WINDOW;

    // Find maximum value in window
    int16_t max_value = state->window[0];
    for (int i = 1; i < PEAK_HOLD_WINDOW; i++) {
        if (state->window[i] > max_value) {
            max_value = state->window[i];
        }
    }

    // Update peak if new maximum is higher
    if (max_value > state->peak) {
        state->peak = max_value;
    }

    // Reset the peak after the timeout
    if (++state->reset_counter == state->reset_value) {
        peak_hold_init(state);
    }
}

int16_t peak_hold_get(peak_hold_state_t *state) {
    return state->peak;
}

void imu_init(){
    mpu6050 = mpu6050_init(MPU6050_I2C_PORT, MPU6050_ADDRESS);

    if (mpu6050_begin(&mpu6050)) {
        mpu6050_set_range(&mpu6050, MPU6050_RANGE_2G); // Accelerometer range. ±2 g = 16384 LSB/g

        // Set digital filters
        mpu6050_set_dhpf_mode(&mpu6050, MPU6050_DHPF_2_5HZ);
        mpu6050_set_dlpf_mode(&mpu6050, MPU6050_DLPF_3);

        mpu6050_set_accelerometer_measuring(&mpu6050, true);

        // We're not calibrating the IMU on Pico RP2040, and we're not fusing gyro and accelerometer data
        // to account for gravitational compensation.
    }

    peak_hold_init(&peak_hold);
}

// Overriding rpi-pico-mpu6050 library methods for minimal, fixed-point calculations
bool read_raw_accel_fixed(struct mpu6050 *self) {
    struct i2c_information *i2c = &self->i2c;
    uint8_t data[6];

    self->ra.x = 0;
    self->ra.y = 0;
    self->ra.z = 0;

    // Read raw accelerometer data
    uint8_t reg = 0x3B; // ACCEL_XOUT_H
    if(i2c_write_timeout_us(i2c->instance, i2c->address, &reg, 1, true, 3000)!=1 ||
       i2c_read_timeout_us(i2c->instance, i2c->address, data, 6, false, 3000)!=6)return false;

    self->ra.x = data[0] << 8 | data[1];
    self->ra.y = data[2] << 8 | data[3];
    self->ra.z = data[4] << 8 | data[5];
    return true;
}

int16_t abs_fixed(int16_t x) {
    return (x < 0) ? -x : x;
}

int8_t map_7(int16_t value) {
    int16_t result = (value + 128) * 127 / 256;
    return (result < 0) ? 0 : (result > 127) ? 127 : result;
}

int16_t map_14(int16_t value) {
    int32_t result = (value + 128) * 16383 / 256;
    return (result < 0) ? 0 : (result > 16383) ? 16383 : result;
}

void imu_task(Imu_data * data) {
    // Acquire I2C lock before reading IMU data
    i2c1_mutex_enter();
    bool valid=read_raw_accel_fixed(&mpu6050);
    i2c1_mutex_exit();
    static uint64_t last_good_us;
    if(!valid){
        if(time_us_64()-last_good_us>=250000){data->available=false;data->deviation_x=8192;data->deviation_y=64;data->acceleration=100;data->tilt_zone=4;}
        return;
    }
    last_good_us=time_us_64();data->available=true;
    
    struct mpu6050_vector16 *accel = &mpu6050.ra;

    // Scale the raw readings.
    // 493 = (MPU6050_SCALE_250DPS * (1 << 16)) / 1
    int16_t ax = (accel->x * 493) / FIXED_POINT_SCALE;
    int16_t ay = (accel->y * 493) / FIXED_POINT_SCALE;
    int16_t az = (accel->z * 493) / FIXED_POINT_SCALE;

    // Calculate the total motion acceleration (which is not linear since we have not accounted for gravity)
    int16_t tot_accel = (int16_t)sensor_magnitude(accel->x,accel->y,accel->z);

    static int16_t prev_tot_accel;
    int16_t delta_accel = abs_fixed(prev_tot_accel - tot_accel);
    prev_tot_accel = tot_accel;

    peak_hold_update(&peak_hold, delta_accel);

#if defined (MPU6050_FLIP_X)
    ax = -ax;
#endif
#if defined (MPU6050_FLIP_Y)
    ay = -ay;
#endif

#if defined (MPU6050_SWAP_X_Y)
    int16_t temp = ax;
    ax = ay;
    ay = temp;
#endif

    // X goes to pitch bending, Y goes to cutoff
    data->deviation_x = map_14(ax);
    data->deviation_y = map_7(LPF_MIN + ay);

    // Acceleration goes to velocity
    data->acceleration = (map_7(peak_hold_get(&peak_hold) * VELOCITY_MULTIPLIER));

    // Calculate tilt zone (3x3 grid) with hysteresis for chord voicing
    // Zone layout:  0 1 2   (top row, Y up)
    //               3 4 5   (middle row)
    //               6 7 8   (bottom row, Y down)
    static uint8_t last_zone = 4; // Start at center
    static int16_t last_ax = 0;
    static int16_t last_ay = 0;
    
    // Apply hysteresis: only change zone if movement exceeds threshold
    int16_t delta_ax = abs_fixed(ax - last_ax);
    int16_t delta_ay = abs_fixed(ay - last_ay);
    
    if (delta_ax > (TILT_HYSTERESIS >> 6) || delta_ay > (TILT_HYSTERESIS >> 6)) {
        last_ax = ax;
        last_ay = ay;
        
        // Map ax to column (0=left, 1=center, 2=right)
        // ax range is roughly -128 to 128 after scaling
        // Lower threshold (25) makes corner zones easier to reach
        uint8_t col;
        if (ax < -25) col = 0;       // Left tilt
        else if (ax > 25) col = 2;  // Right tilt
        else col = 1;                // Center
        
        // Map ay to row (0=up, 1=center, 2=down)
        uint8_t row;
        if (ay > 25) row = 0;        // Tilted up (away)
        else if (ay < -25) row = 2; // Tilted down (toward)
        else row = 1;                // Center
        
        last_zone = row * 3 + col;
    }
    
    data->tilt_zone = last_zone;
}