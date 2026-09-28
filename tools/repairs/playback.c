/* Host probes execute the actual playback modules with a controlled clock and
 * event sink. Assertions describe desired behavior; failures expose the baseline.
 * Including C files lets fixtures reset private state without firmware changes.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../../note_router.c"
#include "../../looper.c"
#include "../../arpeggiator.c"
#include "../../demo_player.c"
static uint64_t now_us;
static int live[128], on_count, off_count, last_note, failures, checks;
static uint8_t pattern, octaves;
static uint16_t speed = 250;
uint64_t time_us_64(void) { return now_us; }
uint32_t time_us_32(void) { return (uint32_t)now_us; }
void display_request_refresh(void) {}
uint8_t get_arp_pattern(void) { return pattern; }
uint8_t get_arp_octave(void) { return octaves; }
uint16_t get_arp_speed_ms(void) { return speed; }
void set_arp_speed_ms(uint16_t n) { speed=n; }
uint8_t get_note_by_id(uint8_t id) { return 60+id; }
void note_sink_on(uint8_t n,uint8_t v) { (void)v; if(n<128) live[n]++; on_count++;last_note=n; }
void note_sink_off(uint8_t n) { if(n<128 && live[n]) live[n]--;off_count++; }
void note_sink_cc(uint8_t n,uint8_t v) {(void)n;(void)v;}
void play_single_note(uint8_t n,uint8_t v) {notes_on(NOTE_SOURCE_ARP,n,v);}
void stop_single_note(uint8_t n) {notes_off(NOTE_SOURCE_ARP,n);}
void looper_send_note_on(uint8_t n,uint8_t v) { notes_on(NOTE_SOURCE_LOOP,n,v); }
void looper_send_note_off(uint8_t n) { notes_off(NOTE_SOURCE_LOOP,n); }
void looper_send_cc(uint8_t n,uint8_t v) { (void)n;(void)v; }
void looper_send_pitch(int16_t b) { (void)b; }
void all_notes_off(void) { memset(live,0,sizeof(live)); }
static void reset(uint16_t capacity) {
    notes_panic();
    free(looper.events);memset(&looper,0,sizeof(looper));
    now_us=1000000;pattern=ARP_OFF;octaves=0;speed=250;
    looper_init(capacity,20000);arpeggiator_init();demo_init();
    memset(live,0,sizeof(live));on_count=off_count=0;last_note=-1;
}
static void check(const char* id,int ok,const char* description) {
    checks++;if(!ok)failures++;
    printf("%s %s: %s\n",ok?"PASS":"FAIL",id,description);
}
static void recorded_loop(uint32_t off_ms,uint32_t length_ms) {
    looper_enable();looper_record_note(60,100,true);
    now_us+=(uint64_t)off_ms*1000;looper_record_note(60,0,false);
    now_us+=(uint64_t)(length_ms-off_ms)*1000;looper_stop_record_and_play();
    looper_task();
}
int main(void) {
    reset(512);recorded_loop(50,100);now_us+=50000;looper_task();
    check("loop-normal",live[60]==0 && off_count==1,"note-off inside the loop is delivered");
    reset(512);recorded_loop(100,100);now_us+=100000;looper_task();
    check("loop-boundary",off_count==1,"note-off exactly at loop end must precede restart");
    reset(512);recorded_loop(90,100);now_us+=105000;looper_task();
    check("loop-late-task",off_count==1,"late task must drain note-off before wrapping");
    reset(512);recorded_loop(90,100);looper_stop();
    check("loop-pause",live[60]==0,"pausing playback releases its sounding notes");
    reset(512);looper_enable();looper_record_note(60,100,true);now_us+=20001000;looper_task();
    check("loop-limit-idle",!looper_is_recording(),"20-second recording limit works without further events");
    reset(2);looper_enable();looper_record_note(60,100,true);now_us+=10000;looper_record_cc(74,60);now_us+=10000;looper_record_note(60,0,false);now_us+=10000;looper_stop_record_and_play();looper_task();now_us+=20000;looper_task();
    check("loop-capacity",off_count>=1 && looper.event_count<=2 && looper_recording_limited(),"capacity exhaustion cannot discard a required release");
    reset(512);recorded_loop(50,100);looper_stop();looper_record_note(60,0,false);
    check("loop-paused-release",looper_get_state()==LOOP_PAUSED,"release after pause must not erase recording and re-arm");
    reset(512);pattern=ARP_UP;arpeggiator_pad_on(0,100);now_us+=200000;arpeggiator_task();
    check("arp-gate",live[60]==0 && off_count==1,"80-percent gate emits a release");
    reset(512);pattern=ARP_UP;arpeggiator_pad_on(0,100);pattern=ARP_OFF;now_us+=1000000;arpeggiator_task();
    check("arp-disable",live[60]==0,"disabling arpeggiator releases current note");
    reset(512);pattern=ARP_DOWN;octaves=2;arpeggiator_pad_on(0,100);octaves=0;now_us+=250000;arpeggiator_task();
    check("arp-octave-change",last_note==60,"reducing octave range does not underflow note calculation");
    reset(512);pattern=ARP_UP;now_us=0xfffffff0ULL;arpeggiator_pad_on(0,100);now_us+=64;arpeggiator_task();
    check("arp-clock-wrap",on_count==1 && off_count==0,"microsecond-clock wrap must not trigger an early gate/step");
    reset(512);pattern=ARP_UP;arpeggiator_pad_on(0,100);arpeggiator_pad_off(0);
    check("arp-pad-release",live[60]==0,"releasing last held pad stops the arpeggio");
    /* 480 PPQN, note on at 0; tempo changes at 480 from 500000 to
       1000000 us/quarter; note off at 960 is due at 1.5 seconds. */
    static const uint8_t tempo_song[]={
      'M','T','h','d',0,0,0,6,0,0,0,1,1,0xe0,
      'M','T','r','k',0,0,0,22,
      0,0x90,60,100,0x83,0x60,0xff,0x51,3,0x0f,0x42,0x40,
      0x83,0x60,0x80,60,0,0x83,0x60,0xff,0x2f,0
    };
    reset(512);demo.midi_data=tempo_song;demo.midi_size=sizeof(tempo_song);
    int parsed=parse_midi_header();read_next_event();demo.play_start_us=now_us;demo.state=DEMO_PLAYING;
    demo_task();now_us+=500000;demo_task();now_us+=1000000;demo_task();
    check("demo-tempo",parsed && off_count==1,"tempo changes integrate elapsed time piecewise");
    demo_stop();check("demo-stop",!demo_is_playing() && live[60]==0,"demo stop reaches stopped state and releases sink");
    demo_start();demo_task();demo_stop();demo_start();
    check("demo-restart",demo_is_playing() && demo.current_tick==0,"restart resets the embedded-song parser");
    uint8_t invalid_song[sizeof(tempo_song)];memcpy(invalid_song,tempo_song,sizeof(tempo_song));invalid_song[12]=invalid_song[13]=0;
    reset(512);demo.midi_data=invalid_song;demo.midi_size=sizeof(invalid_song);
    check("demo-zero-division",!parse_midi_header(),"zero ticks per quarter is rejected");
    for(unsigned song=0;song<demo_get_song_count();song++){
        reset(512);demo_select_song(song);demo_start();bool started=demo_is_playing();
        unsigned ticks=0;while(demo_is_playing() && ticks++<60000){now_us+=10000;demo_task();}
        check("demo-asset",started && !demo_is_playing() && !notes_any_active() && !demo.parse_error,"embedded song terminates and releases notes within ten minutes");
    }
    reset(512);notes_on(0,60,100);notes_on(NOTE_SOURCE_DEMO,60,80);demo_stop();
    check("demo-source-stop",live[60]==1,"demo stop preserves another source on the same pitch");notes_off(0,60);
    check("shared-last-off",live[60]==0,"final owner releases shared pitch");
    int malformed_safe=1;
    for(unsigned length=0;length<sizeof(tempo_song);length++){
        reset(512);demo.midi_data=tempo_song;demo.midi_size=length;
        if(parse_midi_header())malformed_safe=0;
    }
    check("demo-truncated",malformed_safe,"all truncated fixture lengths are rejected");
    uint8_t bad_event[sizeof(tempo_song)];memcpy(bad_event,tempo_song,sizeof(bad_event));
    memset(bad_event+22,0x80,5);reset(512);demo.midi_data=bad_event;demo.midi_size=sizeof(bad_event);
    check("demo-vlq-bound",parse_midi_header() && !read_next_event() && demo.parse_error,"five-byte delta is rejected");
    reset(512);recorded_loop(50,100);now_us+=10005000;looper_task();looper_stop();
    check("loop-long-gap",looper_missed_cycles()>=99 && !notes_any_active(),"large delay resynchronizes and stop releases playback");
    printf("RESULT %d checks, %d failures\n",checks,failures);
    free(looper.events);return failures?1:0;
}
