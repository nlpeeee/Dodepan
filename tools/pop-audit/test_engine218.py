from pathlib import Path
import sys,shutil,subprocess,json,hashlib,wave
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools/audit'))
from build_baseline import vs_environment
OUT=ROOT/'build-audit/engine218-tests';OUT.mkdir(exist_ok=True)
NEW=ROOT/'lib/pra32-u2-v2.18.2/Digital-Synth-PRA32-U2';OLD=ROOT/'lib/digital-synth-pra32-u2/Digital-Synth-PRA32-U2'
# Expose state only in a generated test copy; DSP source and calculations unchanged.
s=(NEW/'pra32-u2-synth.h').read_text();assert 'class PRA32_U2_Synth {' in s
(OUT/'pra32-u2-synth.h').write_text(s.replace('class PRA32_U2_Synth {','class PRA32_U2_Synth {\npublic:',1))
(OUT/'test_backend.h').write_bytes((ROOT/'synth_backend.h').read_bytes())
source=r"""
#define _CRT_SECURE_NO_WARNINGS
#include <cstdint>
#include <climits>
#include <cstdio>
#include <cassert>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <variant>
#include <type_traits>
using boolean=bool;using byte=uint8_t;
#define __attribute__(x)
#define __not_in_flash_func(x) x
#include "config.h"
#include "test_backend.h"
const uint8_t *custom[]={dodepan_preset,magic_bell_preset,space_piano_preset,robot_voice_preset,synthwave_preset,bleep_bloop_preset};
int main(){
 static_assert(SYNTH_RENDER_FREQUENCY==48000 && AUDIO_TRANSPORT_REPEAT==1);
 for(unsigned patch=0;patch<14;++patch){
  auto e=new DodepanEngine;e->initialize();
  const uint8_t *values=patch<6?custom[patch]:legacy_factory_presets[patch-6];
  if(patch<6)backend_preset(*e,values);else backend_program(*e,patch-6);
  for(unsigned i=0;i<48;++i)assert(e->current_controller_value(dodepan_program_parameters[i])==values[i]);
  // Switching from arbitrary controller state must restore the same full patch.
  uint8_t initial[128];for(unsigned i=0;i<128;++i)initial[i]=e->current_controller_value(i);
  e->control_change(STRETCH_TUNE,100);e->control_change(DELAY_TIME,12);e->control_change(OSC_DRIFT,99);
  backend_preset(*e,values);
  for(unsigned i=0;i<128;++i)assert(e->current_controller_value(i)==initial[i]);
  e->control_change(VOICE_MODE,VOICE_POLYPHONIC);
  for(unsigned n=60;n<64;++n)e->note_on(n,100);
  assert(e->m_note_on_total_count==4);unsigned voices=0;
  for(unsigned i=0;i<4;++i)voices+=e->m_note_on_number[i]!=NOTE_NUMBER_INVALID;
  assert(voices==4);
  e->note_on(67,100);voices=0;for(unsigned i=0;i<4;++i)voices+=e->m_note_on_number[i]!=NOTE_NUMBER_INVALID;assert(voices==4);
  for(unsigned n=60;n<64;++n)backend_off(*e,n);backend_off(*e,67);assert(e->m_note_on_total_count==0);
  delete e;e=new DodepanEngine;e->initialize();backend_preset(*e,values);
  char name[40];sprintf(name,"instrument-%02u.pcm",patch);FILE*f=fopen(name,"wb");assert(f);
  uint64_t energy=0;int16_t right;int peak=0;
  for(unsigned frame=0;frame<48000*7;++frame){
   if(frame==0)e->note_on(48,100);if(frame==24000)backend_off(*e,48);
   if(frame==48000)e->note_on(60,100);if(frame==72000)backend_off(*e,60);
   if(frame==96000)e->note_on(84,100);if(frame==120000)backend_off(*e,84);
   if(frame==144000){e->control_change(VOICE_MODE,VOICE_POLYPHONIC);for(int n:{60,64,67,71})e->note_on(n,100);}
   if(frame==192000)for(int n:{60,64,67,71})backend_off(*e,n);
   int16_t sample=e->process(0,0,right);fwrite(&sample,2,1,f);energy+=int64_t(sample)*sample;peak=std::max(peak,std::abs(int(sample)));
  }
  fclose(f);assert(energy>0);printf("%u peak=%d rms=%.1f\n",patch,peak,std::sqrt(double(energy)/(48000*7)));delete e;
 }
 puts("PASS: all 14 instruments, complete preset initialization, four voices/fifth-note replacement, balanced releases, native rendering");
}
"""
(OUT/'test.cpp').write_text(source)
env,_,_=vs_environment();cl=shutil.which('cl.exe',path=env['PATH'])
cmd=[cl,'/nologo','/std:c++17','/O2','/EHsc','/DDODEPAN_ENGINE_218=1','/DPRA32_U2_ENABLE_POLY_ON_1_CORE','/DAUDIO_BUFFER_LENGTH=256','/I'+str(OUT),'/I'+str(ROOT),'/I'+str(NEW),str(OUT/'test.cpp'),'/Fe:'+str(OUT/'test.exe'),'/Fo:'+str(OUT/'test.obj')]
r=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(OUT/'compile.log').write_bytes(r.stdout);r.check_returncode()
r=subprocess.run([str(OUT/'test.exe')],cwd=OUT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(OUT/'test.log').write_bytes(r.stdout);r.check_returncode()
for raw in OUT.glob('instrument-*.pcm'):
 with wave.open(str(raw.with_suffix('.wav')),'wb') as w:w.setnchannels(1);w.setsampwidth(2);w.setframerate(48000);w.writeframes(raw.read_bytes())
assert (ROOT/'instrument_preset.h').read_bytes()==(ROOT/'build-audit/engine218-baseline/instrument_preset.h').read_bytes()
up=ROOT/'build-audit/upstream-v2.18.2/Digital-Synth-PRA32-U2'
assert all((NEW/p.relative_to(up)).read_bytes()==p.read_bytes() for p in up.rglob('*') if p.is_file())
print(r.stdout.decode().splitlines()[-1]);print('PASS: upstream DSP and Dodepan preset bytes unchanged; 14 comparison WAVs generated')
