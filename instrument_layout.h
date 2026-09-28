#pragma once
#include <stdint.h>
#include <stdbool.h>
#define USER_INSTRUMENT_BASE 14
#define USER_INSTRUMENT_COUNT 4
#if DODEPAN_ENGINE_218
#define INSTRUMENT_COUNT 26
#else
#define INSTRUMENT_COUNT 18
#endif
static inline bool instrument_is_user(uint8_t id){return id>=14 && id<18;}
static inline uint8_t instrument_factory_program(uint8_t id){
#if DODEPAN_ENGINE_218
    return id>=6 && id<14? id+10 : id>=18 && id<26? id+6 : 255;
#else
    return id>=6 && id<14? id-6 : 255;
#endif
}

// Menu order is independent of persistent instrument IDs and MIDI/program mapping.
static inline uint8_t instrument_menu_step(uint8_t id,bool forward){
#if DODEPAN_ENGINE_218
    static const uint8_t order[INSTRUMENT_COUNT]={
        0,1,2,3,4,5,             // Dodepan custom sounds
        8,21,                   // Piano and organ
        12,18,19,               // Bass, brass and strings
        6,7,20,                 // Pads
        10,11,22,23,24,          // Leads
        9,13,25,                // Simple and initialization programs
        14,15,16,17             // Saved user slots
    };
    for(unsigned i=0;i<INSTRUMENT_COUNT;i++)if(order[i]==id){
        if(forward && i+1<INSTRUMENT_COUNT)return order[i+1];
        if(!forward && i>0)return order[i-1];
        return id;
    }
    return order[0];
#else
    if(id>=INSTRUMENT_COUNT)return 0;
    return forward?(id+1<INSTRUMENT_COUNT?id+1:id):(id>0?id-1:id);
#endif
}
