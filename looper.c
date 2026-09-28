#include "looper.h"
#include "config.h"
#include "note_router.h"
#include "display/display.h"
#include <stdlib.h>
#include <string.h>
static looper_t looper;
static uint16_t held[128],pending_releases;
static bool recording_limited;
static uint32_t missed_cycles;
static uint64_t last_display_us;
static void refresh(void){display_request_refresh();}
static void clear_internal(void){
    looper.event_count=looper.play_index=0;looper.loop_length_ms=0;looper.has_loop=false;
    memset(held,0,sizeof(held));pending_releases=0;recording_limited=false;
}
void looper_init(uint16_t capacity,uint32_t duration){
    free(looper.events);memset(&looper,0,sizeof(looper));
    looper.events=calloc(capacity,sizeof(looper_event_t));looper.state=LOOP_DISABLED;
    if(looper.events){looper.max_events=capacity;looper.max_length_ms=duration;}clear_internal();
}
bool looper_is_disabled(){return looper.state==LOOP_DISABLED || !looper.events;}
bool looper_is_recording(){return looper.state==LOOP_RECORDING;}
bool looper_is_playing(){return looper.state==LOOP_PLAYING;}
bool looper_has_loop(){return looper.has_loop && looper.loop_length_ms && looper.event_count;}
bool looper_has_events(){return looper.event_count!=0;}
bool looper_recording_limited(){return recording_limited;}
uint32_t looper_missed_cycles(){return missed_cycles;}
void looper_enable(){if(looper.events && looper_is_disabled())looper.state=LOOP_IDLE;refresh();}
void looper_clear(){notes_release_source(NOTE_SOURCE_LOOP);clear_internal();if(!looper_is_disabled())looper.state=LOOP_IDLE;refresh();}
void looper_disable(){notes_release_source(NOTE_SOURCE_LOOP);clear_internal();looper.state=LOOP_DISABLED;refresh();}
void looper_start_record(){
    if(looper_is_disabled())return;notes_release_source(NOTE_SOURCE_LOOP);clear_internal();
    looper.rec_start_us=time_us_64();looper.state=LOOP_RECORDING;refresh();
}
static uint32_t record_time(){uint64_t t=(time_us_64()-looper.rec_start_us)/1000;return t>looper.max_length_ms?looper.max_length_ms:(uint32_t)t;}
static void store_event(looper_event_type_t type,uint8_t a,uint8_t b,int16_t pitch,uint32_t when){
    looper_event_t *e=&looper.events[looper.event_count++];e->type=type;e->data1=a;e->data2=b;e->pitch=pitch;e->timestamp_ms=when;
}
void looper_stop_record_and_play(){
    if(!looper_is_recording())return;uint32_t length=record_time();if(!length)length=1;
    /* Every accepted note-on reserved a release slot. Closing the recording
       cannot lose that release, including at capacity or the time limit. */
    for(int n=0;n<128;n++)while(held[n]){
        store_event(LOOPER_EVENT_NOTE_OFF,(uint8_t)n,0,0,length);held[n]--;pending_releases--;
    }
    looper.loop_length_ms=length;looper.has_loop=looper.event_count>0;
    looper.play_index=0;looper.play_start_us=time_us_64();looper.state=looper.has_loop?LOOP_PLAYING:LOOP_IDLE;refresh();
}
void looper_stop(){if(looper_is_disabled())return;notes_release_source(NOTE_SOURCE_LOOP);looper.state=looper_has_loop()?LOOP_PAUSED:LOOP_IDLE;refresh();}
void looper_restart_from_start(){if(looper_is_disabled() || !looper_has_loop())return;notes_release_source(NOTE_SOURCE_LOOP);looper.play_index=0;looper.play_start_us=time_us_64();looper.state=LOOP_PLAYING;refresh();}
void looper_onpress(){
    switch(looper.state){case LOOP_IDLE:case LOOP_PAUSED:looper_start_record();break;case LOOP_RECORDING:looper_stop_record_and_play();break;case LOOP_PLAYING:looper_stop();break;default:break;}
}
static void append(looper_event_type_t type,uint8_t a,uint8_t b,int16_t pitch){
    if(!looper_is_recording())return;
    if((time_us_64()-looper.rec_start_us)/1000>=looper.max_length_ms){recording_limited=true;looper_stop_record_and_play();return;}
    int pending=pending_releases;
    if(type==LOOPER_EVENT_NOTE_ON)pending++;
    if(type==LOOPER_EVENT_NOTE_OFF){if(!held[a])return;pending--;}
    if((unsigned)looper.event_count+1+(unsigned)pending>looper.max_events){recording_limited=true;looper_stop_record_and_play();return;}
    store_event(type,a,b,pitch,record_time());
    if(type==LOOPER_EVENT_NOTE_ON)held[a]++;
    if(type==LOOPER_EVENT_NOTE_OFF)held[a]--;
    pending_releases=(uint16_t)pending;
}
void looper_record_note(uint8_t n,uint8_t v,bool on){
    if(n>=128 || v>=128 || looper_is_disabled())return;
    on=on && v!=0;
    if(on && (looper.state==LOOP_IDLE || looper.state==LOOP_PAUSED))looper_start_record();
    append(on?LOOPER_EVENT_NOTE_ON:LOOPER_EVENT_NOTE_OFF,n,v,0);
}
void looper_record_cc(uint8_t cc,uint8_t v){if(cc<128 && v<128)append(LOOPER_EVENT_CC,cc,v,0);}
void looper_record_pitch(int16_t p){if(p>=-8192 && p<=8191)append(LOOPER_EVENT_PITCH,0,0,p);}
static void dispatch_until(uint32_t elapsed){
    while(looper.play_index<looper.event_count){
        const looper_event_t *e=&looper.events[looper.play_index];if(e->timestamp_ms>elapsed)break;
        switch(e->type){
        case LOOPER_EVENT_NOTE_ON:looper_send_note_on(e->data1,e->data2);break;
        case LOOPER_EVENT_NOTE_OFF:looper_send_note_off(e->data1);break;
        case LOOPER_EVENT_CC:looper_send_cc(e->data1,e->data2);break;
        case LOOPER_EVENT_PITCH:looper_send_pitch(e->pitch);break;
        }
        looper.play_index++;
    }
}
void looper_task(){
    if(looper_is_disabled())return;uint64_t now=time_us_64();
    if(looper_is_recording() && now-looper.rec_start_us>=(uint64_t)looper.max_length_ms*1000){recording_limited=true;looper_stop_record_and_play();now=time_us_64();}
    if(looper_is_playing() && looper_has_loop()){
        uint64_t elapsed=(now-looper.play_start_us)/1000;
        if(elapsed>=looper.loop_length_ms){
            dispatch_until(looper.loop_length_ms);notes_release_source(NOTE_SOURCE_LOOP);
            uint64_t cycles=elapsed/looper.loop_length_ms;
            if(cycles>1)missed_cycles+=(uint32_t)(cycles-1);
            looper.play_start_us+=cycles*looper.loop_length_ms*1000;looper.play_index=0;
            elapsed%=looper.loop_length_ms;
        }
        dispatch_until((uint32_t)elapsed);
    }
    if(now-last_display_us>=200000){last_display_us=now;refresh();}
}
uint32_t looper_get_loop_length_ms(){return looper.loop_length_ms;}
uint16_t looper_get_event_count(){return looper.event_count;}
uint16_t looper_get_play_index(){return looper.play_index;}
looper_state_t looper_get_state(){return looper.state;}
uint32_t looper_get_elapsed_ms(){if(looper_is_recording())return record_time();if(looper_is_playing())return (uint32_t)((time_us_64()-looper.play_start_us)/1000);return 0;}
uint32_t looper_get_max_length_ms(){return looper.max_length_ms;}
