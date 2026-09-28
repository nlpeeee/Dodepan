/* MPR121 functions */

#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "mpr121.h"         // https://github.com/antgon/pico-mpr121
#include <config.h>
#include "touch.h"

struct mpr121_sensor mpr121;

void mpr121_i2c_init(){
    i2c_init(MPR121_I2C_PORT, MPR121_I2C_FREQ);
    gpio_set_function(MPR121_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(MPR121_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(MPR121_SDA_PIN);
    gpio_pull_up(MPR121_SCL_PIN);

    mpr121_init(MPR121_I2C_PORT, MPR121_ADDRESS, &mpr121);
    mpr121_set_thresholds(MPR121_TOUCH_THRESHOLD,
                          MPR121_RELEASE_THRESHOLD, &mpr121);
    
    mpr121_enable_electrodes(12, &mpr121);
}


// Fixed-time release filtering is independent of display/USB loop load.
static uint16_t touched_mask,raw_mask;
static uint64_t last_poll_us,last_valid_us,started_us,release_since[12];
bool touch_any_held(void){return raw_mask!=0 || touched_mask!=0;}
void mpr121_task(){
    uint64_t now=time_us_64();if(now-last_poll_us<2000)return;last_poll_us=now;
    if(!started_us)started_us=now;
    uint8_t reg=0,data[2];
    bool valid=i2c_write_timeout_us(MPR121_I2C_PORT,MPR121_ADDRESS,&reg,1,true,2000)==1 &&
               i2c_read_timeout_us(MPR121_I2C_PORT,MPR121_ADDRESS,data,2,false,2000)==2;
    if(valid){raw_mask=((uint16_t)data[0]|((uint16_t)data[1]<<8))&0xfff;last_valid_us=now;}
    else if(now-last_valid_us<100000)return;
    else raw_mask=0;
    if(now-started_us<500000)return;
    for(uint8_t i=0;i<12;i++){
        uint16_t bit=1u<<i;
        if(raw_mask&bit){
            release_since[i]=0;
            if(!(touched_mask&bit)){touched_mask|=bit;touch_on(i);}
        }else if(touched_mask&bit){
            if(!release_since[i])release_since[i]=now;
            if(now-release_since[i]>=TOUCH_RELEASE_MS*1000u){touched_mask&=~bit;release_since[i]=0;touch_off(i);}
        }
    }
}
