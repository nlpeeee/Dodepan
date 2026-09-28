from pathlib import Path
import hashlib,json,os,shutil,struct,subprocess,sys,zlib
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/audit'))
from build_baseline import vs_environment
from decode_capture import decode
out=ROOT/'build-audit/pop-capture-tests';out.mkdir(exist_ok=True)
env,_,_=vs_environment();cl=shutil.which('cl.exe',path=env['PATH'])
source=out/'ring.cpp'
source.write_text(r"""
#define __attribute__(x)
#include "pop_capture_ring.h"
#include <cassert>
#include <cstdio>
int main(){
 PopRing<4> r;assert(r.count==0 && r.total==0);
 int16_t s[]={1,99,-2,99,32767,99,-32768,99,5,99,6,99};
 r.append(s,2);assert(r.count==2 && r.byte_at(0)==1 && r.byte_at(2)==254 && r.byte_at(3)==255);
 r.append(s+4,4);assert(r.count==4 && r.total==6);
 unsigned char expected[]={255,127,0,128,5,0,6,0};
 for(int i=0;i<8;++i){assert(r.byte_at(i)==expected[i]);putchar(r.byte_at(i));}
 FILE *f=fopen("crc.txt","w");fprintf(f,"%08x",r.crc());fclose(f);
 // More than one complete wrap in a single append.
 PopRing<2> small;small.append(s,6);assert(small.count==2 && small.byte_at(0)==5 && small.byte_at(2)==6);
}
""")
cmd=[cl,'/nologo','/O2','/EHsc','/std:c++17','/I'+str(ROOT),str(source),'/Fe:'+str(out/'ring.exe'),'/Fo:'+str(out/'ring.obj')]
r=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
(out/'compile.log').write_bytes(r.stdout);r.check_returncode()
pcm=subprocess.check_output([str(out/'ring.exe')],cwd=out)
assert len(pcm)==8 and (out/'crc.txt').read_text()==f'{zlib.crc32(pcm):08x}'
h={'format':'DODEPAN_PCM1','rate':24000,'frames':4,'crc32':f'{zlib.crc32(pcm):08x}'}
blob=json.dumps(h).encode()+b'\n'+pcm
assert decode(blob)==(h,pcm)
for bad in [blob[:-1],blob+b'\0',blob[:-1]+b'\xff',b'bad',blob.replace(b'24000',b'48000'),blob.replace(b'"frames": 4',b'"frames": 0')]:
 try:decode(bad)
 except (ValueError,KeyError):pass
 else:raise AssertionError('Invalid transfer accepted')
base=json.loads((ROOT/'build-audit/pop-capture-baseline/hashes.json').read_text())
assert all(hashlib.sha256((ROOT/name).read_bytes()).hexdigest()==digest for name,digest in base.items())
print('PASS: ring ordering/wrap, signed PCM, CRC, six invalid transfers rejected; engine/preset hashes unchanged.')

# Exercise the actual export state machine with stalled/partial USB writes.
mock=out/'mock';mock.mkdir(exist_ok=True);(mock/'pico').mkdir(exist_ok=True)
(mock/'config.h').write_text('#define DODEPAN_POP_CAPTURE 1\n#define PROGRAM_VERSION "test"\n#define DODEPAN_SOURCE_ID "test"\n#define DODEPAN_ENGINE_ID "test"\n#define SOUND_OUTPUT_FREQUENCY 24000\n#define SYSTEM_CLOCK_KHZ 288000\n#define AUDIO_BUFFER_LENGTH 128\n')
(mock/'pico/stdlib.h').write_text('#define __not_in_flash_func(x) x\n')
(mock/'synth_service.h').write_text('#pragma once\n#include <stdint.h>\nstruct synth_diagnostics_t{bool suspended=false;uint32_t queue_overflows=0,max_render_us=1000;};\nvoid synth_request_suspend(bool);void synth_get_diagnostics(synth_diagnostics_t*);\n')
(mock/'sound_i2s.h').write_text('#pragma once\n#include <stdint.h>\nstruct audio_diagnostics_t{uint32_t underruns=0,late_publications=0,irq_deadline_misses=0,buffer_period_us=5333;};\nvoid audio_get_diagnostics(audio_diagnostics_t*);\n')
(mock/'tusb.h').write_text('#pragma once\n#include <stdint.h>\nunsigned tud_cdc_available();int tud_cdc_read_char();bool tud_cdc_connected();unsigned tud_cdc_write_available();unsigned tud_cdc_write(const void*,unsigned);void tud_cdc_write_flush();\n')
(mock/'capture_impl.cpp').write_bytes((ROOT/'pop_capture.cpp').read_bytes())
(mock/'transport.cpp').write_text(r"""
#define __attribute__(x)
#include "capture_impl.cpp"
#include <cassert>
#include <vector>
static bool requested=false,suspended=false;
static int command=-1;static unsigned capacity=0;
static std::vector<unsigned char> output;
unsigned tud_cdc_available(){return command!=-1;}
int tud_cdc_read_char(){int c=command;command=-1;return c;}
bool tud_cdc_connected(){return true;}
unsigned tud_cdc_write_available(){return capacity;}
unsigned tud_cdc_write(const void* p,unsigned n){n=std::min(n,capacity);auto b=(const unsigned char*)p;output.insert(output.end(),b,b+n);return n;}
void tud_cdc_write_flush(){}
void synth_request_suspend(bool s){requested=s;}
void synth_get_diagnostics(synth_diagnostics_t* d){*d={};d->suspended=suspended;}
void audio_get_diagnostics(audio_diagnostics_t* d){*d={};}
int main(){
 int16_t a[]={1,99,2,99};pop_capture_audio(a,2);assert(!pop_capture_task());
 command='c';assert(!pop_capture_task());assert(!requested);
 pop_capture_audio(a,2);assert(pop_capture_task() && requested);assert(output.empty());
 auto frames=ring.total;pop_capture_audio(a,2);assert(ring.total==frames);
 suspended=true;for(int i=0;i<1000;++i)assert(pop_capture_task());assert(output.empty());
 capacity=7;for(int i=0;i<1000;++i)assert(pop_capture_task());
 auto first=output;assert(!first.empty());output.clear();command='c';
 for(int i=0;i<1000;++i)pop_capture_task();assert(output==first);
 FILE*f=fopen("transport.capture","wb");fwrite(output.data(),1,output.size(),f);fclose(f);
}
""")
cmd=[cl,'/nologo','/O2','/EHsc','/std:c++17','/I'+str(mock),'/I'+str(ROOT),str(mock/'transport.cpp'),'/Fe:'+str(out/'transport.exe'),'/Fo:'+str(out/'transport.obj')]
r=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(out/'transport-compile.log').write_bytes(r.stdout);r.check_returncode()
subprocess.run([str(out/'transport.exe')],cwd=out,check=True)
h,data=decode((out/'transport.capture').read_bytes());assert data==struct.pack('<hhhh',1,2,1,2)
print('PASS: freeze handoff, no export until suspension, stalled/partial USB writes, immutable snapshot and exact resend.')
