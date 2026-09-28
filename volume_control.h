#pragma once
#include <stdint.h>
#define VOLUME_MAX 100u
// Level 1 matches the previous curve at20 (123/32768); 100 stays full scale.
#define VOLUME_DEFAULT 30u
static const int32_t volume_gain_q15[101]={
0,123,130,138,146,154,163,173,183,193,204,216,229,242,256,271,287,303,321,340,359,380,402,426,450,476,504,533,564,597,632,668,707,748,791,837,886,937,992,1049,1110,1175,1243,1315,1391,1472,1557,1648,1744,1845,1952,2065,2185,2312,2446,2588,2738,2897,3065,3243,3431,3630,3841,4064,4300,4549,4813,5093,5388,5701,6032,6382,6752,7144,7559,7997,8461,8952,9472,10022,10603,11219,11870,12559,13287,14059,14875,15738,16651,17618,18640,19722,20866,22077,23359,24714,26149,27666,29272,30971,32768};
static inline uint8_t volume_clamp(uint8_t v){return v>VOLUME_MAX?VOLUME_MAX:v;}
// Q8 fractional gain keeps quiet adjustments smooth; no per-sample division.
typedef struct {int32_t value,step,target;uint32_t remaining;} volume_ramp_t;
static inline void volume_ramp_target(volume_ramp_t *r,int32_t gain,uint32_t frames){
    int32_t target=gain*256;
    if(target==r->target)return;
    r->target=target;r->remaining=frames;r->step=(target-r->value)/(int32_t)frames;
}
static inline int32_t volume_ramp_next(volume_ramp_t *r){
    if(r->remaining){r->value+=r->step;if(!--r->remaining)r->value=r->target;}
    return r->value/256;
}
