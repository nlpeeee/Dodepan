/* Dodepan
 * An expressive electronic instrument and Midi controller built on Raspberry Pi Pico".
 * By Turi Scandurra – https://turiscandurra.com/circuits
 */

#include "volume_control.h"
#include "config.h"         // Most configurable options are here
#include "pico/stdlib.h"
// Arduino types added for compatibility
typedef bool boolean;
typedef uint8_t byte;
#include "hardware/clocks.h"
#include "hardware/timer.h"  // For CPU profiling
#include "hardware/gpio.h"
#include "hardware/adc.h"   // Used for low battery detection
#include "hardware/flash.h"
#include "pico/binary_info.h"
#include "pico/multicore.h"
#include "sound_i2s.h"
#include "pra32-u2-common.h" // https://github.com/risgk/digital-synth-pra32-u2
#include "synth_service.h"
#include "pop_capture.h"
#include "settings_store.h"
#include "note_router.h"
#include "pico/flash.h"
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include "hardware/uart.h"
#include "instrument_preset.h"
#include "encoder.h"        // https://github.com/TuriSc/RP2040-Rotary-Encoder
#include "button.h"         // https://github.com/TuriSc/RP2040-Button
#include "bsp/board_api.h"  // For TinyUSB Midi
#include "tusb.h"           // For TinyUSB Midi
#include "scales.h"
#include "battery-check.h"
#include "imu.h"
#include "touch.h"
#include "looper.h"
#include "arpeggiator.h"
#include "demo_player.h"
#include "display/display.h"
#include "state.h"
#include "i2c_mutex.h"

/* Globals */

// The engine is owned exclusively by synth_service.cpp on core 1.

state_t* state;

Imu_data imu_data;

static uint8_t preset_storage[NUM_PRESET_SLOTS][PROGRAM_PARAMS_NUM];
static uint8_t scale_storage[NUM_SCALE_SLOTS][12];
static uint8_t *preset_rows[]={preset_storage[0],preset_storage[1],preset_storage[2],preset_storage[3]};
static uint8_t *scale_rows[]={scale_storage[0],scale_storage[1],scale_storage[2],scale_storage[3]};
uint8_t **user_presets=preset_rows;
uint8_t **user_scales=scale_rows;

#if defined (USE_DISPLAY)
ssd1306_t display;
#endif

static alarm_id_t power_on_alarm_id;
static alarm_id_t long_press_alarm_id;
static bool save_pending=false;
static uint64_t save_due_us;
static int saved_sector=-1;
static uint32_t saved_generation;
static settings_data_t saved_settings;
static bool have_saved_settings;
void request_flash_write();
void release_live_notes();
extern "C" void all_notes_off();
extern "C" bool settings_save_pending(){return save_pending;}

// Volatile flags for deferred interrupt processing
static volatile int32_t encoder_direction = 0;  // 0 = no change, 1 = up, -1 = down
static volatile bool button_pressed = false;
static volatile bool button_long_press_pending = false;

static uint32_t midi_overflows;
static void diagnostics_task(){
#if DODEPAN_UART_DIAGNOSTICS
    static bool initialized;
    static char line[512];static size_t length,position;
    static uint64_t last,previous;static uint32_t max_loop_us;
    uint64_t now=time_us_64();
    if(previous && now-previous>max_loop_us)max_loop_us=(uint32_t)(now-previous);previous=now;
    if(!initialized){uart_init(uart1,115200);gpio_set_function(8,GPIO_FUNC_UART);gpio_set_function(9,GPIO_FUNC_UART);initialized=true;}
    for(unsigned i=0;i<16 && position<length && uart_is_writable(uart1);i++)uart_get_hw(uart1)->dr=line[position++];
    synth_diagnostics_t s;synth_get_diagnostics(&s);
    if(position<length || !s.ready || now-last<1000000)return;
    last=now;audio_diagnostics_t a;audio_get_diagnostics(&a);
    int n=snprintf(line,sizeof(line),"Dodepan %s src=%s engine=%s flags=%u%u%u t=%llu clk=%lu frames=%u rate_mHz=%lu render_max=%lu avg_pct=%lu peak_pct=%lu irq_gap=%lu underrun=%lu late=%lu irq_miss=%lu dma=%llu observed_frames=%llu q_high=%lu q_loss=%lu midi_loss=%lu loop_gap=%lu imu=%u save=%u suspended=%u\n",
        PROGRAM_VERSION,DODEPAN_SOURCE_ID,DODEPAN_ENGINE_ID,DODEPAN_DISABLE_DISPLAY,DODEPAN_DISABLE_IMU,DODEPAN_DISABLE_PERSISTENCE,(unsigned long long)(now/1000000),(unsigned long)a.system_clock_hz,AUDIO_TRANSPORT_FRAMES,
        (unsigned long)a.frame_rate_millihz,(unsigned long)s.max_render_us,(unsigned long)get_cpu_avg_percent(),(unsigned long)get_cpu_max_percent(),(unsigned long)a.max_irq_gap_us,
        (unsigned long)a.underruns,(unsigned long)a.late_publications,(unsigned long)a.irq_deadline_misses,(unsigned long long)a.dma_notifications,(unsigned long long)a.observed_frames,
        (unsigned long)s.queue_high_water,(unsigned long)s.queue_overflows,(unsigned long)midi_overflows,
        (unsigned long)max_loop_us,imu_data.available,save_pending,s.suspended);
    length=n>0?(size_t)n:0;if(length>=sizeof(line))length=sizeof(line)-1;position=0;
#endif
}

void core1_main();

/* User Presets and flash memory */
static inline uint8_t get_argument_from_parameter(uint8_t parameter) {
    uint8_t control_number = dodepan_program_parameters[parameter];
    return g_synth.current_controller_value(control_number);
}

static inline void update_argument_from_parameter(uint8_t parameter) {
    uint8_t control_number = dodepan_program_parameters[parameter];
    uint8_t argument = g_synth.current_controller_value(control_number);
    set_argument(argument);
}

static inline void update_degree() {
    uint8_t step = get_step();
    uint8_t degree = get_degree(step);
    set_extended_scale(step, degree);
    set_scale_unsaved(true); // Used when the user modifies a scale but does not save it,
                             // and the name of the scale must be changed to "Custom"
}

void load_user_preset(uint8_t instrument) {
    if(!instrument_is_user(instrument))return;
    g_synth.apply_preset(user_presets[instrument-USER_INSTRUMENT_BASE]);
}

