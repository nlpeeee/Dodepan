"""Exercise exact control and queue function bodies; hardware sinks are substituted."""
import sys,pathlib,re,shutil,subprocess
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'audit'))
from build_baseline import ROOT,OUT,vs_environment
from run_main_probes import extract
out=OUT/'repair-tests';out.mkdir(parents=True,exist_ok=True)
env,_,_=vs_environment()
source=(ROOT/'main.cpp').read_text()
prefix=r'''
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include "config.h"
#include "instrument_preset.h"
uint8_t **user_presets,**user_scales;
#include "state.c"
#include "note_router.c"
static int active[128],ons,offs,record_on,record_off,failed;
void note_sink_on(uint8_t n,uint8_t){active[n]++;ons++;}
void note_sink_off(uint8_t n){active[n]--;offs++;}
void note_sink_cc(uint8_t,uint8_t){}
void display_request_refresh(){}
static bool arp_enabled;
static int arp_releases;
bool arpeggiator_is_enabled(){return arp_enabled;}
void arpeggiator_pad_on(uint8_t,uint8_t){}
void arpeggiator_pad_off(uint8_t){arp_releases++;}
void arpeggiator_stop(){}
void looper_record_note(uint8_t,uint8_t,bool on){if(on)record_on++;else record_off++;}
struct {uint8_t tilt_zone,acceleration;} imu_data;
struct {void control_change(uint8_t,uint8_t){} void all_notes_off(){}} g_synth;
void note_off(uint8_t);
void check(const char *n,bool ok){printf("%s %s\n",ok?"PASS":"FAIL",n);failed+=!ok;}
'''
globals=source[source.index('// Chord interval definitions'):source.index('static inline uint32_t tudi_midi_write24')]
# Only chord tables and tracking arrays are needed, not the USB transport globals.
globals=globals[:globals.index('// Core-0-only USB')]
names=['get_note_by_id','make_chord','note_on','note_off','tilt_chord_update','touch_on','touch_off','release_live_notes','all_notes_off']
bodies='\n'.join(extract(source.replace('static uint8_t make_chord','uint8_t make_chord'),n) for n in names)
tests=r'''
int main(){
 set_key(60);for(int i=0;i<12;i++)set_extended_scale(i,i);imu_data.acceleration=100;imu_data.tilt_zone=4;
 set_chord_mode(CHORD_OFF);touch_on(0);arp_enabled=true;touch_off(0);
 check("held live release retains original owner",active[60]==0);
 touch_on(1);arp_enabled=false;touch_off(1);check("held arp release retains original owner",arp_releases==1);
 set_chord_mode(CHORD_TILT);note_on(0,100);
 for(int z=0;z<9;z++){imu_data.tilt_zone=z;tilt_chord_update();check("tilt preserves held root",active[60]==1);}
 note_off(0);check("tilt notes and recorded events balance",!notes_any_active()&&ons==offs&&record_on==record_off);
 set_chord_mode(CHORD_OFF);touch_on(0);release_live_notes();set_key(72);touch_off(0);
 check("held mode change releases without new onset",!notes_any_active()&&active[72]==0);
 set_key(99);for(int i=0;i<12;i++)set_extended_scale(i,24);set_chord_mode(CHORD_TILT);imu_data.tilt_zone=8;touch_on(11);touch_off(11);
 check("upper pitch chord remains balanced",!notes_any_active()&&ons==offs);
 return failed?1:0;
}
'''
queue=(ROOT/'synth_service.cpp').read_text();queue=queue[queue.index('SynthControl g_synth;'):re.search(r'^void (?:__not_in_flash_func\(synth_audio_main\)|synth_audio_main)\(\)',queue,re.M).start()]
queue_prefix=r'''
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <cstdio>
#include "config.h"
#include "synth_service.h"
using critical_section_t=int;
void critical_section_init(int*){} void critical_section_enter_blocking(int*){} void critical_section_exit(int*){}
struct PRA32_U2_Synth {
 uint8_t values[128]={};int on=0,off=0,panics=0;
 void note_on(uint8_t,uint8_t){on++;}void note_off(uint8_t){off++;}
 void control_change(uint8_t c,uint8_t v){values[c]=v;}
 void pitch_bend(uint8_t,uint8_t){}void program_change(uint8_t){}
 void all_notes_off(){panics++;}uint8_t current_controller_value(uint8_t c){return values[c];}
};
static int failed;
void check(const char *n,bool ok){printf("%s %s\n",ok?"PASS":"FAIL",n);failed+=!ok;}
'''
queue_tests=r'''
int main(){
 g_synth.initialize();g_synth.control_change(74,1);g_synth.control_change(74,2);
 check("adjacent CC coalescing",count==1);
 g_synth.note_on(60,100);g_synth.control_change(74,3);check("CC cannot cross note boundary",count==3);
 process_commands();check("applied mirror follows consumption",g_synth.current_controller_value(74)==3&&engine.on==1);
 uint8_t preset[48];for(int i=0;i<48;i++)preset[i]=i;
 for(int i=45;i<48;i++)engine.control_change(dodepan_program_parameters[i],100+i-45);
 g_synth.apply_preset(preset);g_synth.capture_preset(3);process_commands();uint8_t slot,copy[48];
 check("legacy preset application and full capture",synth_take_capture(&slot,copy)&&slot==3&&!memcmp(copy,preset,45)&&copy[45]==100&&copy[46]==101&&copy[47]==102);
 for(int i=0;i<257;i++)g_synth.note_on(i%128,100);
 check("overflow requests explicit panic",stats.queue_overflows==1&&count==0&&panic_requested);
 int before=engine.panics;process_commands();check("overflow panic reaches engine",engine.panics==before+1&&!panic_requested);
 for(int i=0;i<256;i++)g_synth.note_off(i%128);process_commands();check("bounded per-buffer event budget",count==224);
 while(count)process_commands();check("all queued note-offs delivered",engine.off==256);
 return failed?1:0;
}
'''
touch=(ROOT/'touch.c').read_text();touch=touch[touch.index('// Fixed-time'):]
touch_prefix=r'''
#include <cstdint>
#include <cstdio>
#include "config.h"
#undef MPR121_I2C_PORT
#define MPR121_I2C_PORT 0
static uint64_t now;
static uint16_t raw;
static bool bus_ok=true;
static int ons,offs,failed;
uint64_t time_us_64(){return now;}
int i2c_write_timeout_us(int,int,uint8_t*,int,bool,int){return bus_ok?1:-1;}
int i2c_read_timeout_us(int,int,uint8_t *p,int,bool,int){p[0]=raw&255;p[1]=raw>>8;return bus_ok?2:-1;}
void touch_on(uint8_t){ons++;}void touch_off(uint8_t){offs++;}
void check(const char *n,bool ok){printf("%s %s\n",ok?"PASS":"FAIL",n);failed+=!ok;}
'''
touch_tests=r'''
int main(){
 now=1000000;mpr121_task();now+=500000;raw=1;mpr121_task();check("touch first onset",ons==1&&touch_any_held());
 raw=0;now+=2000;mpr121_task();now+=39000;mpr121_task();check("release holds until elapsed threshold",offs==0);
 now+=2000;mpr121_task();check("release after fixed threshold",offs==1&&!touch_any_held());
 raw=2;now+=2000;mpr121_task();bus_ok=false;now+=20000;mpr121_task();check("short bus fault retains held note",offs==1);
 now+=100000;mpr121_task();now+=41000;mpr121_task();check("persistent bus fault releases note",offs==2&&!touch_any_held());
 return failed?1:0;
}
'''
for name,content in [('controls',prefix+globals+bodies+tests),('queue',queue_prefix+queue+queue_tests),('touch',touch_prefix+touch+touch_tests)]:
 p=out/(name+'.cpp');p.write_text(content)
 cmd=[shutil.which('cl.exe',path=env['PATH']),'/nologo','/std:c++17','/W3','/Od','/DBOARD_IS_PICO2','/I'+str(ROOT/'tools/audit/stubs'),'/I'+str(ROOT),'/I'+str(ROOT/'lib/digital-synth-pra32-u2/Digital-Synth-PRA32-U2'),str(p),'/Fe:'+str(out/(name+'.exe')),'/Fo:'+str(out/(name+'.obj'))]
 c=subprocess.run(cmd,env=env,cwd=out,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(out/(name+'-compile.log')).write_text(c.stdout)
 if c.returncode:print(c.stdout);sys.exit(c.returncode)
 r=subprocess.run([str(out/(name+'.exe'))],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(out/(name+'.log')).write_text(r.stdout);print(r.stdout)
 if r.returncode:sys.exit(r.returncode)

