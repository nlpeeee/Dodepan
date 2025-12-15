#ifndef ARPEGGIATOR_H
#define ARPEGGIATOR_H

#include "pico/stdlib.h"

#ifdef __cplusplus
extern "C" {
#endif

// Speed intervals in milliseconds
#define ARP_INTERVAL_SLOW   750     // ~80 BPM
#define ARP_INTERVAL_MEDIUM 500     // 120 BPM
#define ARP_INTERVAL_FAST   375     // 160 BPM
#define ARP_INTERVAL_VFAST  250     // 240 BPM

// Gate length as percentage of interval (80%)
#define ARP_GATE_PERCENT    80

// Initialize the arpeggiator
void arpeggiator_init();

// Called when a pad is pressed - adds note to arp sequence
void arpeggiator_pad_on(uint8_t pad_id, uint8_t velocity);

// Called when a pad is released - removes note from arp sequence
void arpeggiator_pad_off(uint8_t pad_id);

// Stop all arpeggiated notes and reset state
void arpeggiator_stop();

// Check if arpeggiator is currently active (pattern != OFF and has held pads)
bool arpeggiator_is_active();

// Check if arpeggiator is enabled (pattern != OFF)
bool arpeggiator_is_enabled();

// Main task - call from main loop to process arp timing
void arpeggiator_task();

#ifdef __cplusplus
}
#endif

#endif
