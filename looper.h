#ifndef LOOPER_H
#define LOOPER_H
#include "pico/stdlib.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LOOP_DISABLED = 0,
    LOOP_IDLE,
    LOOP_RECORDING,
    LOOP_PLAYING,
    LOOP_PAUSED,
} looper_state_t;

typedef enum {
    LOOPER_EVENT_NOTE_ON = 0,
    LOOPER_EVENT_NOTE_OFF = 1,
    LOOPER_EVENT_CC = 2,
    LOOPER_EVENT_PITCH = 3,
} looper_event_type_t;

typedef struct {
    uint32_t timestamp_ms;  // Time since start of loop
    looper_event_type_t type;
    uint8_t data1;          // note or cc number
    uint8_t data2;          // velocity or cc value
    int16_t pitch;          // pitch bend (-8192..8191) when type == LOOPER_EVENT_PITCH
} looper_event_t;

typedef struct looper {
    looper_state_t state;
    looper_event_t *events;
    uint16_t event_count;
    uint16_t max_events;
    uint16_t play_index;
    uint32_t loop_length_ms;   // Duration of loop in ms
    uint32_t max_length_ms;    // Clamp for loop length
    uint64_t rec_start_us;
    uint64_t play_start_us;
    bool has_loop;
} looper_t;

void looper_init(uint16_t max_events, uint32_t max_length_ms);
void looper_onpress();
void looper_enable();
void looper_disable();
void looper_clear();
bool looper_is_disabled();
bool looper_is_recording();
bool looper_is_playing();
bool looper_has_loop();
bool looper_has_events();
void looper_start_record();
void looper_stop_record_and_play();
void looper_stop();
void looper_record_note(uint8_t note, uint8_t velocity, bool is_on);
void looper_record_cc(uint8_t cc_number, uint8_t value);
void looper_record_pitch(int16_t pitch_bend);
void looper_restart_from_start();
uint32_t looper_get_loop_length_ms();
uint16_t looper_get_event_count();
uint16_t looper_get_play_index();
looper_state_t looper_get_state();
uint32_t looper_get_elapsed_ms();
uint32_t looper_get_max_length_ms();
void looper_task();

// Looper playback hooks - implemented in main.cpp with C linkage
void looper_send_note_on(uint8_t note, uint8_t velocity);
void looper_send_note_off(uint8_t note);
void looper_send_cc(uint8_t cc_number, uint8_t value);
void looper_send_pitch(int16_t pitch_bend);
void all_notes_off(void);

// Legacy compatibility (kept for display/UI expectations)
static inline bool looper_has_recording() { return looper_has_loop(); }

#ifdef __cplusplus
}
#endif

#endif