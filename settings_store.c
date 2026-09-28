#include "volume_control.h"
#include "settings_store.h"
#include "instrument_layout.h"
#include <string.h>
#define PAYLOAD_BYTES 247u
#define RECORD_LENGTH (16u+PAYLOAD_BYTES)
static uint32_t read32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void write32(uint8_t *p,uint32_t n){for(int i=0;i<4;i++)p[i]=(uint8_t)(n>>(i*8));}
static uint32_t crc(const uint8_t *p){
    uint32_t c=0xffffffffu;
    for(size_t i=0;i<RECORD_LENGTH;i++)if(i<12 || i>=16){
        c^=p[i];for(int b=0;b<8;b++)c=(c>>1)^(0xedb88320u&-(c&1));
    }
    return ~c;
}
bool settings_validate(const settings_data_t *d,uint8_t scales){
    if(d->key>99 || d->scale>=scales || d->instrument>=INSTRUMENT_COUNT || d->imu_axes>3 || d->volume>VOLUME_MAX || d->contrast>3 || d->chord>=5)return false;
    for(int i=0;i<4;i++){
        for(int j=0;j<48;j++)if(d->presets[i][j]>127)return false;
        for(int j=0;j<12;j++)if(d->scales[i][j]>24)return false;
    }
    return true;
}
static void unpack(const uint8_t *p,settings_data_t *d){
    d->key=p[0];d->scale=p[1];d->instrument=p[2];d->imu_axes=p[3];d->volume=p[4];d->contrast=p[5];d->chord=p[6];
    memcpy(d->presets,p+7,192);memcpy(d->scales,p+199,48);
}
bool settings_decode(const uint8_t *p,size_t size,uint8_t scales,settings_data_t *out,uint32_t *generation){
    if(size<SETTINGS_RECORD_BYTES || memcmp(p,"DOD2",4) || (p[4]<1 || p[4]>SETTINGS_FORMAT_VERSION) || p[5]!=0 || ((unsigned)p[6]|((unsigned)p[7]<<8))!=RECORD_LENGTH || read32(p+12)!=crc(p))return false;
    settings_data_t d;unpack(p+16,&d);
    if(p[4]<SETTINGS_FORMAT_VERSION){
        if(d.volume>(p[4]==1?8:100))return false;
        d.volume=VOLUME_DEFAULT; // One-time reset when adopting the revised curve.
    }
    if(!settings_validate(&d,scales))return false;
    *out=d;*generation=read32(p+8);return true;
}
bool settings_encode(uint8_t *p,size_t size,uint8_t scales,const settings_data_t *d,uint32_t generation){
    if(size<SETTINGS_RECORD_BYTES || !settings_validate(d,scales))return false;
    memset(p,0xff,SETTINGS_RECORD_BYTES);memcpy(p,"DOD2",4);p[4]=SETTINGS_FORMAT_VERSION;p[5]=0;p[6]=RECORD_LENGTH&255;p[7]=RECORD_LENGTH>>8;write32(p+8,generation);
    uint8_t *v=p+16;v[0]=d->key;v[1]=d->scale;v[2]=d->instrument;v[3]=d->imu_axes;v[4]=d->volume;v[5]=d->contrast;v[6]=d->chord;
    memcpy(v+7,d->presets,192);memcpy(v+199,d->scales,48);write32(p+12,crc(p));return true;
}
bool settings_decode_legacy(const uint8_t *p,size_t size,uint8_t scales,settings_data_t *out){
    if(size<SETTINGS_LEGACY_BYTES || memcmp(p,"DODE",4))return false;
    settings_data_t d={0};d.key=p[4];d.scale=p[5];d.instrument=p[6];d.imu_axes=p[7];d.volume=p[8];d.contrast=p[9];d.chord=p[10];
    for(int i=0;i<4;i++){
        memcpy(d.presets[i],p+16+i*45,45);
        /* Missing legacy delay controls: deterministic Dodepan defaults. */
        d.presets[i][45]=0;d.presets[i][46]=127;d.presets[i][47]=1;
    }
    memcpy(d.scales,p+196,48);
    if(d.volume>8)return false;d.volume=VOLUME_DEFAULT;
    if(!settings_validate(&d,scales))return false;*out=d;return true;
}
int settings_select(const uint8_t *a,const uint8_t *b,uint8_t scales,settings_data_t *out,uint32_t *generation){
    settings_data_t da,db;uint32_t ga=0,gb=0;
    bool va=settings_decode(a,SETTINGS_RECORD_BYTES,scales,&da,&ga),vb=settings_decode(b,SETTINGS_RECORD_BYTES,scales,&db,&gb);
    if(!va&&!vb)return -1;
    if(vb&&(!va||(int32_t)(gb-ga)>0)){*out=db;*generation=gb;return 1;}
    *out=da;*generation=ga;return 0;
}
