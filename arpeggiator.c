#include "pico/stdlib.h"
#include <stdlib.h>
#include "config.h"
#include "state.h"
#include "looper.h"
#include "arpeggiator.h"

// External functions from main.cpp for playing notes
extern void play_single_note(uint8_t note, uint8_t velocity);
extern void stop_single_note(uint8_t note);

// Arpeggiator state
typedef struct {
    uint16_t held_pads;         // Bitmask of currently held pads (bits 0-11)
    uint8_t held_count;         // Number of held pads
    uint8_t held_order[12];     // Pads in order they were pressed (for sorting)
    uint8_t velocity;           // Velocity to use for arpeggiated notes
    
    uint8_t current_index;      // Current position in the arp sequence
    uint8_t current_octave;     // Current octave offset (0, 1, or 2)
    int8_t direction;           // 1 = ascending, -1 = descending (for up-down)
    
    uint8_t current_note;       // Currently playing note (for note_off)
    uint8_t current_pad;        // Pad ID of currently playing note (for looper recording)
    bool note_playing;          // Is a note currently sounding?
    
    uint32_t last_step_time;    // Timestamp of last step
    uint32_t note_on_time;      // Timestamp when current note started
} arpeggiator_t;

static arpeggiator_t arp;

// Get interval in milliseconds from state
static uint32_t get_interval_ms() {
    uint16_t speed = get_arp_speed_ms();
    // Ensure valid range
    if (speed < ARP_SPEED_MIN) return ARP_SPEED_MIN;
    if (speed > ARP_SPEED_MAX) return ARP_SPEED_MAX;
    return speed;
}

// Get the sorted list of held pads (low to high by note)
static void get_sorted_pads(uint8_t *sorted, uint8_t *count) {
    *count = 0;
    
    // Collect held pads
    for (uint8_t i = 0; i < 12; i++) {
        if (arp.held_pads & (1 << i)) {
            sorted[(*count)++] = i;
        }
    }
    
    // Sort by note value (bubble sort is fine for max 12 items)
    // Guard against count < 2 to avoid underflow
    if (*count < 2) return;
    
    for (uint8_t i = 0; i < *count - 1; i++) {
        for (uint8_t j = 0; j < *count - i - 1; j++) {
            uint8_t note_a = get_note_by_id(sorted[j]);
            uint8_t note_b = get_note_by_id(sorted[j + 1]);
            if (note_a > note_b) {
                uint8_t temp = sorted[j];
                sorted[j] = sorted[j + 1];
                sorted[j + 1] = temp;
            }
        }
    }
}

// Get the next pad in the sequence based on pattern
static bool get_next_step(uint8_t *pad_id, uint8_t *octave_offset) {
    if (arp.held_count == 0) return false;
    
    uint8_t sorted[12];
    uint8_t count;
    get_sorted_pads(sorted, &count);
    
    if (count == 0) return false;
    
    uint8_t pattern = get_arp_pattern();
    uint8_t max_octaves = get_arp_octave() + 1;  // 1, 2, or 3
    
    // Clamp current_index to valid range (in case pads were released)
    if (arp.current_index >= count) {
        arp.current_index = 0;
    }
    
    switch (pattern) {
        case ARP_UP:
            *pad_id = sorted[arp.current_index];
            *octave_offset = arp.current_octave;
            
            // Advance to next
            arp.current_index++;
            if (arp.current_index >= count) {
                arp.current_index = 0;
                arp.current_octave++;
                if (arp.current_octave >= max_octaves) {
                    arp.current_octave = 0;
                }
            }
            break;
            
        case ARP_DOWN:
            *pad_id = sorted[count - 1 - arp.current_index];
            *octave_offset = (max_octaves - 1) - arp.current_octave;
            
            // Advance to next
            arp.current_index++;
            if (arp.current_index >= count) {
                arp.current_index = 0;
                arp.current_octave++;
                if (arp.current_octave >= max_octaves) {
                    arp.current_octave = 0;
                }
            }
            break;
            
        case ARP_UP_DOWN:
            if (arp.direction == 1) {
                // Going up
                *pad_id = sorted[arp.current_index];
                *octave_offset = arp.current_octave;
                
                arp.current_index++;
                if (arp.current_index >= count) {
                    arp.current_index = 0;
                    arp.current_octave++;
                    if (arp.current_octave >= max_octaves) {
                        // Reached top, reverse direction
                        arp.direction = -1;
                        arp.current_octave = max_octaves - 1;
                        arp.current_index = count > 1 ? count - 2 : 0;  // Skip the top note (already played)
                    }
                }
            } else {
                // Going down
                *pad_id = sorted[arp.current_index];
                *octave_offset = arp.current_octave;
                
                if (arp.current_index == 0) {
                    if (arp.current_octave == 0) {
                        // Reached bottom, reverse direction
                        arp.direction = 1;
                        arp.current_index = count > 1 ? 1 : 0;  // Skip the bottom note (already played)
                        arp.current_octave = 0;
                    } else {
                        arp.current_octave--;
                        arp.current_index = count - 1;
                    }
                } else {
                    arp.current_index--;
                }
            }
            break;
            
        case ARP_RANDOM:
            *pad_id = sorted[rand() % count];
            *octave_offset = rand() % max_octaves;
            break;
            
        default:
            return false;
    }
    
    return true;
}

