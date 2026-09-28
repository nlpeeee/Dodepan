#include <string.h>
#include "pico/stdlib.h"
#include "pico/critical_section.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "sound_i2s.h"
#include "audio_buffers.h"
#include "sound_i2s_16bits.pio.h"

#define MAX_AUDIO_FRAMES 256
static uint32_t buffers[AUDIO_POOL_COUNT][MAX_AUDIO_FRAMES];
static const uint32_t silence[MAX_AUDIO_FRAMES];
static const uintptr_t silence_address=(uintptr_t)silence;
/* Written by IRQ and cleared by DMA. Single aligned word publication. */
static volatile uintptr_t next_address;
static audio_pool_t pool;
static struct sound_i2s_config config;
static audio_diagnostics_t diagnostics;
static critical_section_t lock;
static PIO pio;
static int sm=-1, data_chan=-1, select_chan=-1, clear_chan=-1;
static uint program_offset;
static uint64_t last_irq_us;
static bool initialized;

static int buffer_at(uintptr_t address, bool advancing) {
    for(int i=0;i<AUDIO_POOL_COUNT;i++)if(address==(uintptr_t)buffers[i])return i;
    for(int i=0;i<AUDIO_POOL_COUNT;i++) {
        uintptr_t start=(uintptr_t)buffers[i];
        if(address==start || (advancing && address>start && address<=start+config.samples_per_buffer*4u)) return i;
    }
    return AUDIO_NO_BUFFER;
}
static void __isr __time_critical_func(audio_dma_irq)(void) {
    uint32_t mask=1u<<clear_chan;
    if(!(dma_hw->ints0&mask))return;
    dma_hw->ints0=mask;
    critical_section_enter_blocking(&lock);
    if(!diagnostics.running){critical_section_exit(&lock);return;}
    uint64_t now=time_us_64();
    uint32_t gap=last_irq_us?(uint32_t)(now-last_irq_us):0;
    if(gap>diagnostics.max_irq_gap_us)diagnostics.max_irq_gap_us=gap;
    if(gap>diagnostics.buffer_period_us*3u/2u)diagnostics.irq_deadline_misses++;
    last_irq_us=now;
    /* Read pending first: select DMA writes data.read_addr BEFORE clearing
       next_address. This order conservatively retains every live reference.
       Only this highest-priority core-1 IRQ publishes DMA addresses. */
    uintptr_t pending=next_address;
    __dmb();
    uintptr_t current=dma_hw->ch[data_chan].read_addr;
    audio_pool_reclaim(&pool,buffer_at(current,true),buffer_at(pending,false));
    diagnostics.dma_notifications++;
    if(gap)diagnostics.observed_frames+=config.samples_per_buffer;
    if(buffer_at(current,true)<0)diagnostics.underruns++;
    if(next_address==silence_address) {
        int next=audio_pool_take_ready(&pool);
        if(next>=0){__dmb();next_address=(uintptr_t)buffers[next];}
    }
    critical_section_exit(&lock);
}

