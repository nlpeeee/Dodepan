# Dodepan firmware audit — 13 September 2026

## Outcome

The working firmware builds successfully, but has confirmed audio timing, preset, and playback defects. The available evidence does **not** establish the cause of crackling on particular hardware units. Keep the customized v1.6.0 engine for initial repairs; defer the engine migration until timing and preset compatibility are under test.

This delivery implements the audit and staged repair plan: evidence, reproducible build tooling, executable defect probes, repair specifications, and hardware test instructions. Production firmware files have not been edited. Diagnostic counters and repairs specified below are subsequent implementation batches; the rebuilt UF2 is a **baseline**, not a crackling fix.

## Baseline and validation

- Source HEAD: `22da4271408380be935113080bb9d49eec3a27cc`, plus the complete pre-existing staged, working, and untracked changes recorded in [baseline.json](baseline.json). SHA-256 hashes cover 180 pre-existing files. Full staged/working binary patches are retained locally under `build-audit/baseline/`; untracked file contents remain in place and their hashes are recorded.
- Ignore-EOL comparison reduces the apparent engine modifications to one functional file: the two polyphony table entries in `pra32-u2-synth.h`. See [engine-local.patch](engine-local.patch).
- Fresh, separate Pico 2 Release build: **passed**, Pico SDK 2.2.0 (`a1438dff1d38bd9c65dbd693f0e5db4b9ae91779`), TinyUSB `86ad6e56c1700e85f1c5678607a762cfe3aa2f47`, Arm GCC 14.2.Rel1 / 14.2.1. The original cached SDK/compiler paths were missing; official SDK and portable compiler were restored only inside ignored `build-audit/`.
- Firmware identity: `Dodepan`, `PROGRAM_VERSION=2.5.1`, customized PRA32-U2 v1.6.0, 288 MHz configured clock, 128 stereo frames per buffer, 16-bit duplicated-left output. Runtime clock setting success is currently unchecked.
- Raw BIN: **1,062,324 bytes**. Static SRAM sections: **316,192 bytes**, plus **6,144 bytes** minimum heap/stack reservations. Runtime heap demand and stack high-water marks have not been measured. The ordinary `size` summary classifies RAM-resident code/tables under text, so its BSS total alone is not a RAM-use estimate. See [sections.log](sections.log).
- Rebuilt UF2 SHA-256: `7b9b4c066fe2a7021e184df883eee5ad8f4a8411af929888c9845e28d2aafc23`; it differs from the pre-existing build UF2. Existing build/dist artifacts therefore must not identify the audited source by filename alone.
- Default firmware compilation completed without firmware compiler warnings. Host-side picotool compilation emitted conversion/format/macro warnings; CMake reported tool-fetch/developer warnings. Complete [configure log](configure.log), [build log](build.log), and [build result](build-result.json) are retained. This is not a claim that additional warning flags or static analyzers would be clean.
- **29 defect-oriented checks: 5 passed, 24 failed.** Playback modules: 16 checks/11 failures; main-function routing/state probes: 11 checks/11 failures; structural checks: 2 checks/2 failures. Failures assert desired behavior and deliberately return nonzero; they are not waived or disguised as passing tests. See [playback results](host-probes.log), [main results](main-probes.log), and [structural results](structural-result.json).
- Engine API probes: local calls compile; unchanged calls against v2.18.2 fail; minimally adapted calls compile. No complete v2.18.2 firmware was built or flashed.

## Findings

P1 = address before reliability release; P2 = next reliability batch. “Source” establishes the code defect, not that it caused the reported hardware crackling. “Probe” adds a repeatable host reproduction with mocked clock/hardware or event sinks.

### A01 — P1: I²S frame rate is half the configured rate

Evidence: [lib/sound_i2s/sound_i2s_16bits.pio:41](E:/PICO/dodepan/Dodepan/lib/sound_i2s/sound_i2s_16bits.pio:41). Source + PIO instruction simulation; high confidence.

The program executes 64 instructions for 32 serial bits, but the divider denominator is sample_rate × 32. At the configured clock this gives approximately 24 kHz LRCLK and 768 kHz BCLK, rather than 48 kHz/1.536 MHz. A synth designed for 48 kHz runs its generated signal at half speed/pitch on this path. The 128-frame deadline is currently about 5.333 ms, while the profiler assumes 2.667 ms.

