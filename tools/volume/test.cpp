#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <initializer_list>
#include "volume_control.h"
#include "settings_store.c"
int main(){
 assert(volume_gain_q15[0]==0 && volume_gain_q15[100]==32768);
 assert(volume_gain_q15[1]==123 && VOLUME_DEFAULT==30);
 for(int i=1;i<=100;i++)assert(volume_gain_q15[i]>volume_gain_q15[i-1]);
 assert(volume_clamp(255)==100);
 for(int rate: {24000,48000}){
  volume_ramp_t r={};int previous=0;
  for(int target: {33,32768,18,0}){
   volume_ramp_target(&r,target,rate/100);
   bool up=target>=previous;
   for(int i=0;i<rate/100;i++){
    volume_ramp_target(&r,target,rate/100); // buffer polling must not restart
    int value=volume_ramp_next(&r);
    assert(up?(value>=previous && value<=target):(value<=previous && value>=target));previous=value;
   }
   assert(previous==target && r.remaining==0);
  }
  volume_ramp_target(&r,32768,rate/100);volume_ramp_next(&r);
  volume_ramp_target(&r,0,rate/100);
  for(int i=0;i<rate/100;i++)volume_ramp_next(&r);
  assert(r.value==0);
 }
 settings_data_t d={},out={};uint32_t generation;uint8_t record[512],snapshot[512];
 for(int v=0;v<=100;v++){
  d.volume=v;assert(settings_encode(record,512,16,&d,7));
  assert(record[4]==SETTINGS_FORMAT_VERSION && settings_decode(record,512,16,&out,&generation));
  assert(out.volume==v && generation==7);
 }
 for(int v=0;v<=9;v++){
  d.volume=v;assert(settings_encode(record,512,16,&d,7));record[4]=1;write32(record+12,crc(record));
  memcpy(snapshot,record,512);bool ok=settings_decode(record,512,16,&out,&generation);
  assert(ok==(v<=8));assert(!memcmp(snapshot,record,512));
  if(v<=8){assert(out.volume==30);}
  uint8_t old[244]={};memcpy(old,"DODE",4);old[8]=v;
  ok=settings_decode_legacy(old,244,16,&out);assert(ok==(v<=8));
  if(ok)assert(out.volume==30);
 }
 // Previous 0-100 format initializes the new default without mutating flash bytes.
 for(int v=0;v<=100;v++){
  d.volume=v;assert(settings_encode(record,512,16,&d,7));record[4]=2;write32(record+12,crc(record));
  memcpy(snapshot,record,512);assert(settings_decode(record,512,16,&out,&generation));
  assert(out.volume==30 && !memcmp(snapshot,record,512));
 }
 // Newest valid sector wins; interrupted/corrupted save falls back to previous record.
 uint8_t a[512],b[512];d.volume=17;assert(settings_encode(a,512,16,&d,10));
 d.volume=42;assert(settings_encode(b,512,16,&d,11));
 assert(settings_select(a,b,16,&out,&generation)==1 && out.volume==42);
 b[20]^=1;assert(settings_select(a,b,16,&out,&generation)==0 && out.volume==17);
 d.volume=101;assert(!settings_validate(&d,16));
 puts("PASS: gain curve, 24/48k ramps, retarget/mute, legacy/v1/v2 migration, v3 roundtrips and interrupted-save fallback");
}
