> Superseded for sound testing: the user reported distortion, wrong pitch, and excessive reverb with rc1. Use the legacy-sound candidate described in [SOUND_RESTORE.md](SOUND_RESTORE.md). The rc1 validation below is historical software evidence, not sound acceptance.

# Staged repair implementation — 2.6.0-rc1

Batches 1–3 are implemented as a Pico 2 diagnostic candidate. Host checks and builds validate the software changes; they do not establish that hardware crackling is fixed. The customized PRA32-U2 v1.6.0 engine remains in place. Batch 4 (v2.18.2 migration) remains deferred under the approved sound and hardware gates.

## Changes by batch

| Batch | Changes | Review interfaces |
| --- | --- | --- |
| 1: Audio | Correct the 64-cycle I2S divider to nominal 48 kHz; three explicitly owned buffers; hardware-chained immutable-silence fallback; prefill/start/stop; coherent timing and underrun counters; ramp output gain | `lib/sound_i2s/{audio_buffers.h,sound_i2s.c,sound_i2s.h,sound_i2s_16bits.pio}`, `synth_service.cpp` |
| 2: Concurrency and persistence | Core 1 exclusively owns the engine; bounded 256-command queue with adjacent-control coalescing and overflow panic/resynchronization; atomic 48-parameter preset changes and capture; validated legacy decoding; alternating CRC/generation records; quiet-tail save acknowledgement and SDK flash coordination | `synth_service.{h,cpp}`, `settings_store.{h,c}`, `storage_reservation.ld`, `main.cpp`, `instrument_preset.h`, `CMakeLists.txt` |
| 3: Reliability | Source-specific note and sustain ownership; held-pad release routing; balanced root-inclusive tilt transitions; loop boundary/capacity/duration repairs; 64-bit arp and demo scheduling; bounded MIDI parsing; USB packet retry/overflow handling; timed touch release; safe IMU magnitude/fault fallback; accumulated encoder detents | `note_router.{h,c}`, `main.cpp`, `looper.c`, `arpeggiator.c`, `demo_player.c`, `touch.c`, `imu.c`, `sensor_math.h`, `state.c`, `display/display.c` |

The original audit findings and approved acceptance criteria remain in `../audit-2026-09-13/`. Review this candidate against the pre-repair snapshot, not just Git HEAD: the working copy already contained substantial user changes. `verification.json` records hashes and preservation checks. No Git commit, reset, checkout, or device flash was performed.

## Validation

See `VALIDATION.md` for the final build sizes, hashes, and counts. Portable regressions execute the actual settings, buffer-pool, note-router, looper, arp, and demo modules. Control, queue, and touch tests extract exact firmware function bodies and replace hardware/engine sinks. Those tests verify ordering and state transitions; they do not simulate DMA hardware, SDK spinlocks, flash electrical behavior, or DSP timing.

Coverage includes all nine embedded demos, tempo changes, truncated tracks/overlong VLQs, 32-bit clock crossing, exact loop-end releases, long missed cycles, capacity-reserved note-offs, 256 repeated loop notes, shared sustain/pitches, held mode changes, every tilt zone, command saturation, atomic capture, touch release timing and bus loss, full 48-parameter records, legacy slot preservation, generation wrap, every truncated record prefix, and corruption of each record byte covered by CRC. Structural checks execute the PIO instruction loop and calculate 64 cycles / 32 bits per frame: 48,000 Hz LRCLK and 1,536,000 Hz BCLK at the default clock.

## Intentional behavior changes and compatibility

- The original divider emitted 24 kHz while the synth generated for 48 kHz. Correcting it changes audible pitch and time relative to that baseline (nominal pitch doubles). Existing patch data and DSP code are preserved; identical audio to the incorrect-rate build is not promised. Compare the six custom instruments and four saved slots by ear before release.
- Instrument IDs are now consistently custom 0–5, factory 6–13, user 14–17. Legacy slot bytes are retained by physical slot position. The firmware cannot reconstruct which slot a user intended when the former mapping was wrong.
- Legacy `DODE` is 244 bytes at flash offset `0x1ff000`. Its four 45-byte presets and four 12-byte scales remain untouched. Missing preset fields are delay feedback=0, delay time=127, delay mode=1 (the current Dodepan defaults).
- New `DOD2` version 1 occupies a 512-byte programmed record in alternating 4 KiB sectors at `0x3fe000` and `0x3ff000`. Its checked length is 263 bytes: 16-byte header plus 247-byte payload. CRC covers header bytes 0–11 and payload 16–262; CRC bytes 12–15 and erased padding are excluded. Boot selects the newest valid generation, then tries legacy, then safe defaults. Migration never erases the legacy sector. Downgrading reads the old legacy settings and will not see edits made only in the new journal.
- Saves remain pending (`*` on the display) during held touches, playback, recording, sustain, or a loud tail. After 250 ms below absolute sample level 32, the renderer ramps to zero, acknowledges stopped DMA/PIO, and permits a verified inactive-sector write. Powering off while `*` is present can lose pending edits. Software interruption tests are not an electrical power-loss test.
- Overlapping owners of the same pitch share one synth/MIDI note. Removing one owner does not release another. This coalesces retriggers/velocities for overlapping equal pitches. Sustain is combined across sources; other synth controllers and pitch bend remain global, with ordered last-writer behavior. Preset changes release current voices transactionally.
- Key, scale, chord, and arp-pattern changes release live notes and require a fresh touch. Tilt retains common notes and balances changed notes; the former assumed voice-specific portamento morph is removed. Voice stealing still follows the existing four-voice engine.
- Queue or USB output overflow is a failed diagnostic run. It increments a counter and stops/resynchronizes active sources, rather than silently losing releases. USB congestion can still delay external MIDI; latency must be measured.
- Touch release uses a provisional 40 ms elapsed-time window. Short read failures retain state; persistent failures release it. IMU failures retain last data briefly, then expose unavailable state and neutral controls. The interval and missing-sensor behavior still need hardware checks.

## Remaining gates

Follow `DEVICE_TESTS.md`. No LRCLK/BCLK trace, speaker recording, processing-headroom measurement, delayed-save listening test, raw-flash power-loss test, or 30-minute device run has been performed here. The 288 MHz default remains an experimental comparison point, not a validated production clock. Select a clock only after measuring the real workload. The larger buffer adds latency and cannot solve inadequate average CPU capacity.

No engine migration or DSP backport is approved by these build results. Reassess v2.18.2 only after device gates and a representative custom-instrument/saved-preset comparison.
