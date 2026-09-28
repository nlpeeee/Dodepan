#pragma once
#include <stdint.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
struct sound_i2s_config {
    uint8_t pio_num, pin_scl, pin_sda, pin_ws;
    uint16_t sample_rate;
    uint8_t bits_per_sample;
    bool swap;
    uint16_t samples_per_buffer;
};
typedef struct { int16_t *samples; uint32_t generation; int id; } audio_buffer_t;
typedef struct {
    uint32_t system_clock_hz, frame_rate_millihz, buffer_period_us;
    uint64_t observed_frames, dma_notifications;
    uint32_t underruns, late_publications, irq_deadline_misses;
    uint32_t max_render_us, max_irq_gap_us;
    bool running;
} audio_diagnostics_t;
int sound_i2s_init(const struct sound_i2s_config *config);
bool audio_acquire_buffer(audio_buffer_t *buffer);
bool audio_publish_buffer(audio_buffer_t *buffer, uint32_t render_us);
bool audio_start(void);
/* Audio owner only; stop must precede any flash lockout. */
void audio_request_stop(void);
void audio_get_diagnostics(audio_diagnostics_t *out);
#ifdef __cplusplus
}
#endif