static inline void sync_control_change() {
    uint8_t parameter = get_parameter();
    uint8_t control_number = dodepan_program_parameters[parameter];
    uint8_t argument = get_argument();
    g_synth.control_change(control_number, argument);
}

void update_instrument() {
    all_notes_off();
    uint8_t instrument = get_instrument();
    switch (instrument) {
        case 0: // Load custom Dodepan preset
            g_synth.apply_custom_preset(0);
            set_preset_slot(-1); // No slot selected
        break;
        case 1: // Magic Bell - child-friendly preset
            g_synth.apply_custom_preset(1);
            set_preset_slot(-1);
        break;
        case 2: // Space Piano - child-friendly preset
            g_synth.apply_custom_preset(2);
            set_preset_slot(-1);
        break;
        case 3: // Robot Voice - child-friendly preset
            g_synth.apply_custom_preset(3);
            set_preset_slot(-1);
        break;
        case 4: // Synthwave - lush 80s style pad
            g_synth.apply_custom_preset(4);
            set_preset_slot(-1);
        break;
        case 5: // Bleep Bloop - Adventure Time style beeps
            g_synth.apply_custom_preset(5);
            set_preset_slot(-1);
        break;
        case 14:
        case 15:
        case 16:
        case 17:
            load_user_preset(instrument);
            // Set preset_slot selection to match loaded instrument
            set_preset_slot(instrument - 14);
        break;
        default: // case 6-13: load PRA32-U presets (shifted by 6)
            g_synth.program_change(instrument_factory_program(instrument));
            set_preset_slot(-1); // No slot selected
        break;
    }
    
    // Force polyphonic mode for chord support (required for chords to work)
    // This overrides any preset's voice mode setting
    if (get_chord_mode() != CHORD_OFF) {
        g_synth.control_change(VOICE_MODE, VOICE_POLYPHONIC);
    }
}

bool load_flash_data() {
    settings_data_t data;
    saved_sector=settings_select((const uint8_t*)(XIP_BASE+SETTINGS_SECTOR_A),(const uint8_t*)(XIP_BASE+SETTINGS_SECTOR_B),NUM_SCALES,&data,&saved_generation);
    bool legacy=saved_sector<0;
    if(legacy && !settings_decode_legacy((const uint8_t*)(XIP_BASE+SETTINGS_LEGACY_OFFSET),SETTINGS_LEGACY_BYTES,NUM_SCALES,&data))return false;
    set_key(data.key);set_instrument(data.instrument);set_imu_axes(data.imu_axes);
    set_volume(data.volume);set_contrast(data.contrast);set_chord_mode(data.chord);
    for(unsigned i=0;i<4;i++){memcpy(user_presets[i],data.presets[i],PROGRAM_PARAMS_NUM);memcpy(user_scales[i],data.scales[i],12);}
    set_and_extend_scale(data.scale);update_instrument();
    bool migrate=legacy || ((const uint8_t*)(XIP_BASE+(saved_sector?SETTINGS_SECTOR_B:SETTINGS_SECTOR_A)))[4]!=SETTINGS_FORMAT_VERSION;
    saved_settings=data;have_saved_settings=!migrate;
    if(migrate)request_flash_write();
#if defined(USE_DISPLAY)
    display_update_contrast(&display);
#endif
    return true;
}

struct flash_transaction_t { uint32_t offset; uint8_t record[SETTINGS_RECORD_BYTES]; };
static void flash_transaction(void *context) {
    auto *tx=(flash_transaction_t*)context;
    flash_range_erase(tx->offset,FLASH_SECTOR_SIZE);
    flash_range_program(tx->offset,tx->record,SETTINGS_RECORD_BYTES);
}
static settings_data_t capture_settings() {
    settings_data_t data{};
    data.key=get_key();data.scale=get_scale();data.instrument=get_instrument();data.imu_axes=get_imu_axes();
    data.volume=get_volume();data.contrast=get_contrast();data.chord=get_chord_mode();
    for(unsigned i=0;i<4;i++){memcpy(data.presets[i],user_presets[i],PROGRAM_PARAMS_NUM);memcpy(data.scales[i],user_scales[i],12);}
    return data;
}
static void persistence_task() {
    uint8_t slot,values[PROGRAM_PARAMS_NUM];
    while(synth_take_capture(&slot,values)){
        memcpy(user_presets[slot],values,PROGRAM_PARAMS_NUM);set_preset_has_changes(true);
        set_instrument(USER_INSTRUMENT_BASE+slot);request_flash_write();display_request_refresh();
    }
    if(!save_pending || time_us_64()<save_due_us)return;
#if DODEPAN_DISABLE_PERSISTENCE
    return;
#endif
    if(notes_any_active() || touch_any_held() || looper_is_playing() || looper_is_recording() || demo_is_playing()){
        synth_request_suspend(false);return;
    }
    settings_data_t data=capture_settings();
    if(have_saved_settings && !memcmp(&data,&saved_settings,sizeof(data))){
        save_pending=false;synth_request_suspend(false);display_request_refresh();return;
    }
    synth_diagnostics_t audio;synth_get_diagnostics(&audio);
    if(audio.quiet_ms<250)return;
    synth_request_suspend(true);if(!audio.suspended)return;
    flash_transaction_t tx{};int next=saved_sector==0?1:0;
    tx.offset=next?SETTINGS_SECTOR_B:SETTINGS_SECTOR_A;
    bool ok=settings_encode(tx.record,sizeof(tx.record),NUM_SCALES,&data,saved_generation+1);
    if(ok)ok=flash_safe_execute(flash_transaction,&tx,1000)==PICO_OK;
    settings_data_t verify;uint32_t generation;
    if(ok)ok=settings_decode((const uint8_t*)(XIP_BASE+tx.offset),SETTINGS_RECORD_BYTES,NUM_SCALES,&verify,&generation) && generation==saved_generation+1 && !memcmp(&verify,&data,sizeof(data));
    if(ok){saved_sector=next;saved_generation++;saved_settings=data;have_saved_settings=true;save_pending=false;set_preset_has_changes(false);set_scale_has_changes(false);}
    else save_due_us=time_us_64()+5000000;
    synth_request_suspend(false);display_request_refresh();
}

void request_flash_write(){
    save_pending=true;save_due_us=time_us_64()+(uint64_t)FLASH_WRITE_DELAY_S*1000000;
    display_request_refresh();
}

void submit_preset_slot(){
#if DODEPAN_ENGINE_218
    return; // Read-only conversion evaluation; native bank persistence is a separate migration.
#endif
    int8_t slot=get_preset_slot();if(slot<0 || slot>=NUM_PRESET_SLOTS)return;
    g_synth.capture_preset((uint8_t)slot);
}