Reproduction and correction: Run check_structure.py; verify BCLK/LRCLK on a device. Correct the divider only alongside a fresh real-time budget measurement: doubling consumption may expose render overruns. This universal code defect alone does not explain why only some units crackle.

### A02 — P1: Core 0 mutates the synth while core 1 renders

Evidence: [main.cpp:391](E:/PICO/dodepan/Dodepan/main.cpp:391). Source; high confidence in missing synchronization; audible manifestation pending.

Touch, tilt, presets, demos, and looper handlers call g_synth directly on core 0. Core 1 concurrently calls process() over mutable voice, envelope, oscillator, and FX state. No queue, lock, or ownership handoff protects those accesses. The 100-us sleeps between chord notes do not provide synchronization.

Reproduction and correction: Stress notes, preset changes, and tilt while rendering. Move synth mutation to the render owner using bounded ordered commands; publish a coherent controller/state mirror back to the UI. See repair batch 2.

### A03 — P1: Delayed settings saves interrupt the audio transport

Evidence: [main.cpp:256](E:/PICO/dodepan/Dodepan/main.cpp:256). Source + SDK interrupt semantics; high confidence, audible severity pending.

The alarm callback resets core 1, disables interrupts on core 0, erases/programs flash, then relaunches core 1. DMA remains active, but its IRQ handler on core 0 is needed to rearm each buffer. During erase, transfer/FIFO exhaustion can stall PIO; restart also resumes synth state interrupted mid-update. The persistent last_buffer pointer is not explicitly reset.

Reproduction and correction: Change a persisted setting while holding a sustained sound and observe the save around 10 seconds later. Use a main-loop save state machine with an explicit audio quiesce/mute/stop and clean restart protocol; remove flash work from the alarm callback. Preserve the legacy data layout until migration is tested.

### A04 — P1: The buffer handoff cannot detect stale or partially rendered audio

Evidence: [lib/sound_i2s/sound_i2s.c:24](E:/PICO/dodepan/Dodepan/lib/sound_i2s/sound_i2s.c:24). Source; high confidence in missing deadline/ownership enforcement.

DMA alternates two buffers unconditionally. The producer only compares buffer addresses, without a ready state or generation counter. If rendering is late, DMA can consume the buffer being written. If an even number of handoffs is missed, the address comparison can conceal the missed work. DMA IRQ priority is 0xff, and startup ignores allocation failure.

Reproduction and correction: Instrument generation and ownership transitions, delayed render, FIFO stalls, and DMA IRQ gaps. Use explicit buffer lifecycle states and a defined underrun fallback. The SDK confirms READ_ADDR_TRIG reloads the transfer count; a missing count reload is not itself a defect here.

### F01 — P1: User instrument numbering can select wrong or out-of-range presets

Evidence: [main.cpp:84](E:/PICO/dodepan/Dodepan/main.cpp:84). Source + 4 main probes; high confidence.

update_instrument assigns user instruments 14–17, but load_user_preset subtracts 12. Instruments 14/15 load slots 2/3; 16/17 index beyond the four allocated pointers. The menu stops at 15 (state.c:124), and saving selects 9 + slot (main.cpp:295), a factory instrument rather than the saved slot. Validated persisted instrument values can include 16/17.

Reproduction and correction: Save four distinct patches, select/reboot into them. Centralize the six-custom/eight-factory/four-user mapping, bound-check the loader, and test all 18 IDs. Out-of-range host probes use extra sentinel slots to observe the invalid index safely; firmware allocates only four.

### F02 — P1: Tilt changes leave synth/MIDI/looper note lifetimes unbalanced

Evidence: [main.cpp:555](E:/PICO/dodepan/Dodepan/main.cpp:555). Source + 4 main probes; high confidence.

Initial chords include the root, but rebuilt new_notes excludes it. Changed pitches call g_synth.note_on directly without note-off for the old pitch, MIDI routing, or looper recording. Overwriting active_chord_notes loses the original release information. all_notes_off clears counts but leaves pad_held set, so another tilt can start sound after panic.

