#include <stdio.h>
#include <string.h>
#include "../../settings_store.c"
#include "../../lib/sound_i2s/audio_buffers.h"
#include "../../sensor_math.h"
#include "../../note_router.c"
static int checks,failures,on,off,pedal;
void note_sink_on(uint8_t n,uint8_t v){(void)n;(void)v;on++;}
void note_sink_off(uint8_t n){(void)n;off++;}
void note_sink_cc(uint8_t cc,uint8_t v){if(cc==64)pedal=v;}
static void check(const char *name,int ok){checks++;failures+=!ok;printf("%s %s\n",ok?"PASS":"FAIL",name);}
int main(void){
    uint8_t a[512],b[512],torn[512],legacy[244]={0};settings_data_t data={0},out={0};uint32_t generation;
    data.key=60;data.volume=8;data.instrument=17;
    for(int i=0;i<4;i++){for(int j=0;j<48;j++)data.presets[i][j]=(i*31+j)%128;for(int j=0;j<12;j++)data.scales[i][j]=j;}
    check("full 48-parameter round trip",settings_encode(a,512,20,&data,7)&&settings_decode(a,512,20,&out,&generation)&&generation==7&&!memcmp(&data,&out,sizeof(data)));
    settings_encode(b,512,20,&data,8);
    check("newest sector",settings_select(a,b,20,&out,&generation)==1&&generation==8);
    int safe=1;
    for(int n=0;n<263;n++){memset(torn,255,512);memcpy(torn,b,n);if(settings_select(a,torn,20,&out,&generation)!=0 || generation!=7)safe=0;}
    check("all torn record prefixes preserve prior sector",safe);
    safe=1;for(int i=0;i<263;i++){memcpy(torn,b,512);torn[i]^=1;if(settings_decode(torn,512,20,&out,&generation))safe=0;}
    check("every single-byte record corruption rejected",safe);
    check("truncated record rejected",!settings_decode(a,511,20,&out,&generation));
    settings_encode(a,512,20,&data,UINT32_MAX);settings_encode(b,512,20,&data,0);
    check("generation wrap",settings_select(a,b,20,&out,&generation)==1&&generation==0);
    memcpy(legacy,"DODE",4);legacy[4]=60;legacy[6]=17;legacy[8]=8;
    for(int i=0;i<4;i++){memcpy(legacy+16+i*45,data.presets[i],45);memcpy(legacy+196+i*12,data.scales[i],12);}
    safe=settings_decode_legacy(legacy,244,20,&out);
    for(int i=0;i<4;i++)safe=safe&&!memcmp(out.presets[i],data.presets[i],45)&&out.presets[i][45]==0&&out.presets[i][46]==127&&out.presets[i][47]==1&&!memcmp(out.scales[i],data.scales[i],12);
    check("all legacy slots and scales preserved",safe);
    data.presets[3][47]=128;check("invalid parameter rejected",!settings_encode(a,512,20,&data,0));data.presets[3][47]=1;
    data.scales[3][11]=25;check("invalid scale degree rejected",!settings_validate(&data,20));data.scales[3][11]=11;
    data.instrument=18;check("invalid instrument rejected",!settings_validate(&data,20));
    audio_pool_t pool={0};uint32_t tickets[3],ticket;int ids[3];
    for(int i=0;i<3;i++){ids[i]=audio_pool_acquire(&pool,&tickets[i]);audio_pool_publish(&pool,ids[i],tickets[i]);}
    check("no producer reuse of ready buffers",audio_pool_acquire(&pool,&ticket)<0);
    int current=audio_pool_take_ready(&pool),pending=audio_pool_take_ready(&pool);audio_pool_reclaim(&pool,current,pending);
    check("current and queued DMA buffers retained",current==ids[0]&&pending==ids[1]&&audio_pool_acquire(&pool,&ticket)<0);
    audio_pool_reclaim(&pool,pending,-1);int reused=audio_pool_acquire(&pool,&ticket);
    check("stale publication rejected",reused==current&&!audio_pool_publish(&pool,reused,tickets[0])&&audio_pool_publish(&pool,reused,ticket));
    check("ready FIFO ordering",audio_pool_take_ready(&pool)==ids[2]);
    check("zero and full-scale IMU",sensor_magnitude(0,0,0)==0&&sensor_magnitude(0,0,16384)==64&&sensor_magnitude(-32768,-32768,-32768)==221);
    safe=1;for(uint32_t i=0;i<100000;i+=17){uint32_t r=sensor_isqrt(i);if(r*r>i||(r+1)*(r+1)<=i)safe=0;}check("integer square root boundaries",safe);
    notes_on(0,60,100);notes_on(1,60,80);notes_off(0,60);check("shared pitch retained",on==1&&off==0&&notes_any_active());notes_off(1,60);check("last owner releases",off==1&&!notes_any_active());
    notes_control(0,64,127);notes_control(1,64,127);notes_release_source(0);check("shared sustain retained",pedal==127&&notes_any_active());notes_release_source(1);check("last sustain released",pedal==0&&!notes_any_active());
    notes_on(0,61,0);check("zero velocity is not an onset",on==1);notes_on(0,61,100);notes_on(14,62,100);notes_panic();check("panic empties all owners",!notes_any_active()&&off==3);
    for(int i=0;i<256;i++)notes_on(NOTE_SOURCE_LOOP,60,100);
    for(int i=0;i<255;i++)notes_off(NOTE_SOURCE_LOOP,60);
    check("maximum loop duplicate count retains final owner",notes_any_active());notes_off(NOTE_SOURCE_LOOP,60);
    check("maximum loop duplicate count releases finally",!notes_any_active());
    printf("RESULT %d checks, %d failures\n",checks,failures);return failures?1:0;
}