void submit_scale_slot() {
    int8_t slot = get_scale_slot();
    if(slot == -1) { return; }
    for (uint8_t i = 0; i < 12; i++) {
        user_scales[slot][i] = get_extended_scale(i);
    }
    set_scale_has_changes(true);
    set_scale_unsaved(false); // Used when the user modifies a scale but does not save it,
                              // and the name of the scale must be changed to "Custom"

    request_flash_write();

    // Since we've written a scale, let's select it on the main screen
    set_scale(NUM_SCALES_BUILTIN + slot);
}

/* Note and audio */

uint8_t get_note_by_id(uint8_t id){if(id>=12)return 0;unsigned n=(unsigned)get_key()+get_extended_scale(id);return n<=127?n:127;}

// Chord interval definitions in semitones
// Power chord: root + perfect 5th (7 semitones)
// Triad: root + major 3rd (4 semitones) + perfect 5th (7 semitones)
// Using fixed intervals ensures consistent chord voicing regardless of scale
#define INTERVAL_THIRD  4   // Major 3rd = 4 semitones
#define INTERVAL_FIFTH  7   // Perfect 5th = 7 semitones

// Voicing interval tables (semitones from root)
// Each voicing has up to 5 intervals; 0xFF marks end
static const uint8_t voicing_intervals[NUM_VOICINGS][5] = {
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF},  // VOICING_BASE - handled specially
    {4, 8, 0xFF, 0xFF, 0xFF},         // VOICING_AUG: augmented [0,4,8]
    {3, 6, 0xFF, 0xFF, 0xFF},         // VOICING_DIM: diminished [0,3,6]
    {4, 7, 0xFF, 0xFF, 0xFF},         // VOICING_MAJ: major [0,4,7]
    {3, 7, 0xFF, 0xFF, 0xFF},         // VOICING_MIN: minor [0,3,7]
    {4, 7, 10, 0xFF, 0xFF},           // VOICING_DOM7: dom7 [0,4,7,10]
    {4, 7, 11, 0xFF, 0xFF},           // VOICING_MAJ7: maj7 [0,4,7,11]
    {3, 7, 10, 0xFF, 0xFF},           // VOICING_MIN7: min7 [0,3,7,10]
    {5, 7, 0xFF, 0xFF, 0xFF},         // VOICING_SUS4: sus4 [0,5,7]
    {2, 7, 0xFF, 0xFF, 0xFF},         // VOICING_SUS2: sus2 [0,2,7]
    {4, 7, 9, 0xFF, 0xFF},            // VOICING_6TH: 6th [0,4,7,9]
    {4, 7, 10, 14, 0xFF},             // VOICING_9TH: 9th [0,4,7,10,14]
};

// Tilt zone to voicing mapping (3x3 grid)
// Layout:   col0(left)  col1(center)  col2(right)
// row0(up):    aug       Maj/min       dom7
// row1(mid):   dim       (base)        Maj7/m7
// row2(down):  6/sus2    sus4          9
static const uint8_t tilt_zone_voicing[9] = {
    VOICING_AUG,   VOICING_MAJ,  VOICING_DOM7,   // row 0 (tilt up/away)
    VOICING_DIM,   VOICING_BASE, VOICING_MAJ7,   // row 1 (level)
    VOICING_SUS2,  VOICING_SUS4, VOICING_9TH,    // row 2 (tilt down/toward)
};

static uint8_t active_chord_notes[12][5],active_chord_count[12];
static bool pad_held[12];
static uint8_t pad_root_note[12],pad_velocity[12],pad_owner[12];
void note_off(uint8_t id);

// Core-0-only USB packet queue. Never block control processing on the host.
static uint8_t midi_packets[256][4];
static uint16_t midi_read,midi_write,midi_count;

static bool midi_panic_pending;
static inline uint32_t tudi_midi_write24(uint8_t jack,uint8_t status,uint8_t a,uint8_t b){
    if(!tud_midi_mounted())return 0;
    if(midi_panic_pending)return 0;
    if(midi_count==256){midi_count=midi_read=midi_write=0;midi_overflows++;midi_panic_pending=true;return 0;}
    uint8_t *p=midi_packets[midi_write];p[0]=(jack<<4)|(status>>4);p[1]=status;p[2]=a;p[3]=b;
    midi_write=(midi_write+1)&255;midi_count++;return 3;
}
static void midi_output_task(){
    static uint8_t panic_step;
    if(!tud_midi_mounted()){midi_count=midi_read=midi_write=0;midi_panic_pending=false;panic_step=0;return;}
    // USB-MIDI packet API accepts a complete event or retains it for retry.
    if(midi_panic_pending){
        const uint8_t controllers[]={64,123,120};
        uint8_t p[]={0x0b,0xb0,controllers[panic_step],0};
        if(tud_midi_packet_write(p) && ++panic_step==3){panic_step=0;midi_panic_pending=false;}
        return;
    }
    panic_step=0;
    for(unsigned i=0;i<16 && midi_count;i++){
        if(!tud_midi_packet_write(midi_packets[midi_read]))break;
        midi_read=(midi_read+1)&255;midi_count--;
    }
}

// Helper functions for playing notes, callable from C (arpeggiator.c)
extern "C" {

// Get current tilt zone for display (callable from C)
uint8_t get_tilt_zone() {
    return imu_data.tilt_zone;
}
void note_sink_on(uint8_t n,uint8_t v){g_synth.note_on(n,v);
#if defined(USE_MIDI)
    tudi_midi_write24(0,0x90,n,v);
#endif
}
void note_sink_off(uint8_t n){g_synth.note_off(n);
#if defined(USE_MIDI)
    tudi_midi_write24(0,0x80,n,0);
#endif
}
void note_sink_cc(uint8_t c,uint8_t v){g_synth.control_change(c,v);
#if defined(USE_MIDI)
    tudi_midi_write24(0,0xb0,c,v);
#endif
}
// Helper to play a single note (internal synth + MIDI)
// Called by both note_on and the arpeggiator
void play_single_note(uint8_t n,uint8_t v){notes_on(NOTE_SOURCE_ARP,n,v);}

// Helper to stop a single note (internal synth + MIDI)
// Called by both note_off and the arpeggiator
void stop_single_note(uint8_t n){notes_off(NOTE_SOURCE_ARP,n);}

// Looper playback hooks (called from looper.c)
void looper_send_note_on(uint8_t n,uint8_t v){notes_on(NOTE_SOURCE_LOOP,n,v);}

void looper_send_note_off(uint8_t n){notes_off(NOTE_SOURCE_LOOP,n);}

void looper_send_cc(uint8_t c,uint8_t v){notes_control(NOTE_SOURCE_LOOP,c,v);}

void looper_send_pitch(int16_t pitch_bend) {
    int16_t bend = pitch_bend + 8192; // Convert signed back to 14-bit MIDI range
    if (bend < 0) bend = 0;
    if (bend > 16383) bend = 16383;
    uint8_t lsb = bend & 0x7F;
    uint8_t msb = (bend >> 7) & 0x7F;
    g_synth.pitch_bend(lsb, msb);
#if defined(USE_MIDI)
    tudi_midi_write24(0, 0xE0, lsb, msb);
#endif
}

} // extern "C"

