#pragma once
#include <stdint.h>
#include <stdbool.h>
#define AUDIO_POOL_COUNT 3
#define AUDIO_NO_BUFFER (-1)
typedef enum { AUDIO_FREE, AUDIO_RENDERING, AUDIO_READY, AUDIO_PLAYING } audio_buffer_state_t;
typedef struct { audio_buffer_state_t state; uint32_t generation, order; } audio_slot_t;
typedef struct { audio_slot_t slots[AUDIO_POOL_COUNT]; uint32_t generation, order; } audio_pool_t;
/* Caller serializes these transitions. DMA may only receive PLAYING buffers. */
static inline int audio_pool_acquire(audio_pool_t *p, uint32_t *generation) {
    for (int i=0;i<AUDIO_POOL_COUNT;i++) if (p->slots[i].state==AUDIO_FREE) {
        p->slots[i].state=AUDIO_RENDERING;
        *generation=p->slots[i].generation=++p->generation; return i;
    }
    return AUDIO_NO_BUFFER;
}
static inline bool audio_pool_publish(audio_pool_t *p,int id,uint32_t generation) {
    if(id<0 || id>=AUDIO_POOL_COUNT || p->slots[id].state!=AUDIO_RENDERING || p->slots[id].generation!=generation) return false;
    p->slots[id].order=++p->order;p->slots[id].state=AUDIO_READY;return true;
}
static inline int audio_pool_take_ready(audio_pool_t *p) {
    int chosen=AUDIO_NO_BUFFER;
    for(int i=0;i<AUDIO_POOL_COUNT;i++) if(p->slots[i].state==AUDIO_READY && (chosen<0 || (int32_t)(p->slots[i].order-p->slots[chosen].order)<0)) chosen=i;
    if(chosen>=0)p->slots[chosen].state=AUDIO_PLAYING;return chosen;
}
static inline void audio_pool_reclaim(audio_pool_t *p,int current,int pending) {
    for(int i=0;i<AUDIO_POOL_COUNT;i++) if(p->slots[i].state==AUDIO_PLAYING && i!=current && i!=pending)p->slots[i].state=AUDIO_FREE;
}
