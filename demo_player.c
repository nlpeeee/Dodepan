#include "demo_player.h"
#include "demo_songs.h"
#include "note_router.h"
#include "display/display.h"
#include <string.h>
#define DEFAULT_TEMPO_US 500000u
static struct {
    demo_state_t state;
    uint8_t current_song,running_status;
    const uint8_t *midi_data;
    size_t midi_size,read_pos,track_end_pos;
    uint16_t ticks_per_quarter;
    uint32_t tempo_us,duration_ms;
    uint64_t current_tick,next_event_tick,play_start_us,next_event_us,remainder,last_display_us;
    bool in_track,parse_error;
} demo;
static uint32_t be32(const uint8_t *p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static bool parse_midi_header(void){
    const uint8_t *d=demo.midi_data;
    if(demo.midi_size<22 || memcmp(d,"MThd",4))return false;
    uint32_t length=be32(d+4);
    if(length<6 || length>demo.midi_size-16 || d[8] || d[9] || d[10] || d[11]!=1)return false;
    uint16_t division=((uint16_t)d[12]<<8)|d[13];if(!division || division&0x8000)return false;
    demo.ticks_per_quarter=division;demo.read_pos=8+length;
    d=demo.midi_data+demo.read_pos;if(memcmp(d,"MTrk",4))return false;
    uint32_t track_length=be32(d+4);demo.read_pos+=8;
    if(track_length>demo.midi_size-demo.read_pos)return false;
    demo.track_end_pos=demo.read_pos+track_length;demo.in_track=true;
    demo.next_event_us=demo.remainder=demo.current_tick=demo.next_event_tick=0;
    return true;
}
static uint32_t read_variable_length(void){
    uint32_t value=0;
    for(unsigned i=0;i<4;i++){
        if(demo.read_pos>=demo.track_end_pos){demo.parse_error=true;return 0;}
        uint8_t b=demo.midi_data[demo.read_pos++];value=(value<<7)|(b&127);
        if(!(b&128))return value;
    }
    demo.parse_error=true;return 0;
}
static bool read_next_event(void){
    if(!demo.in_track || demo.read_pos>=demo.track_end_pos)return false;
    uint32_t delta=read_variable_length();if(demo.parse_error)return false;
    demo.next_event_tick=demo.current_tick+delta;
    uint64_t numerator=(uint64_t)delta*demo.tempo_us+demo.remainder;
    demo.next_event_us+=numerator/demo.ticks_per_quarter;demo.remainder=numerator%demo.ticks_per_quarter;
    uint64_t duration=demo.next_event_us/1000;demo.duration_ms=duration>UINT32_MAX?UINT32_MAX:(uint32_t)duration;
    return demo.read_pos<demo.track_end_pos;
}
static void dispatch_midi_event(uint8_t status,uint8_t a,uint8_t b){
    switch(status&0xf0){
    case 0x80:notes_off(NOTE_SOURCE_DEMO,a);break;
    case 0x90:notes_on(NOTE_SOURCE_DEMO,a,b);break;
    case 0xb0:notes_control(NOTE_SOURCE_DEMO,a,b);break;
    case 0xe0:looper_send_pitch((int16_t)(((uint16_t)b<<7)|a)-8192);break;
    default:break;
    }
}
static bool process_and_advance(void){
    if(demo.read_pos>=demo.track_end_pos)return false;
    uint8_t status=demo.midi_data[demo.read_pos];
    if(status&0x80){demo.read_pos++;if(status<0xf0)demo.running_status=status;}
    else {status=demo.running_status;if(status<0x80 || status>=0xf0)return false;}
    if(status==0xff){
        if(demo.read_pos>=demo.track_end_pos)return false;
        uint8_t type=demo.midi_data[demo.read_pos++];uint32_t length=read_variable_length();
        if(demo.parse_error || length>demo.track_end_pos-demo.read_pos)return false;
        if(type==0x51){
            if(length!=3)return false;const uint8_t *d=demo.midi_data+demo.read_pos;
            uint32_t tempo=((uint32_t)d[0]<<16)|((uint32_t)d[1]<<8)|d[2];if(!tempo)return false;
            demo.tempo_us=tempo;
        }
        if(type==0x2f)return false;
        demo.read_pos+=length;
    }else if(status==0xf0 || status==0xf7){
        demo.running_status=0;uint32_t length=read_variable_length();
        if(demo.parse_error || length>demo.track_end_pos-demo.read_pos)return false;
        demo.read_pos+=length;
    }else{
        if(status>=0xf0)return false;
        unsigned bytes=(status&0xf0)==0xc0 || (status&0xf0)==0xd0?1:2;
        if(bytes>demo.track_end_pos-demo.read_pos)return false;
        uint8_t a=demo.midi_data[demo.read_pos++],b=bytes==2?demo.midi_data[demo.read_pos++]:0;
        if(a>=128 || b>=128)return false;
        dispatch_midi_event(status,a,b);
    }
    demo.current_tick=demo.next_event_tick;return read_next_event();
}
void demo_init(){memset(&demo,0,sizeof(demo));demo.tempo_us=DEFAULT_TEMPO_US;demo.ticks_per_quarter=480;}
uint8_t demo_get_song_count(){return DEMO_SONG_COUNT;}
const char *demo_get_song_name(uint8_t i){return i<DEMO_SONG_COUNT?demo_songs[i].name:"???";}
uint8_t demo_get_current_song(){return demo.current_song;}
void demo_stop(){notes_release_source(NOTE_SOURCE_DEMO);demo.state=DEMO_STOPPED;display_request_refresh();}
void demo_select_song(uint8_t i){demo_stop();demo.current_song=i<DEMO_SONG_COUNT?i:0;}
void demo_next_song(){demo_select_song((demo.current_song+1)%DEMO_SONG_COUNT);}
void demo_prev_song(){demo_select_song((demo.current_song+DEMO_SONG_COUNT-1)%DEMO_SONG_COUNT);}
void demo_start(){
    uint8_t selected=demo.current_song;demo_stop();demo_init();demo.current_song=selected;
    const demo_song_t *song=&demo_songs[selected];demo.midi_data=song->data;demo.midi_size=song->size;
    if(!parse_midi_header() || !read_next_event())return;
    demo.play_start_us=time_us_64();demo.state=DEMO_PLAYING;display_request_refresh();
}
bool demo_is_playing(){return demo.state==DEMO_PLAYING;}
demo_state_t demo_get_state(){return demo.state;}
uint32_t demo_get_elapsed_ms(){return demo_is_playing()?(uint32_t)((time_us_64()-demo.play_start_us)/1000):0;}
uint32_t demo_get_duration_ms(){return demo.duration_ms;}
void demo_task(){
    if(!demo_is_playing())return;uint64_t now=time_us_64(),elapsed=now-demo.play_start_us;
    /* Bound event work so a dense zero-delta asset cannot starve touch/USB. */
    for(unsigned n=0;n<32 && demo.next_event_us<=elapsed;n++)if(!process_and_advance()){demo_stop();return;}
    if(now-demo.last_display_us>=200000){demo.last_display_us=now;display_request_refresh();}
}