Reproduction and correction: Hold a tilt chord, change zones, then release or invoke panic and tilt again. Build complete root-inclusive target sets and apply balanced set differences through one source-aware event path; clear held/root/velocity/voicing state on panic.

### F03 — P1: Held-pad releases follow the current mode instead of the press owner

Evidence: [main.cpp:639](E:/PICO/dodepan/Dodepan/main.cpp:639). Source + main/arp probes; high confidence.

A pad pressed as a normal/chord note is released through the arpeggiator if it was enabled while held. Disabling an active arp makes arpeggiator_task return before releasing its note; release then routes to note_off with different tracking. External MIDI panic is also absent from main all_notes_off.

Reproduction and correction: Hold a pad while switching arp off/on; check synth and external MIDI releases. Track the mode/source that owns each press and make transitions explicit. Panic must release all emitted MIDI notes and reset sustain as well as internal state.

### F04 — P1: Loop wrap and pause lose required note releases

Evidence: [looper.c:228](E:/PICO/dodepan/Dodepan/looper.c:228). Source + 3 playback probes; high confidence.

looper_task restarts before dispatching due events from the end of the previous cycle. Events exactly at the end, or between the previous poll and a late wrap, are skipped. looper_stop changes state without releasing sounding notes; clear also drops events without cleanup.

Reproduction and correction: Record on at 0/off at 100 ms, finish the loop at 100 ms, and advance directly to the boundary; repeat with a late poll. Pause during a sustained note. Drain due events through the boundary before wrapping, advance from the prior epoch, and release only looper-owned notes at transport changes.

### F05 — P2: Recording limits and automatic arming mishandle boundary states

Evidence: [looper.c:148](E:/PICO/dodepan/Dodepan/looper.c:148). Source + 3 playback probes; high confidence.

Capacity exhaustion silently discards all events including releases. The 20-second limit is checked only on append, so an idle or full recording can remain active indefinitely. looper_record_note auto-starts from IDLE/PAUSED on a note-off as well as a note-on; releasing after pause can erase the retained loop.

Reproduction and correction: Fill the buffer with a note and CC events, release, or wait past 20 seconds without input. Stop recording safely at capacity/time, preserve final releases, and arm only on a new positive-velocity note-on. Expose a full/limit state in existing screen feedback.

### F06 — P2: Arpeggiator octave reduction and clock wrap corrupt timing/pitch

Evidence: [arpeggiator.c:106](E:/PICO/dodepan/Dodepan/arpeggiator.c:106). Source + 2 playback probes; high confidence.

Reducing the octave range does not normalize current_octave; DOWN computes an unsigned negative offset, which wraps again when stored as an 8-bit note. Separately, time_us_32()/1000 wraps every ~71.6 minutes, not at the modulus used by uint32 millisecond subtraction, causing an early release/step.

Reproduction and correction: Start DOWN at three octaves, reduce to one before the next step; test across 0xffffffff microseconds. Use wide signed note arithmetic and normalize sequence state; subtract wrapping microseconds before converting, or use a 64-bit monotonic timebase.

### F07 — P2: Demo tempo changes are applied retroactively

Evidence: [demo_player.c:423](E:/PICO/dodepan/Dodepan/demo_player.c:423). Source + playback probe; high confidence.

Elapsed time since song start is converted using the latest tempo. A tempo change reinterprets all prior elapsed time, shifting later notes and releases. A 480-PPQN fixture with a change from 500,000 to 1,000,000 us/quarter at tick 480 should release at 1.5 s, but the baseline defers that tick-960 event until 2 s.

Reproduction and correction: Use the included tempo fixture. Schedule deltas against an accumulated microsecond deadline with fractional remainder; apply each tempo change only to following delta times.

### F08 — P2: The preset parameter count omits three delay settings

Evidence: [instrument_preset.h:5](E:/PICO/dodepan/Dodepan/instrument_preset.h:5). Source + structural check; high confidence.

The table contains 48 parameters but PROGRAM_PARAMS_NUM is 45, so DELAY_TIME, DELAY_FEEDBACK, and DELAY_MODE are not loaded, edited through the complete range, or saved by these loops. Patch sound can depend on the previously selected instrument. Legacy persistence uses 244 bytes (16 + 4×45 + 48); 48 parameters consume exactly 256 bytes and move the scale offsets.

