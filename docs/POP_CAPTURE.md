# Device pop capture

Diagnostic firmware: `build-audit/pop-capture/Dodepan_PopCapture_24k_SRAM_NoSaves.uf2`
SHA256: `c531dd17d3d8466ddf7388b599b3c3d54c10c09e17f10b20f522fbdab78ec8b9`
Reference remains `build-audit/cpu24-sram/Dodepan_CPU24_SRAM_288MHz_NoSaves.uf2`.

## Run

1. Flash the diagnostic firmware. It adds a USB serial interface alongside MIDI (diagnostic PID 4009). Use the same preset, volume, power and playback conditions as the reference. USB connection is now a test condition; record it.
2. In PowerShell run:
   `powershell -ExecutionPolicy Bypass -File E:\PICO\dodepan\Dodepan\tools\pop-audit\capture.ps1`
   If automatic discovery fails, add `-Port COM5`, substituting the device port shown in Device Manager. Python is used only after transfer to verify and convert the file.
3. Start a microphone recording. Play Show Yourself, or the previously failing chord passage. Keep the receiver window focused. Press Enter immediately after hearing a pop. The capture retains the preceding 1.365 seconds, ending at the next rendered block. If your reaction was slower, repeat after reboot.
4. Audio fades out after freezing; do not mistake this deliberate stop for the original pop. Export begins only after audio suspension. The device stays paused until reboot. The receiver saves a `.capture`, unnormalised `.wav`, and `.json` under `build-audit/pop-audit`.
5. Keep the microphone recording, preset name, volume, song position, power source and unit identity with the capture. Repeat the same passage from Let It Go as a control, and capture another run of the failing passage at the same settings. Reboot between runs. Re-running the receiver without reboot resends the same frozen capture.

## What this measures

The ring copies the left channel after master-volume scaling, immediately before audio buffer publication. Left and right are identical at this point. It holds 32768 signed 16-bit samples (64 KiB). No sample calculations, preset data, engine files, output rate, clock or buffer length were changed. Copying and the atomic handoff are included in measured render time; bulk transfer and checksum computation occur after suspension on core 0. MIDI playback sources and outgoing notes are stopped at capture freeze.

This records generated PCM, not a measurement of I2S pins or DMA delivery. Its sample indices count rendered frames since boot; queued buffers introduce a small speaker delay. Align the waveform to the microphone recording by the musical passage, not computer wall-clock time. Counters in the JSON are sampled when the frozen snapshot is observed, before requesting suspension. They cover the whole run, not just the capture window. A host WAV without pops does not exclude ARM-specific arithmetic or scheduling faults.

If the captured digital window contains the familiar pop, inspect that window before changing DSP. If it is clean while the matching microphone window pops, compare existing power/cables, fixed low/high volumes and another unit one variable at a time. This can localize the issue to delivery/amplification/power, but cannot identify a specific electrical component without measurements. No corrective sound change is included in this build.

## Verification and limits

- Pico 2 Release build passes; automatic post-build check requires the renderer and capture audio hook to reside in SRAM. This check also runs for ordinary builds.
- Static SRAM: 436,812 bytes; reserved minimum heap/stacks: 10,240; remaining address-space margin: 85,428 bytes before further runtime allocations. Runtime headroom still requires device testing.
- Focused tests: ring wrap/chronology, signed PCM, CRC, malformed/truncated transfer rejection, freeze handoff, no export before suspension, stalled/partial writes and exact resend. Receiver PowerShell syntax parsed successfully. Vendored engine and preset hashes match the pre-capture snapshot.
- USB enumeration, actual transfer, audible character and render demand have not been tested on hardware. Diagnostic USB activity can affect workload. Compare U/L/I and require peak rendering below 80%; the old A18/P33 measurement does not certify this build.
- The host song tests and on-device demo are repeatable musical comparisons, but not sample-identical schedules. No new deterministic on-device event sequencer or event trace was added in this capture stage.
- Acceptance remains matched pop reduction, preserved sound, no stuck notes or growing U/L/I, and a final 30-minute run. Hardware evidence is needed before selecting a correction.

Rebuild this target with `DODEPAN_POP_CAPTURE=ON` and `DODEPAN_DISABLE_PERSISTENCE=ON`; all other reference settings remain 24 kHz, 288 MHz, 128 frames. With capture OFF, no ring or CDC interface is compiled. Full build logs and firmware hashes: `build-audit/pop-capture-results`. Focused tests: `python tools/pop-audit/test_capture.py`.