static uint8_t make_chord(uint8_t root,uint8_t mode,uint8_t zone,uint8_t *notes){
    uint8_t count=0;notes[count++]=root;
    uint8_t local[5]={0xff,0xff,0xff,0xff,0xff};const uint8_t *intervals=local;
    if(mode==CHORD_POWER)local[0]=7;
    if(mode==CHORD_TRIAD){local[0]=4;local[1]=7;}
    if(mode==CHORD_OCTAVE)local[0]=12;
    if(mode==CHORD_TILT){uint8_t v=tilt_zone_voicing[zone<9?zone:4];if(v==VOICING_BASE)v=VOICING_MAJ;intervals=voicing_intervals[v];}
    for(unsigned i=0;i<5 && intervals[i]!=0xff && count<5;i++){unsigned n=(unsigned)root+intervals[i];if(n<=127)notes[count++]=(uint8_t)n;}
    return count;
}
void note_on(uint8_t id,uint8_t velocity){
    if(id>=12)return;note_off(id);
    uint8_t mode=get_chord_mode();pad_root_note[id]=get_note_by_id(id);pad_velocity[id]=velocity;pad_held[id]=true;
    if(mode!=CHORD_OFF)g_synth.control_change(VOICE_MODE,VOICE_POLYPHONIC);
    active_chord_count[id]=make_chord(pad_root_note[id],mode,imu_data.tilt_zone,active_chord_notes[id]);
    for(unsigned i=0;i<active_chord_count[id];i++){uint8_t n=active_chord_notes[id][i];notes_on(id,n,velocity);looper_record_note(n,velocity,true);}
}

void note_off(uint8_t id){
    if(id>=12)return;pad_held[id]=false;
    for(unsigned i=0;i<active_chord_count[id];i++){uint8_t n=active_chord_notes[id][i];notes_off(id,n);looper_record_note(n,0,false);}
    active_chord_count[id]=0;
}

// Update chord voicing for all held pads when tilt zone changes (rolling chords)
// Retain common pitches; send balanced releases and onsets for changed pitches.
void tilt_chord_update(){
    if(get_chord_mode()!=CHORD_TILT)return;
    for(uint8_t id=0;id<12;id++)if(pad_held[id]){
        uint8_t target[5];uint8_t count=make_chord(pad_root_note[id],CHORD_TILT,imu_data.tilt_zone,target);
        for(unsigned i=0;i<active_chord_count[id];i++){
            uint8_t n=active_chord_notes[id][i];bool keep=false;for(unsigned j=0;j<count;j++)keep|=target[j]==n;
            if(!keep){notes_off(id,n);looper_record_note(n,0,false);}
        }
        for(unsigned i=0;i<count;i++){
            uint8_t n=target[i];bool old=false;for(unsigned j=0;j<active_chord_count[id];j++)old|=active_chord_notes[id][j]==n;
            if(!old){notes_on(id,n,pad_velocity[id]);looper_record_note(n,pad_velocity[id],true);}
        }
        memcpy(active_chord_notes[id],target,count);active_chord_count[id]=count;
    }
}

void touch_on(uint8_t id){
    if(id>=12)return;set_pad_active(id,true);set_last_note(get_note_by_id(id));
    pad_owner[id]=arpeggiator_is_enabled()?2:1;
    if(pad_owner[id]==2)arpeggiator_pad_on(id,imu_data.acceleration);else note_on(id,imu_data.acceleration);
    display_request_refresh();
}

void touch_off(uint8_t id){
    if(id>=12)return;set_pad_active(id,false);
    if(pad_owner[id]==2)arpeggiator_pad_off(id);else if(pad_owner[id]==1)note_off(id);
    pad_owner[id]=0;display_request_refresh();
}

void release_live_notes(){
    for(uint8_t id=0;id<12;id++){note_off(id);notes_release_source(id);pad_owner[id]=0;set_pad_active(id,false);}
    arpeggiator_stop();notes_release_source(NOTE_SOURCE_ARP);
}
extern "C" void all_notes_off(){release_live_notes();notes_panic();g_synth.all_notes_off();}

// Use the IMU to alter parameters according to device tilting
void tilt_process() {
    static uint8_t cc_throttle;
    if(get_imu_axes() & 0x02) {
        g_synth.control_change(FILTER_CUTOFF, imu_data.deviation_y);
        // Thin out recorded CC events to avoid exhausting the looper buffer
        if ((cc_throttle++ & 0x03) == 0) {
            looper_record_cc(FILTER_CUTOFF, imu_data.deviation_y);
        }
    }

    // Split the bytes
    uint8_t bending_lsb = imu_data.deviation_x & 0x7F;
    uint8_t bending_msb = (imu_data.deviation_x >> 7) & 0x7F;

    // Send the instruction to the synth
    if(get_imu_axes() & 0x01) {
        g_synth.pitch_bend(bending_lsb, bending_msb);
        static uint8_t pitch_throttle;
        int16_t bend_signed = (int16_t)imu_data.deviation_x - 8192;
        if ((pitch_throttle++ & 0x03) == 0) {
            looper_record_pitch(bend_signed);
        }

#if defined (USE_MIDI)
        static uint8_t throttle;
        if(throttle++ % 10 != 0) return; // Limit the message rate
        // Pitch wheel range is between 0 and 16383 (0x0000 to 0x3FFF),
        // with 8192 (0x2000) being the center value.
        // Send the Midi message
        tudi_midi_write24 (0, 0xE0, bending_lsb, bending_msb);
#endif
    }
}

