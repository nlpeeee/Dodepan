#include "note_router.h"
#include <string.h>
static uint16_t owned[NOTE_SOURCE_COUNT][128];
static bool sustain[NOTE_SOURCE_COUNT];
static bool pitch_owned(uint8_t note){for(int s=0;s<NOTE_SOURCE_COUNT;s++)if(owned[s][note])return true;return false;}
static bool sustain_active(void){for(int s=0;s<NOTE_SOURCE_COUNT;s++)if(sustain[s])return true;return false;}
void notes_on(uint8_t source,uint8_t note,uint8_t velocity){
    if(source>=NOTE_SOURCE_COUNT || note>=128 || velocity>=128)return;
    if(!velocity){notes_off(source,note);return;}
    bool sounding=pitch_owned(note);
    if(owned[source][note]<UINT16_MAX)owned[source][note]++;
    if(!sounding)note_sink_on(note,velocity);
}
void notes_off(uint8_t source,uint8_t note){
    if(source>=NOTE_SOURCE_COUNT || note>=128 || !owned[source][note])return;
    owned[source][note]--;if(!pitch_owned(note))note_sink_off(note);
}
void notes_control(uint8_t source,uint8_t cc,uint8_t value){
    if(source>=NOTE_SOURCE_COUNT || cc>=128 || value>=128)return;
    if(cc==64){sustain[source]=value>=64;note_sink_cc(64,sustain_active()?127:0);}
    else if(cc==120 || cc==123)notes_release_source(source);
    else if(cc==121){sustain[source]=false;note_sink_cc(64,sustain_active()?127:0);}
    else note_sink_cc(cc,value);
}
void notes_release_source(uint8_t source){
    if(source>=NOTE_SOURCE_COUNT)return;
    for(int n=0;n<128;n++)if(owned[source][n]){owned[source][n]=0;if(!pitch_owned(n))note_sink_off((uint8_t)n);}
    if(sustain[source]){sustain[source]=false;note_sink_cc(64,sustain_active()?127:0);}
}
void notes_panic(void){
    for(int n=0;n<128;n++)if(pitch_owned(n))note_sink_off((uint8_t)n);
    memset(owned,0,sizeof(owned));memset(sustain,0,sizeof(sustain));
    note_sink_cc(64,0);note_sink_cc(123,0);note_sink_cc(120,0);
}
bool notes_any_active(void){for(int s=0;s<NOTE_SOURCE_COUNT;s++)for(int n=0;n<128;n++)if(owned[s][n])return true;return sustain_active();}
