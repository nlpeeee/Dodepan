#include "arpeggiator.h"
#include "config.h"
#include "state.h"
#include "looper.h"
#include "note_router.h"
#include <stdlib.h>
#include <string.h>
extern void play_single_note(uint8_t note,uint8_t velocity);
extern void stop_single_note(uint8_t note);
static struct {
    uint16_t held_pads;
    uint8_t velocity,current_note,last_pattern,last_octaves;
    unsigned step;
    int direction;
    bool note_playing;
    uint64_t last_step_us,note_on_us;
} arp;
static uint64_t interval_us(){uint16_t s=get_arp_speed_ms();if(s<ARP_SPEED_MIN)s=ARP_SPEED_MIN;if(s>ARP_SPEED_MAX)s=ARP_SPEED_MAX;return (uint64_t)s*1000;}
static void stop_current_note(){if(arp.note_playing){stop_single_note(arp.current_note);looper_record_note(arp.current_note,0,false);arp.note_playing=false;}}
void arpeggiator_init(){memset(&arp,0,sizeof(arp));arp.direction=1;arp.velocity=100;if(!get_arp_speed_ms())set_arp_speed_ms(ARP_SPEED_DEFAULT);}
static bool next_note(uint8_t *out){
    uint8_t notes[12];unsigned count=0;
    for(unsigned i=0;i<12;i++)if(arp.held_pads&(1u<<i))notes[count++]=get_note_by_id(i);
    if(!count)return false;
    for(unsigned i=1;i<count;i++){uint8_t n=notes[i];unsigned j=i;while(j && notes[j-1]>n){notes[j]=notes[j-1];j--;}notes[j]=n;}
    uint8_t pattern=get_arp_pattern(),octaves=get_arp_octave()+1;
    if(octaves>3)octaves=3;
    unsigned total=count*octaves;
    if(pattern!=arp.last_pattern || octaves!=arp.last_octaves){arp.step=0;arp.direction=1;arp.last_pattern=pattern;arp.last_octaves=octaves;}
    arp.step%=total;unsigned index=arp.step;
    if(pattern==ARP_DOWN)index=total-1-arp.step;
    else if(pattern==ARP_RANDOM)index=(unsigned)rand()%total;
    else if(pattern!=ARP_UP && pattern!=ARP_UP_DOWN)return false;
    unsigned note=(unsigned)notes[index%count]+12*(index/count);*out=note<=127?note:127;
    if(pattern==ARP_UP_DOWN && total>1){
        if(arp.step==total-1)arp.direction=-1;else if(!arp.step)arp.direction=1;
        arp.step=(unsigned)((int)arp.step+arp.direction);
    }else arp.step=(arp.step+1)%total;
    return true;
}
static void play_next(){
    uint8_t n;if(!next_note(&n))return;stop_current_note();
    play_single_note(n,arp.velocity);looper_record_note(n,arp.velocity,true);
    arp.current_note=n;arp.note_playing=true;arp.note_on_us=time_us_64();
}
void arpeggiator_pad_on(uint8_t id,uint8_t velocity){
    if(id>=12 || !velocity)return;bool first=arp.held_pads==0;
    arp.held_pads|=1u<<id;arp.velocity=velocity;
    if(first){arp.step=0;arp.direction=1;arp.last_step_us=time_us_64();play_next();}
}
void arpeggiator_pad_off(uint8_t id){if(id>=12)return;arp.held_pads&=~(1u<<id);if(!arp.held_pads)arpeggiator_stop();}
void arpeggiator_stop(){stop_current_note();notes_release_source(NOTE_SOURCE_ARP);arp.held_pads=0;arp.step=0;arp.direction=1;}
bool arpeggiator_is_active(){return get_arp_pattern()!=ARP_OFF && arp.held_pads!=0;}
bool arpeggiator_is_enabled(){return get_arp_pattern()!=ARP_OFF;}
void arpeggiator_task(){
    if(!arpeggiator_is_enabled()){arpeggiator_stop();return;}
    if(!arp.held_pads)return;
    uint64_t now=time_us_64(),interval=interval_us();
    if(arp.note_playing && now-arp.note_on_us>=interval*ARP_GATE_PERCENT/100)stop_current_note();
    if(now-arp.last_step_us>=interval){arp.last_step_us+=((now-arp.last_step_us)/interval)*interval;play_next();}
}