// Play the next step in the arpeggio
static void play_next_step() {
    uint8_t pad_id;
    uint8_t octave_offset;
    
    if (!get_next_step(&pad_id, &octave_offset)) {
        return;
    }
    
    // Stop previous note if playing
    if (arp.note_playing) {
        stop_single_note(arp.current_note);
        arp.note_playing = false;
    }
    
    // Calculate note with octave offset
    uint8_t base_note = get_note_by_id(pad_id);
    uint8_t note = base_note + (octave_offset * 12);
    
    // Ensure note is in valid MIDI range
    if (note > 127) note = 127;
    
    // Play the note
    play_single_note(note, arp.velocity);
    arp.current_note = note;
    arp.current_pad = pad_id;
    arp.note_playing = true;
    arp.note_on_time = time_us_32() / 1000;  // Convert to ms

    // Record to looper using the actual note value
    looper_record_note(note, arp.velocity, true);
}

// Stop the current note (for gate timing)
static void stop_current_note() {
    if (arp.note_playing) {
        stop_single_note(arp.current_note);
        // Record note off with the actual note value
        looper_record_note(arp.current_note, 0, false);
        arp.note_playing = false;
    }
}

void arpeggiator_init() {
    arp.held_pads = 0;
    arp.held_count = 0;
    arp.current_index = 0;
    arp.current_octave = 0;
    arp.direction = 1;
    arp.current_note = 0;
    arp.current_pad = 0;
    arp.note_playing = false;
    arp.last_step_time = 0;
    arp.note_on_time = 0;
    arp.velocity = 100;
    
    // Initialize default arp speed if not set
    if (get_arp_speed_ms() == 0) {
        set_arp_speed_ms(ARP_SPEED_DEFAULT);
    }
}

void arpeggiator_pad_on(uint8_t pad_id, uint8_t velocity) {
    if (pad_id >= 12) return;
    
    // Add pad to held set
    if (!(arp.held_pads & (1 << pad_id))) {
        arp.held_pads |= (1 << pad_id);
        arp.held_order[arp.held_count++] = pad_id;
    }
    
    // Store velocity
    arp.velocity = velocity;
    
    // If this is the first pad, start immediately
    if (arp.held_count == 1) {
        arp.current_index = 0;
        arp.current_octave = 0;
        arp.direction = 1;
        arp.last_step_time = time_us_32() / 1000;
        play_next_step();
    }
}

void arpeggiator_pad_off(uint8_t pad_id) {
    if (pad_id >= 12) return;
    
    // Remove pad from held set
    if (arp.held_pads & (1 << pad_id)) {
        arp.held_pads &= ~(1 << pad_id);
        arp.held_count--;
        
        // Remove from held_order array
        for (uint8_t i = 0; i < 12; i++) {
            if (arp.held_order[i] == pad_id) {
                // Shift remaining elements
                for (uint8_t j = i; j < 11; j++) {
                    arp.held_order[j] = arp.held_order[j + 1];
                }
                break;
            }
        }
    }
    
    // If no more pads held, stop
    if (arp.held_count == 0) {
        stop_current_note();
        arp.current_index = 0;
        arp.current_octave = 0;
        arp.direction = 1;
    }
}

void arpeggiator_stop() {
    stop_current_note();
    arp.held_pads = 0;
    arp.held_count = 0;
    arp.current_index = 0;
    arp.current_octave = 0;
    arp.direction = 1;
}

bool arpeggiator_is_active() {
    return (get_arp_pattern() != ARP_OFF) && (arp.held_count > 0);
}

bool arpeggiator_is_enabled() {
    return get_arp_pattern() != ARP_OFF;
}

void arpeggiator_task() {
    // Only process if arpeggiator is enabled and has held pads
    if (get_arp_pattern() == ARP_OFF || arp.held_count == 0) {
        return;
    }
    
    uint32_t now = time_us_32() / 1000;  // Current time in ms
    uint32_t interval = get_interval_ms();
    uint32_t gate_time = (interval * ARP_GATE_PERCENT) / 100;
    
    // Check if we need to stop the current note (gate off)
    if (arp.note_playing && (now - arp.note_on_time >= gate_time)) {
        stop_current_note();
    }
    
    // Check if it's time for the next step
    if (now - arp.last_step_time >= interval) {
        arp.last_step_time = now;
        play_next_step();
    }
}