// Core-0 display/UART consumers share a one-second render-load window.
extern "C" uint32_t get_cpu_avg_percent(){
    static uint64_t last_us,last_total,last_buffers;
    static uint32_t average;
    uint64_t now=time_us_64();
    if(now-last_us>=1000000){
        synth_diagnostics_t d;synth_get_diagnostics(&d);
        uint64_t rendered=d.render_total_us-last_total, buffers=d.render_buffers-last_buffers;
        average=buffers?(uint32_t)(rendered*SOUND_OUTPUT_FREQUENCY/(buffers*AUDIO_TRANSPORT_FRAMES*10000ULL)):0;
        last_total=d.render_total_us;last_buffers=d.render_buffers;last_us=now;
    }
    return average;
}
extern "C" uint32_t get_cpu_max_percent(){
    synth_diagnostics_t d;synth_get_diagnostics(&d);
    return (uint32_t)((uint64_t)d.max_render_us*SOUND_OUTPUT_FREQUENCY/(AUDIO_TRANSPORT_FRAMES*10000ULL));
}
extern "C" void get_audio_error_counts(uint32_t *underruns,uint32_t *late,uint32_t *irq_misses){
    synth_diagnostics_t s;synth_get_diagnostics(&s);
    *underruns=*late=*irq_misses=0;if(!s.ready)return;
    audio_diagnostics_t d;audio_get_diagnostics(&d);
    *underruns=d.underruns;*late=d.late_publications;*irq_misses=d.irq_deadline_misses;
}

int64_t power_on_complete(alarm_id_t id, void *) {
    gpio_put(PICO_DEFAULT_LED_PIN, 0);
    return 0;
}

void intro_complete() {
    set_context(CTX_SELECTION);
}

/* I/O functions */

void encoder_up() {
    context_t context = get_context();
    switch (context) {
        case CTX_SELECTION:
            set_selection_up();
            // Force a display refresh, to clear out any glitches that might
            // have occurred earlier due to I²C timeouts
#if defined (USE_DISPLAY)
            switch(get_selection()) {
                case SELECTION_KEY:
                case SELECTION_CHORD:
                case SELECTION_ARPEGGIO:
                case SELECTION_LOOPER:
                case SELECTION_IMU_CONFIG:
                    display_refresh(&display);
                break;
            }
#endif
        break;
        case CTX_KEY:
            // D#7 is the highest note that can be set as root note
            release_live_notes();
            set_key_up();
        break;
        case CTX_SCALE:
            release_live_notes();
            set_scale_up();
        break;
        case CTX_INSTRUMENT:
            set_instrument_up();
            update_instrument();
        break;
        case CTX_CHORD:
            release_live_notes();
            set_chord_mode_up();
            // Force polyphonic mode for chord support
            if (get_chord_mode() != CHORD_OFF) {
                g_synth.control_change(VOICE_MODE, VOICE_POLYPHONIC);
            }
        break;
        case CTX_ARP_PATTERN:
            release_live_notes();
            set_arp_pattern_up();
        break;
        case CTX_ARP_SPEED:
            set_arp_speed_up();
        break;
        case CTX_ARP_OCTAVE:
            set_arp_octave_up();
        break;
        case CTX_IMU_CONFIG:
            set_imu_axes_up();
        break;
        case CTX_VOLUME:
            set_volume_up();
            request_flash_write();
        break;
        case CTX_CONTRAST:
            set_contrast_up();
#if defined (USE_DISPLAY)
            display_update_contrast(&display);
#endif
        break;
        case CTX_SYNTH_EDIT_PARAM:
            set_parameter_up();
            update_argument_from_parameter(get_parameter());
        break;
        case CTX_SYNTH_EDIT_ARG:
            set_argument_up();
            sync_control_change();
        break;
        case CTX_SYNTH_EDIT_STORE:
            set_preset_slot_up();
        break;
        case CTX_LOOPER:
            // Restart loop from beginning
            if (looper_has_loop()) {
                looper_restart_from_start();
            }
            set_context(CTX_LOOPER); // Keep context steady to avoid stray handlers
        break;
        case CTX_DEMO:
            // Stop playback before changing song
            if (demo_is_playing()) {
                demo_stop();
            }
            demo_next_song();
        break;
        case CTX_SCALE_EDIT_STEP:
            set_step_up();
        break;
        case CTX_SCALE_EDIT_DEG:
            set_degree_up();
            update_degree();
        break;
        case CTX_SCALE_EDIT_STORE:
            set_scale_slot_up();
        break;
        case CTX_INIT:
        case CTX_INFO:
        default:
            ; // Do nothing
        break;
    }
#if defined (USE_DISPLAY)
    if(get_contrast() == CONTRAST_AUTO) {
        display_wake(&display);
    }
    display_draw(&display);
#endif
}

void encoder_down() {
    context_t context = get_context();
    switch (get_context()) {
        case CTX_SELECTION:
            set_selection_down();
            // Force a display refresh, to clear out any glitches that might
            // have occurred earlier due to I²C timeouts
#if defined (USE_DISPLAY)
            switch(get_selection()) {
                case SELECTION_VOLUME:
                case SELECTION_CHORD:
                case SELECTION_ARPEGGIO:
                case SELECTION_LOOPER:
                case SELECTION_IMU_CONFIG:
                    display_refresh(&display);
                break;
            }
#endif
        break;
        case CTX_KEY:
            release_live_notes();
            set_key_down();
        break;
        case CTX_SCALE:
            release_live_notes();
            set_scale_down();
        break;
        case CTX_INSTRUMENT:
            set_instrument_down();
            update_instrument();
        break;
        case CTX_CHORD:
            release_live_notes();
            set_chord_mode_down();
            // Force polyphonic mode for chord support
            if (get_chord_mode() != CHORD_OFF) {
                g_synth.control_change(VOICE_MODE, VOICE_POLYPHONIC);
            }
        break;
        case CTX_ARP_PATTERN:
            release_live_notes();
            set_arp_pattern_down();
        break;
        case CTX_ARP_SPEED:
            set_arp_speed_down();
        break;
        case CTX_ARP_OCTAVE:
            set_arp_octave_down();
        break;
        case CTX_IMU_CONFIG:
            set_imu_axes_down();
        break;
        case CTX_VOLUME:
            set_volume_down();
            request_flash_write();
        break;
        case CTX_CONTRAST:
            set_contrast_down();
#if defined (USE_DISPLAY)
            display_update_contrast(&display);
#endif
        break;
        case CTX_SYNTH_EDIT_PARAM:
            set_parameter_down();
            update_argument_from_parameter(get_parameter());
        break;
        case CTX_SYNTH_EDIT_ARG:
            set_argument_down();
            sync_control_change();
        break;
        case CTX_SYNTH_EDIT_STORE:
            set_preset_slot_down();
        break;
        case CTX_LOOPER:
            // Restart loop from beginning
            if (looper_has_loop()) {
                looper_restart_from_start();
            }
            set_context(CTX_LOOPER); // Keep context steady to avoid stray handlers
        break;
        case CTX_DEMO:
            // Stop playback before changing song
            if (demo_is_playing()) {
                demo_stop();
            }
            demo_prev_song();
        break;
        case CTX_SCALE_EDIT_STEP:
            set_step_down();
        break;
        case CTX_SCALE_EDIT_DEG:
            set_degree_down();
            update_degree();
        break;
        case CTX_SCALE_EDIT_STORE:
            set_scale_slot_down();
        break;
        case CTX_INIT:
        case CTX_INFO:
        default:
            ; // Do nothing
        break;
    }
#if defined (USE_DISPLAY)
    if(get_contrast() == CONTRAST_AUTO) {
        display_wake(&display);
    }
    display_draw(&display);
#endif
}

