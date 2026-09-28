# Hardware crackling test protocol

**Status: not run on a physical device.** The produced UF2 is the audited baseline, with known defects. It is not an instrumented or repaired release. Record observations before attempting to interpret causes.

## Inventory and baseline

Assign a stable identifier to every affected Pico 2/MAX98357 unit and to one unaffected comparison unit if available. Record:

| Field | Value to enter |
| --- | --- |
| Unit ID, board/amplifier revision | |
| Source/build ID and UF2 SHA-256 | |
| Battery/USB power, battery state, USB host/cable | |
| Speaker/load and amplifier gain setting | |
| Clock and buffer configuration | |
| Instrument, volume, scale, chord/arp/loop/demo/IMU settings | |
| Physical differences and prior crackling observations | |
| Settings backup location, audio/trace filenames | |

Retain the existing flashed-firmware identity when known; use “unknown” otherwise. The archived baseline hash and newly built hash are in `baseline.json` and `build-result.json`. Keep a settings backup before changing firmware/storage. This audit did not read or write any attached device.

Use acoustic recording or a suitable differential/attenuated recording interface for the MAX98357 speaker output. If a logic analyzer is available, measure GP0 BCLK and GP1 LRCLK together with an audio-render/IRQ diagnostic pin in a later instrumented build. The current source predicts 768 kHz BCLK/24 kHz LRCLK; a repaired 16-bit path should measure 1.536 MHz/48 kHz. Without a measurement, label the rate “calculated,” not “verified.”

## Short baseline reproduction sequence

1. Boot; wait for sensor calibration and the display intro. Record idle noise for 30 seconds.
2. Play isolated notes and then four held notes with the same instrument/volume. Repeat with a sustained preset and with delay/chorus. Record the exact gesture and whether crackling occurs at onset, throughout the sound, or on release.
3. Play or run a repeatable demo while changing only display/menu activity. Repeat with IMU controls enabled/disabled; note that the baseline already skips IMU polling during demo playback, so demos alone do not exercise the live tilt path.
4. Change one persisted setting, stop further menu changes, and sustain a note for at least 15 seconds. Mark the expected flash save near 10 seconds. Repeat with all notes released. Later repaired builds may defer saving until quiet; record whether the save is pending or completed.
5. Exercise arp enable/disable while a pad is held, DOWN octave reduction, a loop ending at a release, pause/restart, and tilt-zone changes followed by release/panic. Capture MIDI output alongside audio when available.

## One-variable comparisons

Keep the same unit, gesture/event sequence, patch state, volume, and recording setup. Run each condition for two minutes before moving to the next variable; repeat any crackle reproduction at least three times.

| Variable | Comparison | Interpretation limits |
| --- | --- | --- |
| Polyphony | One note vs four | Five-note ninth chords already exceed engine voice count. |
| FX | Same patch with effects bypassed vs enabled | Confirm effective parameters; current preset serialization omits three delay fields. |
| Display | Normal vs diagnostic refresh suppression | Diagnostic suppression is a future build switch; ordinary menu inactivity does not fully disable display work. |
| IMU | Enabled vs diagnostic polling disabled | Ordinary axis-off can still poll in tilt-chord mode. |
| USB | Same power conditions, USB MIDI active vs inactive | Changing the power source at the same time confounds the result. |
| Persistence | Normal save vs diagnostic save suppression | Do not mistake an absent save for successful persistence. |
| Clock | 150/192/240/288 MHz, fixed frame rate and buffer size | Measure actual clock/rate. Lower clock can improve electrical margin but worsen CPU underruns. |
| Buffers | 128 vs 256 frames, fixed clock/rate | Record extra latency; larger buffers do not repair insufficient sustained throughput. |
| Power | Battery vs known-good USB supply | Keep volume/load equal; record voltage if available. |

The audit specifies diagnostic switches/counters in `REPAIR_PLAN.md`; they have not been added to the baseline firmware. Missing counters must be marked unavailable, never zero.

## Thirty-minute acceptance run for each repaired unit

Run one continuous 30-minute session, cycling through these five-minute segments: single notes and silence; four-note/effects load; looper recording/wrap/pause; arpeggiator range/mode changes; live tilt plus menu/preset interaction; repeated preset saves and playback stop/restart. Repeat the sequence as necessary to exercise every custom instrument and all user slots. Use a separate extended run across 71.6 minutes or a test-clock build for timer rollover; the host probe already exercises rollover deterministically.

Record once per second from an instrumented build: frame/DMA counts, underrun and late-publication counts, event overflow/high-water, maximum render time, maximum DMA handoff gap, and actual clock/rate. Maintain an audio recording and timestamp every anomaly. Firmware console output must not run inside audio/IRQ code.

Pass requires: no unexplained audible crackle, no stuck/missing notes, no underruns/late publications/event loss, no restart/save crash, correct recalled settings, and measured processing headroom. A run with a quiet or disabled workload is not evidence that the four-voice case passes. Record every tested unit separately; one clean device does not clear all affected units.

## Result entry — duplicate per run

- Unit/build/hash:
- Condition and elapsed duration:
- Audio/MIDI/logic-trace filenames:
- Observed crackle timestamps and trigger:
- Counter start/end/deltas (or unavailable):
- Actual measured clocks (or calculated only):
- Preset/settings recall outcome:
- Pass/fail/pending and reason:

Return the first reproducible condition and its recording/trace with the unit and UF2 hash. This distinguishes an audio deadline failure from a settings-save interruption, event-routing bug, or an electrical cause.
