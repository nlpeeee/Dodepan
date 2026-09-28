#pragma once
#include "pra32-u2-synth.h"
#include "instrument_preset.h"
#if DODEPAN_ENGINE_218
#include "preset_conversion.h"
#include "converted_custom_presets.h"
using DodepanEngine=PRA32_U2_Synth<>;
static_assert(SAMPLING_RATE==SYNTH_RENDER_FREQUENCY,"Native engine rate mismatch");
static inline void backend_native_preset(DodepanEngine &engine,const uint8_t *converted){
    engine.all_notes_off();
    // Reset every upstream program control before applying the Dodepan bank.
    engine.program_change(16);
    engine.control_change(A_D_VEL_SENS,64);engine.control_change(REL_VEL_SENS,64);
    engine.control_change(A_D_KEY_TRK,64);engine.control_change(STRETCH_TUNE,64);
    engine.control_change(COARSE_TUNE,52);engine.control_change(FINE_TUNE,64);
    engine.control_change(OSC_DRIFT,0);engine.control_change(PAN,64);
    engine.control_change(OSC_SAW_W_MODE,0);
    for(unsigned i=0;i<PROGRAM_PARAMS_NUM;i++)engine.control_change(dodepan_program_parameters[i],converted[i]);
}
static inline void backend_preset(DodepanEngine &engine,const uint8_t *values){uint8_t converted[48];convert_legacy_preset(values,converted);backend_native_preset(engine,converted);}
static inline void backend_custom(DodepanEngine &engine,uint8_t id){if(id<6)backend_native_preset(engine,converted_custom_presets[id]);}
static inline void backend_program(DodepanEngine &engine,uint8_t p){if(p>=16 && p<=31){engine.all_notes_off();engine.program_change(p);}}
static inline void backend_off(DodepanEngine &engine,uint8_t n){engine.note_off(n,64);}
#else
using DodepanEngine=PRA32_U2_Synth;
static inline void backend_preset(DodepanEngine &engine,const uint8_t *values){
    engine.all_notes_off();
    for(unsigned i=0;i<PRESET_APPLY_PARAMS_NUM;i++)engine.control_change(dodepan_program_parameters[i],values[i]);
}
static inline void backend_program(DodepanEngine &engine,uint8_t p){engine.all_notes_off();engine.program_change(p);}
static inline void backend_custom(DodepanEngine &engine,uint8_t id){const uint8_t *p[]={dodepan_preset,magic_bell_preset,space_piano_preset,robot_voice_preset,synthwave_preset,bleep_bloop_preset};if(id<6)backend_preset(engine,p[id]);}
static inline void backend_off(DodepanEngine &engine,uint8_t n){engine.note_off(n);}
#endif
