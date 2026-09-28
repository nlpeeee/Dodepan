# CPU overload investigation

The reported diagnostic UF2 hash matches the archived rc1 no-saves variant: 48 kHz, 288 MHz, 128 frames, display/IMU enabled, saves disabled. Its OLED CPU value was lifetime average render demand; max was lifetime peak. Neither is whole-device CPU utilization. The reported 173% corresponds to approximately 4.61 ms rendering per 2.67 ms buffer, but which display field was 173 remains unconfirmed.

## New diagnostic

File: `build-audit/cpu24-diagnostic/Dodepan_CPU24_288MHz_NoSaves.uf2`.

Version: `2.6.0-rc3-cpu24`. Output: 24 kHz; clock: 288 MHz; buffer: 128 frames (5.333 ms). Display and IMU enabled. Saves disabled. UART1 diagnostics enabled on GPIO8/9 at 115200 baud. Original preset arrays, parameter mapping, and engine files were verified unchanged. Legacy 45-control preset application retained. No preset rewriting performed. The previous diagnostic binary remains untouched.

On the demo screen the bottom row alternates every two seconds:

- `24k A:... P:...`: A is recent render-demand percentage, sampled approximately every second; P is maximum per-buffer render percentage since boot.
- `U:... L:... I:...`: cumulative observed silence-buffer underruns, late/invalid publications, and excessive DMA IRQ gaps. OLED values cap at 999; UART retains full counts.

Reboot and play the same MIDI track with the same instrument/volume. Record A/P and whether U/L/I increase during distortion. Sustained A above 100 cannot keep up. A peak above 100 may be absorbed by buffering; delivery errors and listening determine its impact. Counter values can undercount when IRQ notifications coalesce. Zero counters alone do not establish correct electrical output.

The old 48 kHz build also differs in delay application, so this is not a pure rate-only A/B comparison. This candidate tests whether the current restored 24 kHz implementation still overruns. Keep the confirmed working original firmware as the sound reference.

## Verification

Release build passed without compiler warning/error lines. Seven focused tests of the actual CPU/error readout functions passed, including 4.613 ms producing 86% at 24 kHz, recent-window averaging, peak calculation, and safe error access before audio initialization. Build and test logs: `build-audit/cpu24-results/`.

UF2 SHA-256: `5e51f87115ab8bd32d49b51435c30b4e33b9994b257151051d8dabbe8f5942de`.

## Confirmed overload and SRAM correction

The user measured A=115%, P=273%, U=999 and L=999 in rc3 at 24 kHz. This establishes rendering deadline failures, with displayed error counts saturated. It does not establish which individual delay or pitch artifact follows from which dropped buffer.

Binary inspection found a concrete placement regression: original i2s_audio_task was in SRAM, while the replacement synth_audio_main was in XIP flash. PRA32 processing is always-inline, so DSP instructions inherit the owner's placement. The missing __not_in_flash_func attribute has now been restored on synth_audio_main.

Renderer addresses:

- original: `i2s_audio_task()` at `0x20000110`, 28736 bytes.
- regressed: `synth_audio_main()` at `0x100042d4`, 58964 bytes.
- corrected: `synth_audio_main()` at `0x20003528`, 58964 bytes.

Candidate: `build-audit/cpu24-sram/Dodepan_CPU24_SRAM_288MHz_NoSaves.uf2`; version `2.6.0-rc4-sram24`. Same 24 kHz, 288 MHz, 128 frames, enabled IMU/display and disabled saves as rc3. Only functional firmware change from rc3 is renderer SRAM placement; version/source identity also changes. Preset values and loading are unchanged.

Release build passed with no compiler warnings/errors. Linked renderer SRAM address was asserted programmatically; control, queue and touch regressions passed. Static SRAM excluding minimum reserved heap/stacks: 369684 bytes; minimum heap/stacks reservation: 10240 bytes. Runtime stack margin and device performance remain unmeasured.

UF2 SHA-256: `d12e1e63bd50dac60e2ba17745ab1b0329814f7c89dae16f519961569087fb73`.

Repeat the same MIDI track after reboot and report A/P, U/L/I, and sound. A reduction in load without growing delivery-error counters would support the placement diagnosis. Test the previously affected presets separately before declaring sound compatibility restored. Both prior binaries remain preserved.
