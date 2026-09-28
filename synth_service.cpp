#include "volume_control.h"
#include "pico/stdlib.h"
#include "pico/critical_section.h"
#include "pico/flash.h"
#include "config.h"
using boolean=bool;
using byte=uint8_t;
#include "synth_backend.h"
#include "synth_service.h"
#include "sound_i2s.h"
#include "pop_capture.h"
#include <cstring>
#include <algorithm>

SynthControl g_synth;
static DodepanEngine engine;
enum class CommandType : uint8_t { On,Off,CC,Pitch,Program,Preset,Custom,Panic,Capture };
struct Command { CommandType type; uint8_t a,b; uint8_t values[PROGRAM_PARAMS_NUM]; };
static Command commands[256];
static uint16_t read_index,write_index,count;
static critical_section_t control_lock;
static uint8_t mirror[128], captures[NUM_PRESET_SLOTS][PROGRAM_PARAMS_NUM], capture_mask;
static uint8_t desired_volume=0;
static bool panic_requested,suspend_requested;
static synth_diagnostics_t stats;

void SynthControl::initialize(){critical_section_init(&control_lock);}
static void enqueue(Command command) {
    critical_section_enter_blocking(&control_lock);
    /* Only adjacent equal CC/Pitch commands can be coalesced: never cross a
       note, preset, capture, or transport boundary. */
    if(count && (command.type==CommandType::CC || command.type==CommandType::Pitch)) {
        Command &previous=commands[(write_index-1)&255];
        if(previous.type==command.type && (command.type==CommandType::Pitch || previous.a==command.a)) {
            previous=command;critical_section_exit(&control_lock);return;
        }
    }
    if(count==256) {
        count=read_index=write_index=0;stats.queue_overflows++;panic_requested=true;
        critical_section_exit(&control_lock);return;
    }
    commands[write_index]=command;write_index=(write_index+1)&255;count++;
    stats.queue_high_water=std::max(stats.queue_high_water,(uint32_t)count);
    critical_section_exit(&control_lock);
}
void SynthControl::note_on(uint8_t n,uint8_t v){if(n<128 && v<128)enqueue({v?CommandType::On:CommandType::Off,n,v,{}});}
void SynthControl::note_off(uint8_t n){if(n<128)enqueue({CommandType::Off,n,0,{}});}
void SynthControl::control_change(uint8_t cc,uint8_t v){if(cc<128 && v<128)enqueue({CommandType::CC,cc,v,{}});}
void SynthControl::pitch_bend(uint8_t l,uint8_t m){if(l<128 && m<128)enqueue({CommandType::Pitch,l,m,{}});}
void SynthControl::program_change(uint8_t p){enqueue({CommandType::Program,p,0,{}});}
void SynthControl::apply_preset(const uint8_t *v){Command c{CommandType::Preset,0,0,{}};memcpy(c.values,v,PROGRAM_PARAMS_NUM);enqueue(c);}
void SynthControl::apply_custom_preset(uint8_t id){if(id<6)enqueue({CommandType::Custom,id,0,{}});}
void SynthControl::all_notes_off(){enqueue({CommandType::Panic,0,0,{}});}
void SynthControl::capture_preset(uint8_t slot){if(slot<NUM_PRESET_SLOTS)enqueue({CommandType::Capture,slot,0,{}});}
uint8_t SynthControl::current_controller_value(uint8_t cc){
    critical_section_enter_blocking(&control_lock);uint8_t v=cc<128?mirror[cc]:0;critical_section_exit(&control_lock);return v;
}
void synth_set_volume(uint8_t v){critical_section_enter_blocking(&control_lock);desired_volume=volume_clamp(v);critical_section_exit(&control_lock);}
void synth_request_suspend(bool value){critical_section_enter_blocking(&control_lock);suspend_requested=value;critical_section_exit(&control_lock);}
void synth_get_diagnostics(synth_diagnostics_t *out){critical_section_enter_blocking(&control_lock);*out=stats;critical_section_exit(&control_lock);}
bool synth_take_capture(uint8_t *slot,uint8_t *v){
    critical_section_enter_blocking(&control_lock);
    for(uint8_t i=0;i<NUM_PRESET_SLOTS;i++)if(capture_mask&(1u<<i)){
        *slot=i;memcpy(v,captures[i],PROGRAM_PARAMS_NUM);capture_mask&=~(1u<<i);critical_section_exit(&control_lock);return true;
    }
    critical_section_exit(&control_lock);return false;
}
static void process_commands() {
    critical_section_enter_blocking(&control_lock);bool panic=panic_requested;panic_requested=false;critical_section_exit(&control_lock);
    if(panic){engine.control_change(64,0);engine.all_notes_off();}
    for(unsigned processed=0;processed<32;processed++) {
        critical_section_enter_blocking(&control_lock);
        if(!count){critical_section_exit(&control_lock);break;}
        Command c=commands[read_index];read_index=(read_index+1)&255;count--;
        critical_section_exit(&control_lock);
        switch(c.type){
        case CommandType::On:engine.note_on(c.a,c.b);break;
        case CommandType::Off:backend_off(engine,c.a);break;
        case CommandType::CC:engine.control_change(c.a,c.b);break;
        case CommandType::Pitch:engine.pitch_bend(c.a,c.b);break;
        case CommandType::Program:backend_program(engine,c.a);break;
        case CommandType::Custom:backend_custom(engine,c.a);break;
        case CommandType::Preset:
            backend_preset(engine,c.values);break;
        case CommandType::Panic:engine.control_change(64,0);engine.all_notes_off();break;
        case CommandType::Capture:
            critical_section_enter_blocking(&control_lock);
            for(unsigned i=0;i<PROGRAM_PARAMS_NUM;i++)captures[c.a][i]=engine.current_controller_value(dodepan_program_parameters[i]);
            capture_mask|=1u<<c.a;critical_section_exit(&control_lock);break;
        }
    }
    critical_section_enter_blocking(&control_lock);
    for(unsigned i=0;i<128;i++)mirror[i]=engine.current_controller_value(i);
    critical_section_exit(&control_lock);
}
// The DSP is always-inline: keep this owner in SRAM, as the original
// i2s_audio_task was, so sample rendering does not depend on XIP cache hits.
void __not_in_flash_func(synth_audio_main)() {
    if(!flash_safe_execute_core_init())panic("Flash coordination init failed");
    const sound_i2s_config config={I2S_PIO_NUM,I2S_CLOCK_PIN_BASE,I2S_DATA_PIN,I2S_CLOCK_PIN_BASE+1,SOUND_OUTPUT_FREQUENCY,16,false,AUDIO_TRANSPORT_FRAMES};
    if(sound_i2s_init(&config)!=0)panic("Audio initialization failed");
    engine.initialize();
    critical_section_enter_blocking(&control_lock);stats.ready=true;critical_section_exit(&control_lock);
    bool running=false;unsigned prefilled=0;
    volume_ramp_t volume_ramp={};int32_t gain=0;uint64_t quiet_since=0,suspend_since=0;
    while(true){
        critical_section_enter_blocking(&control_lock);
        bool suspend=suspend_requested;uint8_t volume=desired_volume;
        bool suspended=stats.suspended;
        critical_section_exit(&control_lock);
        if(suspended){
            if(suspend){tight_loop_contents();continue;}
            critical_section_enter_blocking(&control_lock);stats.suspended=false;critical_section_exit(&control_lock);
            running=false;prefilled=0;suspend_since=0;
        }
        if(suspend && !suspend_since)suspend_since=time_us_64();
        if(!suspend)suspend_since=0;
        if(suspend_since && time_us_64()-suspend_since>50000 && gain==0){
            audio_request_stop();
            critical_section_enter_blocking(&control_lock);stats.suspended=true;critical_section_exit(&control_lock);continue;
        }
        audio_buffer_t buffer;
        if(!audio_acquire_buffer(&buffer)){tight_loop_contents();continue;}
        uint64_t start=time_us_64();process_commands();
        int16_t right;int peak=0;int32_t target=suspend?0:volume_gain_q15[volume];
        volume_ramp_target(&volume_ramp,target,SYNTH_RENDER_FREQUENCY/100); // 10 ms
        for(unsigned i=0;i<AUDIO_BUFFER_LENGTH;i++){
            gain=volume_ramp_next(&volume_ramp);
#if DODEPAN_ENGINE_218
            int16_t output=(int32_t)engine.process(0,0,right)*gain/32768;
#else
            int16_t output=(int32_t)engine.process(right)*gain/32768;
#endif
            // Repeat the scaled sample without advancing the synth or volume ramp.
            for(unsigned repeat=0;repeat<AUDIO_TRANSPORT_REPEAT;repeat++){
                unsigned frame=i*AUDIO_TRANSPORT_REPEAT+repeat;
                buffer.samples[frame*2]=buffer.samples[frame*2+1]=output;
            }
            peak=std::max(peak,std::abs((int)output));
        }
        pop_capture_audio(buffer.samples,AUDIO_BUFFER_LENGTH);
        uint64_t now=time_us_64();uint32_t elapsed=(uint32_t)(now-start);
        if(peak>32)quiet_since=0;else if(!quiet_since)quiet_since=now;
        critical_section_enter_blocking(&control_lock);
        stats.max_render_us=std::max(stats.max_render_us,elapsed);stats.render_total_us+=elapsed;stats.render_buffers++;
        stats.quiet_ms=quiet_since?(uint32_t)((now-quiet_since)/1000):0;
        critical_section_exit(&control_lock);
        audio_publish_buffer(&buffer,elapsed);
        if(!running && ++prefilled==3){running=audio_start();if(!running)panic("Audio prefill failed");}
    }
}
