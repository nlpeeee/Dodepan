from pathlib import Path
import hashlib,json,shutil,subprocess,sys
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools/audit'))
from build_baseline import vs_environment
out=ROOT/'build-audit/transport48-tests';out.mkdir(exist_ok=True)
s=(ROOT/'synth_service.cpp').read_text();begin=s.index('        for(unsigned i=0;i<AUDIO_BUFFER_LENGTH;i++)');end=s.index('        pop_capture_audio',begin);loop=s[begin:end]
m=(ROOT/'main.cpp').read_text();a=m.index('extern "C" uint32_t get_cpu_avg_percent');b=m.index('extern "C" void get_audio_error_counts',a);cpu=m[a:b]
source=r"""
#include "config.h"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
static uint64_t now=1000000;
uint64_t time_us_64(){return now;}
struct synth_diagnostics_t{uint64_t render_total_us=100000,render_buffers=100;uint32_t max_render_us=1760;};
void synth_get_diagnostics(synth_diagnostics_t*d){*d={};}
"""+cpu+r"""
struct Engine {unsigned calls=0;int16_t process(int16_t&r){r=123;return (int16_t)((calls++%2)?-32768:32767);}};
int main(){
 static_assert(SYNTH_RENDER_FREQUENCY==24000);
 static_assert(AUDIO_TRANSPORT_FRAMES<=256);
 assert(SOUND_OUTPUT_FREQUENCY==24000*AUDIO_TRANSPORT_REPEAT);
 assert(uint64_t(AUDIO_TRANSPORT_FRAMES)*1000000/SOUND_OUTPUT_FREQUENCY==5333);
 // PIO divisor is exactly representable at 288 MHz for both test modes.
 const unsigned div256=uint64_t(288000000)*256/(SOUND_OUTPUT_FREQUENCY*64);
 assert(uint64_t(288000000)*256==uint64_t(div256)*SOUND_OUTPUT_FREQUENCY*64);
 assert(get_cpu_avg_percent()==18 && get_cpu_max_percent()==33);
 int16_t memory[2*AUDIO_TRANSPORT_FRAMES+2];std::fill(std::begin(memory),std::end(memory),12345);
 struct{int16_t*samples;}buffer{memory+1};Engine engine;int16_t right;int peak=0;int32_t gain=0,target=32768;
 for(int block=0;block<2;++block){
   int32_t reference_gain=gain;
"""+loop+r"""
   assert(engine.calls==(block+1)*128);assert(memory[0]==12345 && memory[2*AUDIO_TRANSPORT_FRAMES+1]==12345);
   for(unsigned i=0;i<128;++i){
     reference_gain+=std::max<int32_t>(-256,std::min<int32_t>(256,target-reference_gain));
     int16_t expected=(int32_t)((i%2)?-32768:32767)*reference_gain/32768;
     for(unsigned r=0;r<AUDIO_TRANSPORT_REPEAT;++r){unsigned f=i*AUDIO_TRANSPORT_REPEAT+r;assert(buffer.samples[2*f]==expected && buffer.samples[2*f+1]==expected);}
   }
   assert(reference_gain==gain);
 }
 puts("PASS: exact PCM/stereo, guards, synth calls, ramps, PIO rate, deadline and CPU percentages");
}
"""
# std::begin/end for fixed arrays.
source='#include <iterator>\n'+source
(out/'test.cpp').write_text(source)
env,_,_=vs_environment();cl=shutil.which('cl.exe',path=env['PATH'])
for mode in [0,1]:
 exe=out/f'test-{mode}.exe'
 r=subprocess.run([cl,'/nologo','/std:c++17','/EHsc','/O2','/DDODEPAN_TRANSPORT_48K='+str(mode),'/I'+str(ROOT),str(out/'test.cpp'),'/Fe:'+str(exe),'/Fo:'+str(out/f'test-{mode}.obj')],env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
 (out/f'compile-{mode}.log').write_bytes(r.stdout);r.check_returncode()
 r=subprocess.run([str(exe)],stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(out/f'test-{mode}.log').write_bytes(r.stdout);r.check_returncode();print(f'mode {mode}: '+r.stdout.decode().strip())
base=json.loads((ROOT/'build-audit/transport48-baseline/preserved-hashes.json').read_text())
assert all(hashlib.sha256((ROOT/n).read_bytes()).hexdigest()==h for n,h in base.items())
print('PASS: engine and preset hashes unchanged')