void encoder_onchange(rotary_encoder_t *encoder) {
    static long int last_position;
    long int position = encoder->position / 4; // Adjust the encoder sensitivity here
    if(last_position != position) {
        // Set flag for main loop to process (avoid I2C/display calls in interrupt context)
        if(encoder_direction>-128 && encoder_direction<128)encoder_direction += (last_position < position ? 1 : -1);
    }
    last_position = position;
}

int64_t on_long_press(alarm_id_t id, void *) {
    // Just set flag - actual processing happens in main loop
    // This avoids I2C/display calls in interrupt/alarm context
    button_long_press_pending = true;
    long_press_alarm_id = 0;  // Clear the alarm ID since it fired
    return 0;
}

// Actual long press handler - called from main loop
void button_long_press() {
    context_t context = get_context();
    selection_t selection = get_selection();
    switch(context) {
        case CTX_SELECTION:
            switch (selection) {
                case SELECTION_SCALE:
                    set_context(CTX_SCALE_EDIT_STEP);
                break;
                case SELECTION_KEY:
                    set_context(CTX_INFO);
                break;
                case SELECTION_INSTRUMENT:
                    set_context(CTX_SYNTH_EDIT_PARAM);
                    update_argument_from_parameter(get_parameter());
                break;
                default:
                    // Do nothing
                break;
                }
        break;
        case CTX_KEY:
            set_context(CTX_INFO);
        break;
        case CTX_VOLUME:
            set_context(CTX_CONTRAST);
        break;
        case CTX_CONTRAST:
        case CTX_CHORD:
        case CTX_ARP_PATTERN:
        case CTX_ARP_SPEED:
        case CTX_ARP_OCTAVE:
        case CTX_IMU_CONFIG:
            set_context(CTX_SELECTION);
        break;
        case CTX_INSTRUMENT:
            set_context(CTX_SYNTH_EDIT_PARAM);
            update_argument_from_parameter(get_parameter());
        break;
        case CTX_SCALE:
            set_context(CTX_SCALE_EDIT_STEP);
        break;
        case CTX_SCALE_EDIT_STEP:
        case CTX_SCALE_EDIT_DEG:
            set_context(CTX_SCALE_EDIT_STORE);
        break;
        case CTX_SYNTH_EDIT_PARAM:
        case CTX_SYNTH_EDIT_ARG:
            set_context(CTX_SYNTH_EDIT_STORE);
        break;
        case CTX_INFO:
            set_context(CTX_SELECTION);
        break;
        case CTX_LOOPER:
            // Stop and disable looper before exiting
            looper_disable();
            set_context(CTX_SELECTION);
        break;
        case CTX_DEMO:
            // Stop demo playback and exit
            demo_stop();
            set_context(CTX_SELECTION);
        break;
        case CTX_INIT:
        case CTX_SYNTH_EDIT_STORE:
        case CTX_SCALE_EDIT_STORE:
        default:
            ; // Do nothing
        break;
    }

#if defined (USE_DISPLAY)
    if(get_contrast() == CONTRAST_AUTO) {
        display_wake(&display);
    }
    display_draw(&display);
#endif
}

// Actual short press handler - called from main loop
void button_short_press() {
    context_t context = get_context();
    selection_t selection = get_selection();

    switch(context){
        case CTX_SELECTION: {
            switch (selection) {
                case SELECTION_KEY:
                    set_context(CTX_KEY);
                break;
                case SELECTION_SCALE:
                    set_context(CTX_SCALE);
                break;
                case SELECTION_INSTRUMENT:
                    set_context(CTX_INSTRUMENT);
                break;
                case SELECTION_CHORD:
                    set_context(CTX_CHORD);
                break;
                case SELECTION_ARPEGGIO:
                    set_context(CTX_ARP_PATTERN);
                break;
                case SELECTION_VOLUME:
                    set_context(CTX_VOLUME);
                break;
                case SELECTION_LOOPER:
                    set_context(CTX_LOOPER);
                    looper_enable();
                break;
                case SELECTION_DEMO:
                    set_context(CTX_DEMO);
                break;
                case SELECTION_IMU_CONFIG:
                    set_context(CTX_IMU_CONFIG);
                break;
                default:
                    ; // Do nothing
                break;
            }
        }
        break;
        case CTX_INFO:
        case CTX_KEY:
        case CTX_SCALE:
        case CTX_INSTRUMENT:
        case CTX_CHORD:
        case CTX_VOLUME:
        case CTX_CONTRAST:
        case CTX_IMU_CONFIG:
            set_context(CTX_SELECTION);
            request_flash_write();
        break;
        case CTX_ARP_PATTERN:
            set_context(CTX_ARP_SPEED);
        break;
        case CTX_ARP_SPEED:
            set_context(CTX_ARP_OCTAVE);
        break;
        case CTX_ARP_OCTAVE:
            set_context(CTX_SELECTION);
            // Don't save arp to flash - it resets on boot
        break;
        case CTX_SYNTH_EDIT_PARAM:
            set_context(CTX_SYNTH_EDIT_ARG);
        break;
        case CTX_SYNTH_EDIT_ARG:
            set_context(CTX_SYNTH_EDIT_PARAM);
        break;
        case CTX_SYNTH_EDIT_STORE:
            submit_preset_slot();
            set_context(CTX_SELECTION);
            set_selection(SELECTION_INSTRUMENT);
        break;
        case CTX_LOOPER:
            looper_onpress();
        break;
        case CTX_DEMO:
            // Toggle play/stop
            if (demo_is_playing()) {
                demo_stop();
            } else {
                demo_start();
            }
        break;
        case CTX_SCALE_EDIT_STEP:
            set_context(CTX_SCALE_EDIT_DEG);
        break;
        case CTX_SCALE_EDIT_DEG:
            set_context(CTX_SCALE_EDIT_STEP);
        break;
        case CTX_SCALE_EDIT_STORE:
            submit_scale_slot();
            set_context(CTX_SELECTION);
            set_selection(SELECTION_SCALE);
        break;
        case CTX_INIT:
        default:
            ; // Do nothing
        break;
    }
#if defined (USE_DISPLAY)
    if(get_contrast() == CONTRAST_AUTO) {
        display_wake(&display);
    }
    display_draw(&display);
#endif
}

