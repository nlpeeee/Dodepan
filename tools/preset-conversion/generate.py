"""Derive legal controller mappings from the actual pinned engine implementations."""
from pathlib import Path
import sys,subprocess,shutil,re,json,math
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools/audit'))
from build_baseline import vs_environment
OUT=ROOT/'build-audit/preset-conversion';OUT.mkdir(exist_ok=True)
OLD=ROOT/'lib/digital-synth-pra32-u2/Digital-Synth-PRA32-U2';NEW=ROOT/'lib/pra32-u2-v2.18.2/Digital-Synth-PRA32-U2'
CPP=r"""
#define _CRT_SECURE_NO_WARNINGS
#include <stdint.h>
#include <climits>
#include <cmath>
#include <cstdio>
#include <algorithm>
#include <cstring>
#include <variant>
#include <type_traits>
using boolean=bool;using byte=uint8_t;
static uint8_t g_midi_ch=0;
#define __attribute__(x)
#define __not_in_flash_func(x) x
#include "pra32-u2-synth.h"
static double freq(PRA32_U2_Filter &f){
 double a1=f.m_a_1_over_a_0/double(1ULL<<30),a2=f.m_a_2_over_a_0/double(1ULL<<30);
 return acos(std::clamp(-a1/(1+a2),-1.,1.))*RATE/(2*3.141592653589793);
}
int main(){
 for(unsigned v=0;v<128;++v){
  PRA32_U2_EG eg;eg.set_attack(v);eg.set_decay(v);eg.set_release(v);
  PRA32_U2_LFO lfo;lfo.set_lfo_rate(v);lfo.set_lfo_fade_time(v);
  PRA32_U2_ChorusFx ch;ch.set_chorus_rate(v);ch.set_chorus_depth(v);ch.set_chorus_delay_time(v);
  PRA32_U2_DelayFx delay;delay.set_delay_time(v);
  PRA32_U2_Osc osc;osc.set_portamento<0>(v);
  PRA32_U2_Amp amp;amp.set_gain(v);for(int i=0;i<256;++i)amp.process_at_low_rate(16384);
  double amp_gain=amp.process(1<<20)/double(1<<20);
#if NEW_ENGINE
  PRA32_U2_Panner pan;pan.set_pan(64);for(int i=0;i<256;++i)pan.process_at_low_rate();int32_t pr; amp_gain*=pan.process(1<<20,pr)/double(1<<20);
#else
  amp_gain*=2;
#endif
  PRA32_U2_Filter filter;filter.set_cutoff(v);filter.set_cutoff_pitch_amt(64);
  for(int i=0;i<2048;++i)filter.process_at_low_rate(i,0,0,60<<8);
  printf("%u,%d,%d,%d,%u,%u,%u,%u,%u,%u,%u,%.12g,%.12g\n",v,eg.m_attack_coef,eg.m_decay_coef,eg.m_release_coef,
   unsigned(lfo.m_lfo_rate),unsigned(lfo.m_lfo_fade_coef),unsigned(ch.m_chorus_rate_control),
   unsigned(ch.m_chorus_depth_control),unsigned(ch.m_chorus_delay_time_control),unsigned(delay.m_delay_time),unsigned(osc.m_portamento_coef[0]),freq(filter),amp_gain);
 }
#if NEW_ENGINE
 FILE*f=fopen("filter-grid.csv","w");
 for(unsigned key=0;key<128;++key)for(unsigned v=0;v<128;++v){
  PRA32_U2_Filter filter;filter.set_cutoff(v);filter.set_cutoff_pitch_amt(key);
  for(int i=0;i<2048;++i)filter.process_at_low_rate(i,0,0,48<<8);
  fprintf(f,"%u,%u,%.12g\n",key,v,freq(filter));
 }
 fclose(f);
#endif
}
"""
env,_,_=vs_environment();cl=shutil.which('cl.exe',path=env['PATH'])
metrics=[]
for mode,engine,rate in [(0,OLD,24000),(1,NEW,48000)]:
 d=OUT/f'probe-{mode}';d.mkdir(exist_ok=True)
 for f in engine.glob('*.h'):
  s=f.read_text();s=re.sub(r'(class PRA32_U2_\w+\s*\{)',r'\1\npublic:',s);s=s.replace('private:','public:')
  (d/f.name).write_text(s)
 (d/'probe.cpp').write_text(CPP)
 cmd=[cl,'/nologo','/O2','/EHsc','/std:c++17',f'/DNEW_ENGINE={mode}',f'/DRATE={rate}','/DPRA32_U2_ENABLE_POLY_ON_1_CORE','/I'+str(d),str(d/'probe.cpp'),'/Fe:'+str(d/'probe.exe'),'/Fo:'+str(d/'probe.obj')]
 result=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(d/'compile.log').write_bytes(result.stdout);result.check_returncode()
 result=subprocess.run([str(d/'probe.exe')],cwd=d,stdout=subprocess.PIPE,check=True);(d/'metrics.csv').write_bytes(result.stdout)
 metrics.append([[float(x) for x in line.split(',')] for line in result.stdout.decode().splitlines()])
