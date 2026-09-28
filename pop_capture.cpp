#include "config.h"
#include "pop_capture.h"
#if DODEPAN_POP_CAPTURE
#include "pop_capture_ring.h"
#include "pico/stdlib.h"
#include "synth_service.h"
#include "sound_i2s.h"
#include "tusb.h"
#include <atomic>
#include <cstdio>
#include <algorithm>
static_assert(std::atomic<unsigned>::is_always_lock_free,"Capture handoff must not lock");
static PopRing<32768> ring;
static std::atomic<unsigned> state{0}; // 0 recording, 1 freeze requested, 2 immutable
void __not_in_flash_func(pop_capture_audio)(const int16_t *stereo,unsigned frames){
    unsigned s=state.load(std::memory_order_relaxed);
    if(s==2)return;
    ring.append(stereo,frames);
    if(s==1)state.store(2,std::memory_order_release);
}
bool pop_capture_task(){
    static bool stopping=false,send=false,prepared=false;
    static char header[768];static unsigned header_len=0,header_pos=0,pcm_pos=0;
    static audio_diagnostics_t audio{};static synth_diagnostics_t synth{};
    while(tud_cdc_available()){
        int c=tud_cdc_read_char();
        if(c=='c'){
            if(state.load(std::memory_order_acquire)==0)state.store(1,std::memory_order_relaxed);
            send=true;header_pos=pcm_pos=0; // Repeat c resends the frozen capture.
        }
    }
    if(state.load(std::memory_order_acquire)!=2)return false;
    if(!stopping){
        audio_get_diagnostics(&audio);synth_get_diagnostics(&synth);
        synth_request_suspend(true);stopping=true;
    }
    synth_diagnostics_t current;synth_get_diagnostics(&current);
    if(!current.suspended || !send || !tud_cdc_connected())return true;
    if(!prepared){
        header_len=snprintf(header,sizeof(header),
            "{\"format\":\"DODEPAN_PCM1\",\"firmware\":\"%s\",\"source\":\"%s\",\"engine\":\"%s\",\"rate\":%u,\"frames\":%u,\"first_frame\":%llu,\"crc32\":\"%08lx\",\"clock_khz\":%u,\"buffer_frames\":%u,\"U\":%lu,\"L\":%lu,\"I\":%lu,\"queue_losses\":%lu,\"peak_render_us\":%lu,\"period_us\":%lu}\n",
            PROGRAM_VERSION,DODEPAN_SOURCE_ID,DODEPAN_ENGINE_ID,SOUND_OUTPUT_FREQUENCY,unsigned(ring.count),
            (unsigned long long)(ring.total-ring.count),(unsigned long)ring.crc(),SYSTEM_CLOCK_KHZ,AUDIO_BUFFER_LENGTH,
            (unsigned long)audio.underruns,(unsigned long)audio.late_publications,(unsigned long)audio.irq_deadline_misses,
            (unsigned long)synth.queue_overflows,(unsigned long)synth.max_render_us,(unsigned long)audio.buffer_period_us);
        if(header_len>=sizeof(header)){send=false;return true;}
        prepared=true;
    }
    // Export is bounded and begins only after the audio core confirms suspension.
    unsigned available=std::min<unsigned>(tud_cdc_write_available(),64);
    if(header_pos<header_len){header_pos+=tud_cdc_write(header+header_pos,std::min(available,header_len-header_pos));}
    else if(pcm_pos<ring.count*2){
        uint8_t bytes[64];unsigned n=std::min<unsigned>(available,ring.count*2-pcm_pos);
        for(unsigned i=0;i<n;++i)bytes[i]=ring.byte_at(pcm_pos+i);
        pcm_pos+=tud_cdc_write(bytes,n);
    }else send=false;
    tud_cdc_write_flush();return true;
}
#endif