// Button callback - runs in interrupt/alarm context, just sets flag
void button_onchange(button_t *button_p) {
    button_t *button = (button_t*)button_p;
    if (button->state) {
        // Button released - trigger short press if long press hasn't fired
        if (long_press_alarm_id) {
            cancel_alarm(long_press_alarm_id);
            long_press_alarm_id = 0;
            // Long press didn't fire, so this is a short press
            button_pressed = true;
        }
        return;
    }
    // Button pressed - start long press timer (short press triggers on release)
    long_press_alarm_id = add_alarm_in_ms(LONG_PRESS_THRESHOLD, on_long_press, NULL, true);
}

void battery_low_detected() {
    set_low_batt(true);
    battery_check_stop(); // Stop the timer
}

// Declare binary information
void bi_decl_all() {
    bi_decl(bi_program_name(PROGRAM_NAME));
    bi_decl(bi_program_description(PROGRAM_DESCRIPTION));
    bi_decl(bi_program_version_string(PROGRAM_VERSION));
    bi_decl(bi_program_url(PROGRAM_URL));
    bi_decl(bi_1pin_with_name(MPR121_SDA_PIN, MPR121_SDA_DESCRIPTION));
    bi_decl(bi_1pin_with_name(MPR121_SCL_PIN, MPR121_SCL_DESCRIPTION));
    bi_decl(bi_1pin_with_name(ENCODER_DT_PIN, ENCODER_DT_DESCRIPTION));
    bi_decl(bi_1pin_with_name(ENCODER_CLK_PIN, ENCODER_CLK_DESCRIPTION));
    bi_decl(bi_1pin_with_name(ENCODER_SWITCH_PIN, ENCODER_SWITCH_DESCRIPTION));
    bi_decl(bi_1pin_with_name(BATT_LVL_PIN, BATT_LVL_DESCRIPTION));
    
#if defined USE_DISPLAY && defined USE_IMU
    bi_decl(bi_1pin_with_name(SSD1306_SDA_PIN, SSD1306_MPU6050_SDA_DESCRIPTION));
    bi_decl(bi_1pin_with_name(SSD1306_SCL_PIN, SSD1306_MPU6050_SCL_DESCRIPTION));
#elif defined USE_DISPLAY
    bi_decl(bi_1pin_with_name(SSD1306_SDA_PIN, SSD1306_SDA_DESCRIPTION));
    bi_decl(bi_1pin_with_name(SSD1306_SCL_PIN, SSD1306_SCL_DESCRIPTION));
#elif defined USE_IMU
    bi_decl(bi_1pin_with_name(SSD1306_SDA_PIN, MPU6050_SDA_DESCRIPTION));
    bi_decl(bi_1pin_with_name(SSD1306_SCL_PIN, MPU6050_SCL_DESCRIPTION));
#endif
    bi_decl(bi_3pins_with_names(I2S_DATA_PIN, I2S_DATA_DESCRIPTION,
    I2S_CLOCK_PIN_BASE, I2S_BCK_DESCRIPTION,
    I2S_CLOCK_PIN_BASE+1, I2S_LRCK_DESCRIPTION));
}

// Secondary core task - handles audio generation
// With higher clock speed, single core can handle 4-voice polyphony
void core1_main(){synth_audio_main();}

int main() {
    // Adjust the clock speed to be an even multiplier
    // of the audio sampling frequency
    // RP2350 runs at higher clock for 4-voice polyphony on single core
    // Use SYSTEM_CLOCK_KHZ from config.h - lower if experiencing instability
    if(!set_sys_clock_khz(SYSTEM_CLOCK_KHZ, false))panic("Unsupported system clock");
    stdio_init_all();

    bi_decl_all();

#if defined (USE_MIDI)
    // Enable Midi device functionality
    board_init();
    tud_init(BOARD_TUD_RHPORT);

    if (board_init_after_tusb) {
        board_init_after_tusb();
    }
#endif

    // Initialize display and IMU (sharing an I²C bus)
#if defined (USE_DISPLAY) || defined (USE_IMU)
    gpio_init(SSD1306_SDA_PIN);
    gpio_init(SSD1306_SCL_PIN);
    gpio_set_function(SSD1306_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SSD1306_SCL_PIN, GPIO_FUNC_I2C);
    // External pullup resistors are required (2.2KΩ to 4.7KΩ recommended)
    // gpio_pull_up(SSD1306_SDA_PIN);
    // gpio_pull_up(SSD1306_SCL_PIN);

    i2c_init(SSD1306_I2C_PORT, SSD1306_I2C_FREQ);
    
    // Initialize mutex for I2C1 bus to prevent collisions between display and IMU
    i2c1_mutex_init();
#endif

#if defined (USE_DISPLAY)
    display_init(&display);
#endif

#if defined (USE_IMU)
    imu_init(); // MPU6050
#endif

    // Start the audio engine.
    // Audio resources and DSP are initialized by core 1.

    // Start the synth
    g_synth.initialize();

    // Initialize the state
    state = get_state();
    set_context(CTX_INIT);
    set_low_batt(false);
    set_preset_slot(-1); // No slot selected
    set_scale_slot(-1);  // No slot selected
    set_preset_has_changes(false);
    set_scale_has_changes(false);

    // Attempt to load previous settings, if stored on flash
    bool data_loaded = load_flash_data();
    if(!data_loaded) {
        // Settings not loaded, initialize state with default values
        set_key(60); // C4
        set_and_extend_scale(SCALE_DEFAULT); // Pentatonic Major - no wrong notes!
        set_scale_unsaved(false);
        set_instrument(0); // Dodepan custom preset
        update_instrument();
        set_chord_mode(CHORD_OFF); // Single notes by default
        set_imu_axes(0x02); // Filter cutoff modulation enabled, pitch bending disabled
        set_volume(VOLUME_DEFAULT);
        set_contrast(CONTRAST_AUTO); // Automatic dimming of display brightness
#if defined (USE_DISPLAY)
        display_update_contrast(&display);
#endif
        // Copy the custom preset to the four user preset slots
        for (uint8_t i = 0; i < NUM_PRESET_SLOTS; i++) {
            for (uint8_t j = 0; j < PROGRAM_PARAMS_NUM; j++) {
                user_presets[i][j] = dodepan_preset[j];
            }
        }
        // Set all user scales to chromatic
        for (uint8_t i = 0; i < NUM_SCALE_SLOTS; i++) {
            for (uint8_t j = 0; j < 12; j++) {
                user_scales[i][j] = j;
            }
        }
    }

#if DODEPAN_DISABLE_PERSISTENCE
    // This build cannot remember volume adjustments: never restore a stale loud level.
    set_volume(0);
#endif

    // Launch the routine on the second core
    multicore_launch_core1(core1_main);

    // Initialize the touch module
    mpr121_i2c_init();

    // Initialize the audio looper with fixed-length buffer
    looper_init(LOOPER_MAX_EVENTS, LOOPER_MAX_SECONDS * 1000);

    // Initialize the arpeggiator
    arpeggiator_init();

    // Initialize the demo player
    demo_init();

    // Initialize the rotary encoder and switch
#if defined (ENCODER_USE_PULLUPS)
    gpio_pull_up(ENCODER_DT_PIN);
    gpio_pull_up(ENCODER_CLK_PIN);
    gpio_pull_up(ENCODER_SWITCH_PIN);
#endif
    rotary_encoder_t *encoder = create_encoder(ENCODER_DT_PIN, ENCODER_CLK_PIN, encoder_onchange);
    button_t *button = create_button(ENCODER_SWITCH_PIN, button_onchange);

    // Falloff values in case the IMU is disabled
    imu_data.deviation_x = 0x2000;  // Center value
    imu_data.deviation_y = 64;      // Center value
    imu_data.acceleration = 127;    // Max value
    imu_data.tilt_zone = 4;         // Center zone (base voicing) for CHORD_TILT mode

    // Use the onboard LED as a power-on indicator
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, 1);
    power_on_alarm_id = add_alarm_in_ms(500, power_on_complete, NULL, true);

    // Initialize the ADC, used for voltage sensing
    adc_init();
    // Launch the battery check timed task
    battery_check_init(5000, NULL, (void*)battery_low_detected);