int sound_i2s_init(const struct sound_i2s_config *cfg) {
    if(initialized || !cfg || !cfg->sample_rate || !cfg->samples_per_buffer || cfg->samples_per_buffer>MAX_AUDIO_FRAMES || cfg->bits_per_sample!=16)return -1;
    config=*cfg;pio=cfg->pio_num?pio1:pio0;
    sm=pio_claim_unused_sm(pio,false);
    data_chan=dma_claim_unused_channel(false);
    select_chan=dma_claim_unused_channel(false);
    clear_chan=dma_claim_unused_channel(false);
    if(sm<0 || data_chan<0 || select_chan<0 || clear_chan<0 || !pio_can_add_program(pio,&sound_i2s_16bits_program)) {
        if(sm>=0)pio_sm_unclaim(pio,sm);
        if(data_chan>=0)dma_channel_unclaim(data_chan);
        if(select_chan>=0)dma_channel_unclaim(select_chan);
        if(clear_chan>=0)dma_channel_unclaim(clear_chan);
        return -1;
    }
    critical_section_init(&lock);
    program_offset=pio_add_program(pio,&sound_i2s_16bits_program);
    sound_i2s_16bits_program_init(pio,sm,program_offset,cfg->sample_rate,cfg->pin_sda,cfg->pin_scl);
    diagnostics.system_clock_hz=clock_get_hz(clk_sys);
    uint32_t raw=pio->sm[sm].clkdiv;
    uint32_t divisor256=(raw>>16)*256+((raw>>8)&255);
    diagnostics.frame_rate_millihz=(uint32_t)(((uint64_t)clock_get_hz(clk_sys)*256*1000)/(divisor256*64u));
    diagnostics.buffer_period_us=(uint32_t)(((uint64_t)cfg->samples_per_buffer*1000000000)/diagnostics.frame_rate_millihz);
    irq_set_exclusive_handler(DMA_IRQ_0,audio_dma_irq);
    irq_set_priority(DMA_IRQ_0,0);
    dma_channel_set_irq0_enabled(clear_chan,true);
    irq_set_enabled(DMA_IRQ_0,true);
    initialized=true;return 0;
}
bool audio_acquire_buffer(audio_buffer_t *b) {
    critical_section_enter_blocking(&lock);
    b->id=audio_pool_acquire(&pool,&b->generation);
    b->samples=b->id>=0?(int16_t*)buffers[b->id]:NULL;
    critical_section_exit(&lock);return b->id>=0;
}
bool audio_publish_buffer(audio_buffer_t *b,uint32_t render_us) {
    critical_section_enter_blocking(&lock);
    if(render_us>diagnostics.max_render_us)diagnostics.max_render_us=render_us;
    bool ok=audio_pool_publish(&pool,b->id,b->generation);
    if(!ok || render_us>diagnostics.buffer_period_us)diagnostics.late_publications++;
    __dmb();critical_section_exit(&lock);b->samples=NULL;return ok;
}
bool audio_start(void) {
    critical_section_enter_blocking(&lock);
    if(diagnostics.running){critical_section_exit(&lock);return false;}
    int first=audio_pool_take_ready(&pool);
    if(first<0){critical_section_exit(&lock);return false;}
    next_address=(uintptr_t)buffers[first];
    /* data -> select -> clear -> data. The selected descriptor is reset to
       immutable silence in hardware BEFORE data starts, even if IRQs are late.
       No DMA ever repeats a consumed sample buffer by default. */
    dma_channel_config d=dma_channel_get_default_config(data_chan);
    channel_config_set_transfer_data_size(&d,DMA_SIZE_32);
    channel_config_set_read_increment(&d,true);
    channel_config_set_dreq(&d,pio_get_dreq(pio,sm,true));
    channel_config_set_chain_to(&d,select_chan);
    dma_channel_configure(data_chan,&d,&pio->txf[sm],silence,config.samples_per_buffer,false);
    d=dma_channel_get_default_config(select_chan);
    channel_config_set_transfer_data_size(&d,DMA_SIZE_32);
    channel_config_set_read_increment(&d,false);channel_config_set_write_increment(&d,false);
    channel_config_set_chain_to(&d,clear_chan);
    dma_channel_configure(select_chan,&d,&dma_hw->ch[data_chan].read_addr,&next_address,1,false);
    d=dma_channel_get_default_config(clear_chan);
    channel_config_set_transfer_data_size(&d,DMA_SIZE_32);
    channel_config_set_read_increment(&d,false);channel_config_set_write_increment(&d,false);
    channel_config_set_chain_to(&d,data_chan);
    dma_channel_configure(clear_chan,&d,&next_address,&silence_address,1,false);
    pio_sm_clear_fifos(pio,sm);pio_sm_restart(pio,sm);
    pio_sm_exec(pio,sm,pio_encode_jmp(program_offset));pio_sm_clkdiv_restart(pio,sm);
    last_irq_us=0;diagnostics.running=true;__dmb();
    dma_start_channel_mask(1u<<select_chan);
    pio_sm_set_enabled(pio,sm,true);
    critical_section_exit(&lock);return true;
}
void audio_request_stop(void) {
    critical_section_enter_blocking(&lock);
    diagnostics.running=false;
    pio_sm_set_enabled(pio,sm,false);
    /* Disable all links before aborting any channel, so no abort can restart
       another channel in the cycle. */
    for(int i=0;i<3;i++) {
        int channel=i==0?data_chan:i==1?select_chan:clear_chan;
        hw_clear_bits(&dma_hw->ch[channel].ctrl_trig,DMA_CH0_CTRL_TRIG_EN_BITS);
    }
    dma_channel_abort(data_chan);dma_channel_abort(select_chan);dma_channel_abort(clear_chan);
    dma_hw->ints0=1u<<clear_chan;
    pio_sm_clear_fifos(pio,sm);
    memset(&pool.slots,0,sizeof(pool.slots));pool.generation++;
    next_address=silence_address;
    critical_section_exit(&lock);
}
void audio_get_diagnostics(audio_diagnostics_t *out) {
    critical_section_enter_blocking(&lock);*out=diagnostics;critical_section_exit(&lock);
}
