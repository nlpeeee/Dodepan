#include "i2c_mutex.h"

static mutex_t i2c1_mutex;

void i2c1_mutex_init(void) {
    mutex_init(&i2c1_mutex);
}

void i2c1_mutex_enter(void) {
    mutex_enter_blocking(&i2c1_mutex);
}

bool i2c1_mutex_try_enter(void) {
    return mutex_try_enter(&i2c1_mutex, NULL);
}

void i2c1_mutex_exit(void) {
    mutex_exit(&i2c1_mutex);
}
