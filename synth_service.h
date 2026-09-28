#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "instrument_preset.h"
struct synth_diagnostics_t {
    uint32_t queue_high_water, queue_overflows, max_render_us;
    uint64_t render_total_us,render_buffers;
    uint32_t quiet_ms;
    bool suspended, ready;
};
class SynthControl {
public:
    void initialize();
    void note_on(uint8_t note,uint8_t velocity);
    void note_off(uint8_t note);
    void control_change(uint8_t cc,uint8_t value);
    void pitch_bend(uint8_t lsb,uint8_t msb);
    void program_change(uint8_t program);
    void apply_preset(const uint8_t *values);
    void apply_custom_preset(uint8_t id);
    void all_notes_off();
    uint8_t current_controller_value(uint8_t cc);
    void capture_preset(uint8_t slot);
};
extern SynthControl g_synth;
void synth_audio_main();
void synth_set_volume(uint8_t volume);
void synth_request_suspend(bool suspend);
bool synth_take_capture(uint8_t *slot,uint8_t *values);
void synth_get_diagnostics(synth_diagnostics_t *out);
