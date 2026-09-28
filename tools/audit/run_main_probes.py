"""Exercise exact main.cpp function bodies with state.c and hardware/synth sinks.
Generated source stays in build-audit. Sink tests verify event routing, not DSP.
"""
import pathlib,re,shutil,subprocess,sys
from build_baseline import ROOT,OUT,DOC,vs_environment

def extract(text,name):
    m=re.search(r'^(?:static inline |extern "C" )?(?:void|bool|uint8_t) '+name+r'\([^\n]*\)\s*\{',text,re.M)
    if not m:raise ValueError(name)
    end=m.end();depth=1
    while depth:
        if text[end]=='{':depth+=1
        if text[end]=='}':depth-=1
        end+=1
    return text[m.start():end]+'\n'

def main():
    env,_,_=vs_environment();target=OUT/'host';target.mkdir(parents=True,exist_ok=True)
    text=(ROOT/'main.cpp').read_text()
    prefix=r'''
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include "config.h"
#include "instrument_preset.h"
uint8_t **user_presets, **user_scales;
#include "state.c"
#include "display/display.h"
#undef FLASH_TARGET_OFFSET
#define FLASH_TARGET_OFFSET 0
static uint8_t stored_flash[256];
#define XIP_BASE reinterpret_cast<uintptr_t>(stored_flash)
static ssd1306_t display;
void display_update_contrast(ssd1306_t*) {}
void display_request_refresh() {}
void sleep_us(uint32_t) {}
static int notes[128],midi_notes[128],recorded_on,recorded_off;
static bool arp_enabled;
static uint8_t synth_values[256];
struct SynthSink {
 void note_on(uint8_t n,uint8_t) { if(n<128)notes[n]++; }
 void note_off(uint8_t n) { if(n<128 && notes[n])notes[n]--; }
 void all_notes_off() { std::memset(notes,0,sizeof(notes)); }
 void control_change(uint8_t cc,uint8_t v) { synth_values[cc]=v; }
 uint8_t current_controller_value(uint8_t cc) { return synth_values[cc]; }
 void program_change(uint8_t) {}
} g_synth;
static struct {uint8_t tilt_zone,acceleration;} imu_data;
uint32_t tudi_midi_write24(uint8_t,uint8_t status,uint8_t n,uint8_t) {
 if(n<128) { if(status==0x90)midi_notes[n]++;if(status==0x80 && midi_notes[n])midi_notes[n]--; }return 3;
}
void play_single_note(uint8_t n,uint8_t v) {g_synth.note_on(n,v);tudi_midi_write24(0,0x90,n,v);}
void stop_single_note(uint8_t n) {g_synth.note_off(n);tudi_midi_write24(0,0x80,n,0);}
void looper_record_note(uint8_t,uint8_t,bool on) {if(on)recorded_on++;else recorded_off++;}
void arpeggiator_stop() {}
bool arpeggiator_is_enabled(){return arp_enabled;}
void arpeggiator_pad_on(uint8_t,uint8_t) {}
void arpeggiator_pad_off(uint8_t) {}
void request_flash_write() {}
'''
    state_source=(ROOT/'state.c').read_text()
    # Host C++ enums differ from ARM -fshort-enums; omit unused context/menu
    # functions instead of changing the state/preset functions under test.
    for unused in ['get_context','get_selection','set_selection_up','set_selection_down']:
        state_source=state_source.replace(extract(state_source,unused).rstrip(), '')
    prefix=prefix.replace('#include "state.c"',state_source)
    globals=text[text.index('// Chord interval definitions'):text.index('static const struct sound_i2s_config')]
    names=['get_argument_from_parameter','load_user_preset','update_instrument','load_flash_data','submit_preset_slot','get_note_by_id','note_on','note_off','tilt_chord_update','touch_on','touch_off','all_notes_off']
    bodies='\n'.join(extract(text,n) for n in names)
    tests=r'''
static int checks,failures;
static void check(const char *id,bool ok,const char *msg) {
 checks++;if(!ok)failures++;printf("%s %s: %s\n",ok?"PASS":"FAIL",id,msg);
}
static void reset() {
 std::memset(notes,0,sizeof(notes));std::memset(midi_notes,0,sizeof(midi_notes));
 std::memset(active_chord_count,0,sizeof(active_chord_count));std::memset(pad_held,0,sizeof(pad_held));
 last_tilt_voicing=0xff;recorded_on=recorded_off=0;arp_enabled=false;
 set_key(60);for(int i=0;i<12;i++)set_extended_scale(i,i);
 set_chord_mode(CHORD_OFF);imu_data.tilt_zone=4;imu_data.acceleration=100;
}
int main() {
 // Two extra sentinel slots expose invalid indexing without dereferencing unmapped memory.
 static uint8_t backing[6][PROGRAM_PARAMS_NUM], scales_backing[4][12];
 static uint8_t *slots[6],*scales_slots[4];
 for(int i=0;i<6;i++){slots[i]=backing[i];std::memset(backing[i],10*(i+1),PROGRAM_PARAMS_NUM);}
 for(int i=0;i<4;i++)scales_slots[i]=scales_backing[i];user_presets=slots;user_scales=scales_slots;
 reset();load_user_preset(14);
 check("preset-slot-first",synth_values[dodepan_program_parameters[0]]==10,"instrument 14 must load user slot 0");
 load_user_preset(16);
 check("preset-slot-third",synth_values[dodepan_program_parameters[0]]==30,"instrument 16 must load valid user slot 2, not sentinel slot 4");
 set_instrument(15);set_instrument_up();
 check("preset-menu-range",get_instrument()==16,"all four user instruments must be reachable");
 set_preset_slot(0);submit_preset_slot();
 check("preset-store-selection",get_instrument()==14,"saving slot 0 selects user instrument 14");
 reset();touch_on(0);arp_enabled=true;touch_off(0);
 check("held-mode-routing",notes[60]==0,"release must reach the owner that handled the press");
 reset();set_chord_mode(CHORD_TILT);note_on(0,100);imu_data.tilt_zone=0;tilt_chord_update();note_off(0);
 int remaining=0,midi_remaining=0;for(int i=0;i<128;i++){remaining+=notes[i];midi_remaining+=midi_notes[i];}
 check("tilt-release",remaining==0,"tilt transition balances every synth note-on");
 check("tilt-midi-release",midi_remaining==0,"tilt transition balances every external MIDI note-on");
 check("tilt-looper-release",recorded_on==recorded_off,"tilt transition records matching looper releases");
 reset();set_chord_mode(CHORD_TILT);note_on(0,100);all_notes_off();imu_data.tilt_zone=0;tilt_chord_update();
 remaining=0;for(int i=0;i<128;i++)remaining+=notes[i];
 check("panic-tilt-tracking",remaining==0,"panic clears held tilt pads so movement cannot restart notes");
 reset();note_on(0,100);all_notes_off();
 check("panic-midi",midi_notes[60]==0,"panic releases notes previously sent to external MIDI");
 std::memset(stored_flash,0,sizeof(stored_flash));const uint8_t magic[]=MAGIC_NUMBER;
 std::memcpy(stored_flash,magic,4);stored_flash[4]=60;stored_flash[8]=8;
 stored_flash[16+NUM_PRESET_SLOTS*PROGRAM_PARAMS_NUM]=255;
 bool accepted=load_flash_data();
 check("flash-scale-validation",!accepted,"invalid persisted scale degree is rejected before applying state");
 printf("RESULT %d checks, %d failures\n",checks,failures);return failures?1:0;
}
'''
    generated=target/'main_probes.cpp';generated.write_text(prefix+globals+bodies+tests)
    command=[shutil.which('cl.exe',path=env['PATH']),'/nologo','/std:c++17','/W3','/Od','/I'+str(ROOT/'tools/audit/stubs'),'/I'+str(ROOT),'/I'+str(ROOT/'lib/digital-synth-pra32-u2/Digital-Synth-PRA32-U2'),str(generated),'/Fe:'+str(target/'main_probes.exe'),'/Fo:'+str(target/'main_probes.obj')]
    result=subprocess.run(command,env=env,cwd=target,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    (DOC/'main-compile.log').write_text(result.stdout)
    if result.returncode:print(result.stdout);return result.returncode
    result=subprocess.run([str(target/'main_probes.exe')],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    (DOC/'main-probes.log').write_text(result.stdout);print(result.stdout);return result.returncode
if __name__=='__main__':sys.exit(main())
