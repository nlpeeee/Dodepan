#pragma once
#include <cstring>
#include "instrument_preset.h"
#include "preset_conversion_tables.h"
static inline unsigned preset_index(uint8_t cc){for(unsigned i=0;i<48;i++)if(dodepan_program_parameters[i]==cc)return i;return 48;}
static inline void convert_legacy_preset(const uint8_t *source,uint8_t *target){
    memcpy(target,source,48);
    for(unsigned i=0;i<48;++i){
        const uint8_t v=source[i];
        switch(dodepan_program_parameters[i]){
        case EG_ATTACK:case AMP_ATTACK:target[i]=preset_map_attack[v];break;
        case EG_DECAY:case AMP_DECAY:target[i]=preset_map_decay[v];break;
        case EG_RELEASE:case AMP_RELEASE:target[i]=preset_map_release[v];break;
        case LFO_RATE:target[i]=preset_map_lfo_rate[v];break;
        case LFO_FADE_TIME:target[i]=preset_map_lfo_fade[v];break;
        case PORTAMENTO:target[i]=preset_map_portamento[v];break;
        case CHORUS_RATE:target[i]=preset_map_chorus_rate[v];break;
        case CHORUS_DEPTH:target[i]=preset_map_chorus_depth[v];break;
        case CHORUS_MIX:target[i]=preset_map_chorus_mix[v];break;
        case DELAY_TIME:target[i]=preset_map_delay_time[v];break;
        case FILTER_CUTOFF:target[i]=preset_map_filter[source[preset_index(FILTER_KEY_TRK)]][v];break;
        case AMP_GAIN:target[i]=preset_map_gain_by_chorus[source[preset_index(CHORUS_MIX)]][v];break;
        }
    }
    // Old release=decay selected the RELEASE table using the raw decay control.
    // Translate independently because the target decay and release tables differ.
    if(source[preset_index(REL_EQ_DECAY)]>=64){
        target[preset_index(EG_RELEASE)]=preset_map_release[source[preset_index(EG_DECAY)]];
        target[preset_index(AMP_RELEASE)]=preset_map_release[source[preset_index(AMP_DECAY)]];
        target[preset_index(REL_EQ_DECAY)]=0;
    }
}
