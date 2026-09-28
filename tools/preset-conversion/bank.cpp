#include <cstdio>
#include "instrument_preset.h"
#include "preset_conversion.h"
const uint8_t* patches[]={dodepan_preset,magic_bell_preset,space_piano_preset,robot_voice_preset,synthwave_preset,bleep_bloop_preset};
int main(){for(auto p:patches){uint8_t raw[48],converted[48];memcpy(raw,p,48);raw[45]=127;raw[46]=0;raw[47]=1;convert_legacy_preset(raw,converted);for(int i=0;i<48;i++)printf("%s%u",i?",":"",converted[i]);puts("");}}
