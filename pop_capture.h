#pragma once
#include <stdint.h>
#if DODEPAN_POP_CAPTURE
void pop_capture_audio(const int16_t *stereo, unsigned frames);
bool pop_capture_task(); // Core 0: true parks normal controls until reboot.
#else
inline void pop_capture_audio(const int16_t *, unsigned) {}
inline bool pop_capture_task(){return false;}
#endif
