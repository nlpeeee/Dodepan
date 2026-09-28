#pragma once
#include <stdint.h>
#include <stdbool.h>
#define NOTE_SOURCE_ARP 12
#define NOTE_SOURCE_LOOP 13
#define NOTE_SOURCE_DEMO 14
#define NOTE_SOURCE_COUNT 15
#ifdef __cplusplus
extern "C" {
#endif
void notes_on(uint8_t source,uint8_t note,uint8_t velocity);
void notes_off(uint8_t source,uint8_t note);
void notes_control(uint8_t source,uint8_t cc,uint8_t value);
void notes_release_source(uint8_t source);
void notes_panic(void);
bool notes_any_active(void);
void note_sink_on(uint8_t note,uint8_t velocity);
void note_sink_off(uint8_t note);
void note_sink_cc(uint8_t cc,uint8_t value);
#ifdef __cplusplus
}
#endif
