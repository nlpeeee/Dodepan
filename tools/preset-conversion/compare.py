from pathlib import Path
import subprocess,shutil,sys,wave,json
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools/audit'))
from build_baseline import vs_environment
OUT=ROOT/'build-audit/preset-conversion';env,_,_=vs_environment();cl=shutil.which('cl.exe',path=env['PATH'])
CPP=r"""
#define _CRT_SECURE_NO_WARNINGS
#include <cstdint>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cassert>
#include <algorithm>
#include <cstring>
#include <variant>
#include <type_traits>
#define __attribute__(x)
#define __not_in_flash_func(x) x
using boolean=bool;using byte=uint8_t;
#include "config.h"
#include "synth_backend.h"
const uint8_t *patches[]={dodepan_preset,magic_bell_preset,space_piano_preset,robot_voice_preset,synthwave_preset,bleep_bloop_preset};
int16_t sample(DodepanEngine &e){int16_t right;
#if DODEPAN_ENGINE_218
return e.process(0,0,right);
#else
return e.process(right);
#endif
}
int main(){
#if DODEPAN_ENGINE_218
 assert(INSTRUMENT_COUNT==26);
 for(int id=0;id<256;++id){assert(instrument_is_user(id)==(id>=14 && id<=17));uint8_t p=instrument_factory_program(id);if(id>=6 && id<=13)assert(p==id+10);else if(id>=18 && id<=25)assert(p==id+6);else assert(p==255);}
 auto ref=new DodepanEngine;ref->initialize();auto e=new DodepanEngine;e->initialize();
 for(int p=16;p<=31;++p){
  backend_custom(*e,p%6);backend_program(*e,p);ref->program_change(p);
  for(auto cc:s_program_table_parameters)assert(e->current_controller_value(cc)==ref->current_controller_value(cc));
 }
 for(int slot=0;slot<4;++slot){
  uint8_t guarded[50],out[48];guarded[0]=0xa5;guarded[49]=0x5a;
  for(int i=0;i<48;++i)guarded[i+1]=(i*17+slot*29)&127;
  uint8_t backup[50];memcpy(backup,guarded,50);convert_legacy_preset(guarded+1,out);backend_preset(*e,guarded+1);
  assert(memcmp(guarded,backup,50)==0);for(int i=0;i<48;++i)assert(e->current_controller_value(dodepan_program_parameters[i])==out[i]);
 }
 delete e;delete ref;
#endif
 FILE*params=fopen("parameters.csv","w");
 for(int patch=0;patch<6;++patch){
  auto e=new DodepanEngine;e->initialize();
#if DODEPAN_ENGINE_218
  backend_custom(*e,patch);
  for(int i=0;i<48;++i)assert(e->current_controller_value(dodepan_program_parameters[i])==converted_custom_presets[patch][i]);
#else
  backend_preset(*e,patches[patch]);e->control_change(DELAY_TIME,127);e->control_change(DELAY_FEEDBACK,0);e->control_change(DELAY_MODE,1);
#endif
  for(int i=0;i<48;++i)fprintf(params,"%s%u",i?",":"",e->current_controller_value(dodepan_program_parameters[i]));fprintf(params,"\n");
#if DODEPAN_ENGINE_218
  uint8_t snapshot[128];for(int i=0;i<128;++i)snapshot[i]=e->current_controller_value(i);
  backend_program(*e,21);backend_custom(*e,patch);for(auto cc:s_program_table_parameters)assert(snapshot[cc]==e->current_controller_value(cc));
#endif
  char name[40];sprintf(name,"custom-%d.pcm",patch);FILE*f=fopen(name,"wb");assert(f);uint64_t energy=0;
  int32_t gain=0;
  // Nine single notes: pitches 48/60/84, each at velocities 48/100/127.
  // Then repeated notes, four-note chord, fifth-note replacement and release tails.
  for(unsigned frame=0;frame<SYNTH_RENDER_FREQUENCY*18;++frame){
   unsigned sr=SYNTH_RENDER_FREQUENCY,sec=frame/sr,within=frame%sr;
   if(sec<9){int note=48+(sec/3==1?12:sec/3==2?36:0);int vel=sec%3==0?48:sec%3==1?100:127;
    if(within==0)e->note_on(note,vel);if(within==sr/2)backend_off(*e,note);
   }
   if(sec>=9 && sec<11){if(within%(sr/4)==0)e->note_on(60,100);if(within%(sr/4)==sr/8)backend_off(*e,60);}
   if(frame==sr*11){e->control_change(VOICE_MODE,VOICE_POLYPHONIC);for(int n:{60,64,67,71})e->note_on(n,100);}
   if(frame==sr*12)e->note_on(74,100);
   if(frame==sr*13)for(int n:{60,64,67,71,74})backend_off(*e,n);
   gain+=std::min<int32_t>(32768-gain,SYNTH_GAIN_STEP);
   int16_t v=int32_t(sample(*e))*gain/32768;energy+=int64_t(v)*v;fwrite(&v,2,1,f);
  }
  assert(energy>0);assert(e->m_note_on_total_count==0);fclose(f);delete e;
 }
 fclose(params);puts("PASS: renders and balanced notes; native build also checks factory identity, mapping, slot safety and deterministic controls");
}
"""
(OUT/'compare.cpp').write_text(CPP)
for mode in [0,1]:
 d=OUT/f'compare-{mode}';d.mkdir(exist_ok=True)
 probe=OUT/f'probe-{mode}'
 cmd=[cl,'/nologo','/O2','/EHsc','/std:c++17',f'/DDODEPAN_ENGINE_218={mode}','/DPRA32_U2_ENABLE_POLY_ON_1_CORE','/DAUDIO_BUFFER_LENGTH=256','/I'+str(ROOT),'/I'+str(probe),str(OUT/'compare.cpp'),'/Fe:'+str(d/'compare.exe'),'/Fo:'+str(d/'compare.obj')]
 a=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(d/'compile.log').write_bytes(a.stdout);a.check_returncode()
 a=subprocess.run([str(d/'compare.exe')],cwd=d,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(d/'test.log').write_bytes(a.stdout);a.check_returncode()
 for raw in d.glob('custom-*.pcm'):
  with wave.open(str(raw.with_suffix('.wav')),'wb') as w:w.setnchannels(1);w.setsampwidth(2);w.setframerate(48000 if mode else 24000);w.writeframes(raw.read_bytes())
 print(f'Mode {mode}: '+a.stdout.decode().strip())
