#define _CRT_SECURE_NO_WARNINGS
#include <cstdint>
#include "config.h"
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>
#include <algorithm>
using boolean=bool;using byte=uint8_t;
#define __attribute__(x)
#define __not_in_flash_func(x) x
static uint64_t frame;static FILE *events,*trace;
static uint64_t mono_transitions;
static void pop_mono(int old,int note){mono_transitions++;if(events)fprintf(events,"%llu,%.6f,mono_transition,0,%d,%d\n",(unsigned long long)frame,frame/24000.0,old,note);}
static uint64_t steals,retriggers,clips,clip_run,max_clip_run;
static int64_t mix_peak,chorus_peak,delay_peak;
static uint32_t window_clips,window_jump;
static int64_t window_peak;
static void pop_voice(int voice,int old,int note,bool retrigger){
    if(retrigger)retriggers++;else if(old<128)steals++;
    if(events)fprintf(events,"%llu,%.6f,%s,%d,%d,%d\n",(unsigned long long)frame,frame/24000.0,retrigger?"retrigger":old<128?"steal":"allocate",voice,old,note);
}
static int64_t mag(int64_t v){return v<0?-v:v;}
static void pop_signal(int32_t mix,int32_t chorus,int32_t delay){
    mix_peak=std::max(mix_peak,mag(mix));chorus_peak=std::max(chorus_peak,mag(chorus));delay_peak=std::max(delay_peak,mag(delay));
    int64_t pre=(int64_t)delay*2;window_peak=std::max(window_peak,mag(pre));
    if(pre>32767LL*256 || pre< -32767LL*256){clips++;window_clips++;clip_run++;max_clip_run=std::max(max_clip_run,clip_run);}else clip_run=0;
}
#if OBSERVED
#include "observed-synth.h"
#else
#include "pra32-u2-synth.h"
#endif
#include "instrument_preset.h"
#include "demo_songs.h"
struct Event{uint64_t at;uint8_t status,a,b;};
static std::vector<Event> schedule;
static void add(uint64_t ms,uint8_t s,uint8_t a,uint8_t b){schedule.push_back({ms*24,s,a,b});}
static uint32_t be32(const uint8_t*p){return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3];}
static void midi(const demo_song_t &song){
 const uint8_t*d=song.data;size_t size=song.size,p=0,end=0;
 auto need=[&](size_t n){if(p+n>end){fprintf(stderr,"truncated MIDI\n");exit(2);}};
 if(size<22||memcmp(d,"MThd",4)||d[8]||d[9]||d[10]||d[11]!=1)exit(2);
 uint32_t division=(d[12]<<8)|d[13];if(!division||(division&0x8000))exit(2);
 p=8+be32(d+4);end=size;need(8);if(memcmp(d+p,"MTrk",4))exit(2);
 uint32_t length=be32(d+p+4);p+=8;need(length);end=p+length;
 auto vlq=[&](){uint32_t v=0;for(int i=0;i<4;i++){need(1);uint8_t b=d[p++];v=(v<<7)|(b&127);if(!(b&128))return v;}exit(2);return 0u;};
 uint64_t us=0,rem=0;uint32_t tempo=500000;uint8_t running=0;
 while(p<end){uint32_t delta=vlq();uint64_t n=(uint64_t)delta*tempo+rem;us+=n/division;rem=n%division;need(1);uint8_t s=d[p];if(s&128){p++;if(s<0xf0)running=s;}else{s=running;if(s<0x80||s>=0xf0)exit(2);}
  if(s==0xff){need(1);uint8_t type=d[p++];uint32_t n=vlq();need(n);if(type==0x2f)break;if(type==0x51){if(n!=3)exit(2);tempo=d[p]<<16|d[p+1]<<8|d[p+2];if(!tempo)exit(2);}p+=n;}
  else if(s==0xf0||s==0xf7){running=0;uint32_t n=vlq();need(n);p+=n;}
  else{if(s>=0xf0)exit(2);int n=((s&0xf0)==0xc0||(s&0xf0)==0xd0)?1:2;need(n);uint8_t a=d[p++],b=n==2?d[p++]:0;if(a>127||b>127)exit(2);schedule.push_back({(us*24000+999999)/1000000,s,a,b});}
 }
}
static void u16(FILE*f,uint16_t n){fputc(n&255,f);fputc(n>>8,f);}static void u32(FILE*f,uint32_t n){u16(f,n&65535);u16(f,n>>16);}
int main(int argc,char **argv){
 if(argc!=4)return 2;std::string name=argv[1],base=argv[2];
 static PRA32_U2_Synth synth;synth.initialize();synth.program_change(6); // Elec. Piano, factory program 6
 if(std::string(argv[3])=="poly")synth.control_change(VOICE_MODE,VOICE_POLYPHONIC);
 bool song=name=="show-yourself"||name=="let-it-go";
 if(song)midi(demo_songs[name=="show-yourself"?6:5]);
 else if(name=="held"){add(0,0x90,60,100);add(3000,0x80,60,0);}
 else if(name=="fifth"){for(int i=0;i<5;i++)add(i*500,0x90,(uint8_t)(60+i*3),100);for(int i=0;i<5;i++)add(4000,0x80,(uint8_t)(60+i*3),0);}
 else if(name=="retrigger"){for(int i=0;i<8;i++)add(i*250,0x90,60,100);for(int i=0;i<8;i++)add(2500,0x80,60,0);}
 else if(name=="chords"){for(int i=0;i<6;i++)for(int n:{60,64,67}){add(i*400,0x90,(uint8_t)(n+(i%2)*2),100);add(i*400+250,0x80,(uint8_t)(n+(i%2)*2),0);}}
 else return 2;
 std::stable_sort(schedule.begin(),schedule.end(),[](const Event&a,const Event&b){return a.at<b.at;});
 events=fopen((base+"-events.csv").c_str(),"w");trace=fopen((base+"-signal.csv").c_str(),"w");FILE*w=fopen((base+".wav").c_str(),"wb");if(!events||!trace||!w)return 2;
 fprintf(events,"frame,seconds,event,voice_or_status,old_or_note,new_or_velocity\n");fprintf(trace,"end_frame,preclamp_peak,clipped_samples,max_step\n");
 uint64_t total=schedule.back().at+24000*5;uint32_t bytes=(uint32_t)(total*2);fwrite("RIFF",1,4,w);u32(w,36+bytes);fwrite("WAVEfmt ",1,8,w);u32(w,16);u16(w,1);u16(w,1);u32(w,24000);u32(w,48000);u16(w,2);u16(w,16);fwrite("data",1,4,w);u32(w,bytes);
 size_t pos=0;int previous=0;uint32_t maximum_step=0;uint64_t maximum_step_frame=0;uint16_t owned[128]={};bool stopped=false;
 for(frame=0;frame<total;frame++){
  while(pos<schedule.size()&&schedule[pos].at<=frame){auto e=schedule[pos++];fprintf(events,"%llu,%.6f,input,%u,%u,%u\n",(unsigned long long)frame,frame/24000.0,e.status,e.a,e.b);
   switch(e.status&0xf0){case 0x90:if(e.b){if(!song||owned[e.a]++==0)synth.note_on(e.a,e.b);break;}[[fallthrough]];case 0x80:if(!song||(owned[e.a]&&!--owned[e.a]))synth.note_off(e.a);break;case 0xb0:synth.control_change(e.a,e.b);break;case 0xe0:synth.pitch_bend(e.a,e.b);break;default:break;}
  }
  if(song&&!stopped&&pos==schedule.size()){synth.control_change(64,0);synth.all_notes_off();stopped=true;}
  int16_t right;int16_t sample=synth.process(right);u16(w,(uint16_t)sample);
  uint32_t step=(uint32_t)abs((int)sample-previous);previous=sample;window_jump=std::max(window_jump,step);if(step>maximum_step){maximum_step=step;maximum_step_frame=frame;}
  if(frame%128==127||frame+1==total){fprintf(trace,"%llu,%lld,%u,%u\n",(unsigned long long)frame,(long long)window_peak,window_clips,window_jump);window_peak=0;window_clips=window_jump=0;}
 }
 fclose(w);fclose(events);fclose(trace);
 FILE*j=fopen((base+"-summary.json").c_str(),"w");fprintf(j,"{\"frames\":%llu,\"steals\":%llu,\"retriggers\":%llu,\"clip_samples\":%llu,\"longest_clip_run\":%llu,\"mix_peak\":%lld,\"chorus_peak\":%lld,\"delay_peak\":%lld,\"maximum_step\":%u,\"maximum_step_frame\":%llu}\n",(unsigned long long)total,(unsigned long long)steals,(unsigned long long)retriggers,(unsigned long long)clips,(unsigned long long)max_clip_run,(long long)mix_peak,(long long)chorus_peak,(long long)delay_peak,maximum_step,(unsigned long long)maximum_step_frame);fclose(j);
 return 0;
}
