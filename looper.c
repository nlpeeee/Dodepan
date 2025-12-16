#include "pico/stdlib.h"
#include <stdlib.h>
#include "config.h"
#include "state.h"
#include "looper.h"
#include "display/display.h"

// Display refresh interval in milliseconds
#define LOOPER_DISPLAY_REFRESH_MS 100

// Single global looper instance
static looper_t looper;

static void looper_clear_internal(void) {
    looper.event_count = 0;
    looper.play_index = 0;
    looper.loop_length_ms = 0;
    looper.has_loop = false;
}

static void looper_restart_playback(void) {
    looper.play_index = 0;
    looper.play_start_us = time_us_64();
    looper.state = LOOP_PLAYING;
}

void looper_init(uint16_t max_events, uint32_t max_length_ms) {
    looper.state = LOOP_DISABLED;
    looper.events = (looper_event_t *)calloc(max_events, sizeof(looper_event_t));
    
    if (!looper.events) {
        looper.max_events = 0;
        looper.max_length_ms = 0;
        return;
    }
    
    looper.max_events = max_events;
    looper.max_length_ms = max_length_ms;
    looper_clear_internal();
}

void looper_enable() {
    if (looper.events && looper_is_disabled()) {
        looper.state = LOOP_IDLE;
    }
}

void looper_disable() {
    looper.state = LOOP_DISABLED;
    looper_clear_internal();
    all_notes_off();
}

void looper_clear() {
    looper_clear_internal();
    if (!looper_is_disabled()) {
        looper.state = LOOP_IDLE;
    }
}

bool looper_is_disabled() {
    return (looper.state == LOOP_DISABLED || looper.events == NULL);
}

bool looper_is_recording() {
    return (looper.state == LOOP_RECORDING);
}

bool looper_is_playing() {
    return (looper.state == LOOP_PLAYING);
}

bool looper_has_loop() {
    return looper.has_loop && looper.loop_length_ms > 0 && looper.event_count > 0;
}

bool looper_has_events() {
    return looper.event_count > 0;
}

// Display throttle using 32-bit timestamps (faster, 71min wrap is fine for throttle)
static uint32_t last_display_refresh_ms_rec = 0;
static uint32_t last_display_refresh_ms_play = 0;

void looper_start_record() {
    if (looper_is_disabled()) { return; }
    looper_clear_internal();
    looper.rec_start_us = time_us_64();
    last_display_refresh_ms_rec = 0;  // Reset display throttle
    last_display_refresh_ms_play = 0;
    looper.state = LOOP_RECORDING;
}

void looper_stop_record_and_play() {
    if (looper_is_disabled()) { return; }
    if (looper.state != LOOP_RECORDING) { return; }

    uint64_t now_us = time_us_64();
    uint32_t elapsed_ms = (uint32_t)((now_us - looper.rec_start_us) / 1000);
    if (elapsed_ms == 0) {
        looper_clear();
        return;
    }
    if (elapsed_ms > looper.max_length_ms) {
        elapsed_ms = looper.max_length_ms;
    }
    looper.loop_length_ms = elapsed_ms;
    looper.has_loop = (looper.event_count > 0);

    if (looper.has_loop) {
        looper_restart_playback();
    } else {
        looper.state = LOOP_IDLE;
    }
}

void looper_stop() {
    if (looper_is_disabled()) { return; }
    looper.state = looper_has_loop() ? LOOP_PAUSED : LOOP_IDLE;
}

void looper_restart_from_start() {
    if (looper_is_disabled()) { return; }
    if (!looper_has_loop()) { return; }
    all_notes_off();
    looper.play_index = 0;
    looper.play_start_us = time_us_64();
    looper.state = LOOP_PLAYING;
}

void looper_onpress() {
    switch (looper.state) {
        case LOOP_DISABLED:
            return;
        case LOOP_IDLE:
        case LOOP_PAUSED:
            looper_start_record();
            break;
        case LOOP_RECORDING:
            looper_stop_record_and_play();
            break;
        case LOOP_PLAYING:
            looper_stop();
            break;
    }
}

