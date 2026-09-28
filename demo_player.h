#ifndef DEMO_PLAYER_H
#define DEMO_PLAYER_H

#include "pico/stdlib.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DEMO_STOPPED = 0,
    DEMO_PLAYING,
} demo_state_t;

/**
 * Initialize the demo player.
 */
void demo_init(void);

/**
 * Get the current number of available demo songs.
 */
uint8_t demo_get_song_count(void);

/**
 * Get the name of a song by index.
 */
const char* demo_get_song_name(uint8_t index);

/**
 * Get the currently selected song index.
 */
uint8_t demo_get_current_song(void);

/**
 * Select a song by index (does not start playback).
 */
void demo_select_song(uint8_t index);

/**
 * Select next song (wraps around).
 */
void demo_next_song(void);

/**
 * Select previous song (wraps around).
 */
void demo_prev_song(void);

/**
 * Start playing the currently selected song.
 */
void demo_start(void);

/**
 * Stop playback.
 */
void demo_stop(void);

/**
 * Check if demo is currently playing.
 */
bool demo_is_playing(void);

/**
 * Get current playback state.
 */
demo_state_t demo_get_state(void);

/**
 * Get elapsed playback time in milliseconds.
 */
uint32_t demo_get_elapsed_ms(void);

/**
 * Get total song duration in milliseconds (estimated from MIDI data).
 */
uint32_t demo_get_duration_ms(void);

/**
 * Main task - call from main loop to dispatch MIDI events.
 */
void demo_task(void);

// Playback hooks - implemented in main.cpp with C linkage
// (Reuses the same hooks as looper)
extern void looper_send_note_on(uint8_t note, uint8_t velocity);
extern void looper_send_note_off(uint8_t note);
extern void looper_send_cc(uint8_t cc_number, uint8_t value);
extern void looper_send_pitch(int16_t pitch_bend);
extern void all_notes_off(void);

#ifdef __cplusplus
}
#endif

#endif /* DEMO_PLAYER_H */
