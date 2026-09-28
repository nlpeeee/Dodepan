# Device validation handoff

These files target matching Pico 2 / RP2350 boards with 4 MiB flash and MAX98357 amplifiers. No firmware has been flashed by this task. Preserve a raw settings backup before first installation or electrical power-loss experiments, including legacy sector `0x1ff000` and journal sectors `0x3fe000` / `0x3ff000`.

## Candidates

`build-audit/repaired/Dodepan_RP2350.uf2` is the candidate with UART diagnostics disabled. `build-audit/diagnostics/<variant>/Dodepan_RP2350.uf2` contains these labeled diagnostic variants:

| Variant | Change from diagnostic baseline |
| --- | --- |
| baseline | 288 MHz, 128 frames, display/IMU/saves enabled |
| clock150 | 150 MHz only |
| clock192 | 192 MHz only |
| clock240 | 240 MHz only |
| buffer256 | 256 frames only |
| no-display | Display initialization and updates disabled |
| no-imu | IMU initialization and polling disabled |
| no-saves | Persistent writes disabled; edits remain pending |

All diagnostics use UART1 TX GPIO8, RX GPIO9, 115200 baud, 8N1, 3.3 V logic, common ground. Connect a receiver to GPIO8 to capture logs. UART0 stdio is disabled because its default pins overlap I2S GPIO0/1. USB MIDI product identity is unchanged. No logs are emitted from audio rendering or DMA interrupts. The core-0 task writes at most 16 currently writable UART bytes per pass.

Each line reports firmware/source/engine identity, disable flags in display/IMU/persistence order, elapsed seconds, system clock, buffer frames, calculated frame rate in milli-Hz, maximum render and IRQ-gap microseconds, underruns, late publications, missed IRQ deadlines, DMA notification count, observed frames, queue high-water/loss, USB MIDI loss, maximum main-loop gap, IMU availability, save pending, and suspended status. `observed_frames` is derived from serviced notifications and can undercount if interrupts coalesce; any missed-IRQ count invalidates a clean-run conclusion. Counters are cumulative since boot. Frame rate is calculated from the configured PIO divider, not measured on the pins.

## Repeatable sequence

1. Choose one affected unit and an unaffected unit if available. Record unit ID, UF2 SHA-256, amplifier/speaker, power source/cable, USB attachment, volume, instrument, and exact prior crackle reproduction in `device-results.csv`.
2. Retain the original 24 kHz baseline at `build-audit/repair-baseline/Dodepan_RP2350.uf2` (SHA-256 `7b9b4c066fe2a7021e184df883eee5ad8f4a8411af929888c9845e28d2aafc23`) for comparison. Reproduce and record the old symptom first.
3. Test diagnostic baseline. Confirm LRCLK 48 kHz and BCLK 1.536 MHz with a logic analyzer. Exercise all custom instruments, saved slots, four simultaneous voices, and effects. Measure touch-to-audio latency and worst render time. At 128 frames require worst render <= about 2,133 microseconds (80% of 2,667 microseconds).
4. Repeat the same scripted notes/demos and control movements, changing only one variant or external variable at a time: clock, buffer, display, IMU, saves, USB, power, voice count, or effects. Do not interpret a lower-clock build as an established fix.
5. Hold pads while changing key, scale, instrument, chord, and arp pattern; overlap demo/loop/live notes at the same pitch; stop and restart each source; fill a loop and reach its duration limit. Check both local sound and an external MIDI event capture for stuck or prematurely released notes.
6. Change settings while playing beyond the ten-second save delay. Confirm `*` stays present. Stop playback/release pads, let the tail decay, and record save plus restart. Confirm `*` clears only after success; reboot and compare all four presets/scales. With backups available, test interrupted writes separately and confirm either the previous or complete new record loads.
7. Run the unchanged stress sequence for at least 30 minutes on every tested unit, including repeated delayed saves. Require no unexplained crackling, stuck notes, underruns, late publications, IRQ deadline misses, command loss, or USB MIDI loss. Retain logs and speaker recordings, including counter values before and after saves.
8. Select the lowest measured stable clock with adequate headroom. Compare all six custom patches and four distinct saved presets against the baseline, recording the expected rate-related pitch/time change and any additional audible differences. Engine migration remains deferred.

A failed run is evidence to investigate, not a reason to silently raise buffers or clock. Record when a counter changed and the concurrent interaction. The chained DMA silence fallback and flash ramp/restart need oscilloscope/audio confirmation; host ownership tests do not establish electrical correctness.