old,new=metrics
assert old[100][11]>old[32][11]*10 and new[100][11]>new[32][11]*10, "Filter must settle through full control path"
# Exponential recurrence q per quarter-rate update: equal seconds => q_new=sqrt(q_old).
def tau(q,rate):
 q=q/2**30
 if q>=1:return 1e12
 if q<=0:return 0.
 return -4/rate/math.log(q)
def distance(a,b):
 if a==0 or b==0:return 0 if a==b else 1e9+abs(a-b)
 return abs(math.log(b/a))
def mapping(a,b):return [min(range(128),key=lambda j:(distance(x,b[j]),j)) for x in a]
maps={};physical={}
for name,col,kind in [('attack',1,'tau'),('decay',2,'tau'),('release',3,'tau'),('lfo_rate',4,'phase'),('lfo_fade',5,'fade'),('chorus_rate',6,'phase'),('chorus_depth',7,'samples'),('chorus_delay',8,'samples'),('delay_time',9,'delay'),('portamento',10,'tau')]:
 vals=[]
 for rows,rate in [(old,24000),(new,48000)]:
  raw=[row[col] for row in rows]
  if kind=='tau':raw=[tau(x,rate) for x in raw]
  elif kind=='phase':raw=[x*(rate/4)/2**24 for x in raw]
  elif kind=='fade':raw=[(x+(1 if rate==48000 else 0))*128*4/rate if x>1 else 0 for x in raw]
  elif kind=='samples':raw=[x/16/rate for x in raw]
  elif kind=='delay':raw=[x/rate for x in raw]
  vals.append(raw)
 maps[name]=mapping(*vals);physical[name]={'old':vals[0],'new':vals[1]}
# Filter mapping accounts for the new engine's -12-semitone coarse tuning affecting key tracking.
grid=[[0.]*128 for _ in range(128)]
for line in (OUT/'probe-1/filter-grid.csv').read_text().splitlines():
 k,v,f=line.split(',');grid[int(k)][int(v)]=float(f)
filter_maps=[mapping([row[11] for row in old],grid[k]) for k in range(128)]
# Old chorus is a crossfade: wet/dry = m/(128-m); new chorus is additive with wet=level/64.
maps['amp_gain']=mapping([row[12] for row in old],[row[12] for row in new])
physical['amp_gain']={'old':[row[12] for row in old],'new':[row[12] for row in new]}
maps['chorus_mix']=[min(range(128),key=lambda j:(abs(((j+1)//2)/64-((v+1)//2)/(128-(v+1)//2)),j)) for v in range(128)]
header='#pragma once\n#include <stdint.h>\n// Generated by tools/preset-conversion/generate.py from pinned engine code.\n'
for name,values in maps.items():header+='static const uint8_t preset_map_'+name+'[128]={'+','.join(map(str,values))+'};\n'
header+='static const uint8_t preset_map_gain_by_chorus[128][128]={\n'+''.join('{'+','.join(map(str,mapping([row[12]*(1-((mix+1)//2)/128) for row in old],[row[12] for row in new])))+'},\n' for mix in range(128))+'};\n'
header+='static const uint8_t preset_map_filter[128][128]={\n'+''.join('{'+','.join(map(str,v))+'},\n' for v in filter_maps)+'};\n'
(ROOT/'preset_conversion_tables.h').write_text(header)
(OUT/'physical-mappings.json').write_text(json.dumps({'maps':maps,'physical':physical,'old_filter_hz':[row[11] for row in old],'new_filter_hz_by_keytrack':grid},indent=2))
print('Generated legal-value conversion tables from old/new coefficient and timing probes.')