static void looper_append_event(looper_event_type_t type, uint8_t d1, uint8_t d2, int16_t pitch) {
    // Combined check: must be recording with valid events buffer
    if (looper.state != LOOP_RECORDING || looper.events == NULL) return;
    if (looper.event_count >= looper.max_events) return; // Drop extra events silently

    uint64_t now_us = time_us_64();
    uint32_t ts_ms = (uint32_t)((now_us - looper.rec_start_us) / 1000);
    if (ts_ms > looper.max_length_ms) {
        looper_stop_record_and_play();
        return;
    }

    looper_event_t *evt = &looper.events[looper.event_count++];
    evt->timestamp_ms = ts_ms;
    evt->type = type;
    evt->data1 = d1;
    evt->data2 = d2;
    evt->pitch = pitch;
}

void looper_record_note(uint8_t note, uint8_t velocity, bool is_on) {
    // Early exit when disabled - avoid any work in hot path
    if (looper_is_disabled()) return;
    
    if (looper.state == LOOP_IDLE || looper.state == LOOP_PAUSED) {
        looper_start_record();
    }
    looper_append_event(is_on ? LOOPER_EVENT_NOTE_ON : LOOPER_EVENT_NOTE_OFF, note, velocity, 0);
}

void looper_record_cc(uint8_t cc_number, uint8_t value) {
    if (looper_is_disabled()) return;
    looper_append_event(LOOPER_EVENT_CC, cc_number, value, 0);
}

void looper_record_pitch(int16_t pitch_bend) {
    if (looper_is_disabled()) return;
    looper_append_event(LOOPER_EVENT_PITCH, 0, 0, pitch_bend);
}

static void looper_dispatch_event(const looper_event_t *evt) {
    switch (evt->type) {
        case LOOPER_EVENT_NOTE_ON:
            looper_send_note_on(evt->data1, evt->data2);
            break;
        case LOOPER_EVENT_NOTE_OFF:
            looper_send_note_off(evt->data1);
            break;
        case LOOPER_EVENT_CC:
            looper_send_cc(evt->data1, evt->data2);
            break;
        case LOOPER_EVENT_PITCH:
            looper_send_pitch(evt->pitch);
            break;
        default:
            break;
    }
}

void looper_task() {
    // Early exit when disabled - avoid all work including time_us_64 call
    if (looper_is_disabled()) return;

    // Refresh display periodically during recording to advance the clock
    if (looper.state == LOOP_RECORDING) {
        uint32_t now_ms = time_us_32() / 1000;
        if ((now_ms - last_display_refresh_ms_rec) >= LOOPER_DISPLAY_REFRESH_MS) {
            display_request_refresh();
            last_display_refresh_ms_rec = now_ms;
        }
        return;
    }

    if (!looper_is_playing() || !looper_has_loop()) {
        return;
    }

    uint64_t now_us = time_us_64();
    uint32_t elapsed_ms = (uint32_t)((now_us - looper.play_start_us) / 1000);

    // Restart loop when reaching end
    if (elapsed_ms >= looper.loop_length_ms) {
        looper_restart_playback();
        elapsed_ms = 0;
    }

    // Dispatch any due events
    while (looper.play_index < looper.event_count) {
        const looper_event_t *evt = &looper.events[looper.play_index];
        if (evt->timestamp_ms <= elapsed_ms) {
            looper_dispatch_event(evt);
            looper.play_index++;
        } else {
            break;
        }
    }

    uint32_t now_ms = time_us_32() / 1000;
    if ((now_ms - last_display_refresh_ms_play) >= LOOPER_DISPLAY_REFRESH_MS) {
        display_request_refresh();
        last_display_refresh_ms_play = now_ms;
    }
}

uint32_t looper_get_loop_length_ms() {
    return looper.loop_length_ms;
}

uint16_t looper_get_event_count() {
    return looper.event_count;
}

uint16_t looper_get_play_index() {
    return looper.play_index;
}

looper_state_t looper_get_state() {
    return looper.state;
}

uint32_t looper_get_elapsed_ms() {
    if (looper.state == LOOP_PLAYING && looper.loop_length_ms > 0) {
        return (uint32_t)((time_us_64() - looper.play_start_us) / 1000);
    }
    if (looper.state == LOOP_RECORDING) {
        return (uint32_t)((time_us_64() - looper.rec_start_us) / 1000);
    }
    return 0;
}

uint32_t looper_get_max_length_ms() {
    return looper.max_length_ms;
}