#if defined (USE_DISPLAY)
    // Show a short intro animation. This will distract the user
    // while the hardware is calibrating
    intro_animation(&display, intro_complete);
    display_draw(&display);
#else
    // Since there's no intro animation without a display,
    // let's trigger the new state manually
    set_context(CTX_SELECTION);
#endif

    while (true) { // Main loop
#if DODEPAN_POP_CAPTURE
        tud_task();
        if(pop_capture_task()){
            static bool capture_stopped=false;
            if(!capture_stopped){
                looper_stop();demo_stop();all_notes_off();
                midi_count=midi_read=midi_write=0;midi_panic_pending=true;
                capture_stopped=true;
            }
            midi_output_task();continue;
        }
#endif
        diagnostics_task();
        mpr121_task();
        synth_set_volume(get_volume());
        persistence_task();
        synth_diagnostics_t queue_status;synth_get_diagnostics(&queue_status);
        static uint32_t observed_overflows,observed_midi_overflows;
        if(queue_status.queue_overflows!=observed_overflows || midi_overflows!=observed_midi_overflows){
            observed_overflows=queue_status.queue_overflows;observed_midi_overflows=midi_overflows;
            looper_stop();demo_stop();all_notes_off();synth_request_suspend(false);
            update_instrument();display_request_refresh();
        }

        // Process deferred encoder events (set from interrupt context)
        if (encoder_direction != 0) {
            uint32_t irq_state=save_and_disable_interrupts();
            int32_t dir = encoder_direction;
            encoder_direction -= dir>0?1:-1;
            restore_interrupts(irq_state);
            dir=dir>0?1:-1;
            //  // Clear flag first to avoid missing events
            if (dir == 1) {
                encoder_down();
            } else if (dir == -1) {
                encoder_up();
            }
        }

        // Process deferred button events (set from interrupt context)
        if (button_pressed) {
            button_pressed = false;
            button_short_press();
        }
        if (button_long_press_pending) {
            button_long_press_pending = false;
            button_long_press();
        }

#if defined (USE_IMU)
        // Run IMU task if axes control is enabled OR if CHORD_TILT mode needs tilt_zone
        // Skip IMU polling during demo playback to reduce I2C bus contention
        // Throttle to 20Hz (50ms) to reduce I2C bus load - human tilt response is ~100-200ms
        if((get_imu_axes() > 0 || get_chord_mode() == CHORD_TILT) && !demo_is_playing() && !DODEPAN_DISABLE_IMU) {
            static uint64_t last_imu_poll_ms = 0;
            uint64_t now_ms = time_us_64() / 1000;
            if (now_ms - last_imu_poll_ms >= 50) {
                last_imu_poll_ms = now_ms;
                static uint8_t prev_tilt_zone = 4;
                imu_task(&imu_data);
                // Only process pitch bend / filter CC when axes control is enabled
                if(get_imu_axes() > 0) {
                    tilt_process();
                }
                // Handle tilt zone changes in CHORD_TILT mode
                if(get_chord_mode() == CHORD_TILT && imu_data.tilt_zone != prev_tilt_zone) {
                    prev_tilt_zone = imu_data.tilt_zone;
                    // Update rolling chords for any held pads
                    tilt_chord_update();
                    // Request display refresh if on chord mode screen
                    if(get_selection() == SELECTION_CHORD) {
                        display_request_refresh();
                    }
                }
            }
        }
#endif

#if defined (USE_DISPLAY)
        // Process any deferred display updates (when I2C was busy during touch/encoder events)
        // Skip display updates during demo playback if DEBUG_DEMO_NO_DISPLAY is defined
#ifdef DEBUG_DEMO_NO_DISPLAY
        if(display_is_pending() && !demo_is_playing() && !DODEPAN_DISABLE_IMU) {
            display_draw(&display);
        }
#else
        if(display_is_pending()) {
            display_draw(&display);
        }
#endif
#endif

        looper_task();
        arpeggiator_task();
        demo_task();
#if defined (USE_MIDI)
        tud_task(); // tinyusb device task
        midi_output_task();
#endif


    }
}