Reproduction and correction: Save/recall patches with different delay settings. Derive counts from array sizes, add compile-time checks, and implement a versioned legacy reader before changing serialization. Use a size_t offset: the present uint8_t offset would wrap at 256.

### F09 — P2: Persisted payload and demo headers are insufficiently validated

Evidence: [main.cpp:169](E:/PICO/dodepan/Dodepan/main.cpp:169). Source + flash/header probes; high confidence.

Flash validation checks top-level settings but not preset controller bytes or the 48 custom-scale degrees. A 255 scale degree is accepted by the baseline probe. There is no schema version or checksum to detect interrupted writes. Demo headers accept zero PPQN; tempo payload reads lack a complete three-byte track-bound check, and VLQs are not capped at four bytes.

Reproduction and correction: Feed the included invalid scale and zero-division fixtures. Validate the entire payload before applying any state; preserve raw legacy data for migration. For embedded demos, reject malformed lengths, tempo, division, and running status. Malformed-MIDI reachability is primarily asset-generation quality, not an exposed network parser.

### A05 — P2: The CPU display cannot prove underrun-free playback

Evidence: [main.cpp:703](E:/PICO/dodepan/Dodepan/main.cpp:703). Source; high confidence.

Core 1 increments total/count/max while core 0 reads and resets the same volatile fields independently. This can lose samples or combine different windows. Metrics cover render time only, assume the incorrect sample period, and do not measure DMA deadlines, event loss, or FIFO starvation.

Reproduction and correction: Compare the display with sequence-based diagnostics under load. Publish coherent snapshots from the owning core; record monotonic counters and actual deadlines, with logging outside time-critical work.

### H01 — P2: Sensor failure and debounce behavior can disturb playability

Evidence: [imu.c:103](E:/PICO/dodepan/Dodepan/imu.c:103). Source; high confidence in error-handling defects, hardware symptoms pending.

The IMU ignores both I2C return values and decodes an uninitialized data array after failure. sqrt_fixed divides by y without a zero guard; zero or sufficiently small acceleration input can reach division by zero. Touch release debounce counts main-loop iterations rather than elapsed time, making release latency load-dependent; its startup time_us_32 check repeats on rollover.

Reproduction and correction: Exercise missing/disconnected IMU and synthetic zero input, then compare tap/release latency with and without display activity. Keep last valid sensor data or explicit neutral defaults after a failed transfer; replace the square-root calculation with bounded integer math and use elapsed-time debounce.

### U01 — P2: Encoder events can be overwritten or lost during consumption

Evidence: [main.cpp:976](E:/PICO/dodepan/Dodepan/main.cpp:976). Source; high confidence.

The interrupt callback stores only the latest direction. Multiple detents during display/I2C work collapse to one; an interrupt between the main-loop read and clearing encoder_direction is also lost. This is consistent with a control needing polish, but has not been measured on the physical encoder.

Reproduction and correction: Rotate rapidly while menus redraw. Use a bounded accumulated delta or queue with an atomic/IRQ-protected consume operation; keep interrupts short and process navigation in the main loop.

## Hardware-dependent hypotheses and limits

The 288 MHz overclock, regulator/flash timing variation, battery/USB supply conditions, amplifier gain/load, grounding, and physical I²S wiring remain hypotheses. No voltage, temperature, logic-analyzer traces, serial diagnostics, or audio recordings were collected from a device in this session. The requested 30-minute per-unit stress run and sound comparisons remain pending user hardware testing.

A ninth chord requests five notes from a four-voice engine; voice stealing is expected, not proof of crackling. Identical left/right output is an intentional integration choice; stereo panning added upstream should not silently change the device output. Existing engine note counters already handle some repeated pitches; the source-ownership repair is aimed at balanced transport/mode cleanup, not a claim that every overlapping chord currently releases early.

See [repair batches](REPAIR_PLAN.md), [hardware protocol](HARDWARE_TESTS.md), [engine assessment](UPSTREAM_ASSESSMENT.md), and [reproduction instructions](REPRODUCE.md).
