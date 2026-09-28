"""Validate the actual CPU readout functions against controlled render counters."""
import pathlib,sys,subprocess,shutil
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'audit'))
from build_baseline import ROOT,OUT,vs_environment
import re
def extract(text,name):
    m=re.search(r'^extern "C" (?:uint32_t|void) '+name+r'\([^\n]*\)\s*\{',text,re.M)
    if not m:raise ValueError(name)
    end=m.end();depth=1
    while depth:
        if text[end]=='{':depth+=1
        if text[end]=='}':depth-=1
        end+=1
    return text[m.start():end]

text=(ROOT/'main.cpp').read_text()
source=r'''
#include <cstdint>
#include <cstdio>
#include "config.h"
static uint64_t now;
uint64_t time_us_64(){return now;}
struct synth_diagnostics_t {uint64_t render_buffers,render_total_us;uint32_t max_render_us;bool ready;};
struct audio_diagnostics_t {uint32_t underruns,late_publications,irq_deadline_misses;};
static synth_diagnostics_t stats;
static audio_diagnostics_t errors;
void synth_get_diagnostics(synth_diagnostics_t *p){*p=stats;}
void audio_get_diagnostics(audio_diagnostics_t *p){*p=errors;}
'''
source+='\n'.join(extract(text,n) for n in ['get_cpu_avg_percent','get_cpu_max_percent','get_audio_error_counts'])
source+=r'''
static int failed;
void check(const char *name,bool ok){printf("%s %s\n",ok?"PASS":"FAIL",name);failed+=!ok;}
int main(){
 check("diagnostic is explicitly 24 kHz",SOUND_OUTPUT_FREQUENCY==24000);
 now=1000000;stats={100,461300,4613,false};
 check("4.613ms uses 86 percent of 24k buffer",get_cpu_avg_percent()==86&&get_cpu_max_percent()==86);
 now=1500000;stats={200,1261300,8000,false};
 check("average cached inside sampling window",get_cpu_avg_percent()==86);
 now=2000000;check("new window reports overload without lifetime dilution",get_cpu_avg_percent()==150&&get_cpu_max_percent()==150);
 now=3000000;check("no rendered buffers is safe",get_cpu_avg_percent()==0);
 errors={1,2,3};uint32_t u,l,i;get_audio_error_counts(&u,&l,&i);
 check("uninitialized audio lock is not accessed",u==0&&l==0&&i==0);
 stats.ready=true;get_audio_error_counts(&u,&l,&i);check("delivery errors exposed separately",u==1&&l==2&&i==3);
 return failed?1:0;
}
'''
out=OUT/'cpu24-results';out.mkdir(exist_ok=True);p=out/'cpu-probes.cpp';p.write_text(source)
env,_,_=vs_environment()
c=subprocess.run([shutil.which('cl.exe',path=env['PATH']),'/nologo','/std:c++17','/W3','/I'+str(ROOT),str(p),'/Fe:'+str(out/'cpu-probes.exe'),'/Fo:'+str(out/'cpu-probes.obj')],env=env,cwd=out,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
(out/'cpu-probes-compile.log').write_text(c.stdout)
if c.returncode:print(c.stdout);sys.exit(c.returncode)
r=subprocess.run([str(out/'cpu-probes.exe')],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
(out/'cpu-probes.log').write_text(r.stdout);print(r.stdout);sys.exit(r.returncode)
