# 48 kHz transport listening test
Firmware: `build-audit/transport48-render24/Dodepan_T48_Render24_288MHz_NoSaves.uf2`
Version: `2.6.0-test-t48-r24`
SHA256: `457b2ef228cb22c7277f0a91e042a56ef8e8ba71b9663c5fb5d9b147b4f96ccf`

The synth still generates 24000 samples per second, including unchanged master-volume ramps. Every generated sample is copied twice into both stereo channels. Each buffer contains 128 synth samples / 256 transport frames, lasting 5.333 ms. Command processing still occurs once per buffer. I2S LRCLK is configured for 48 kHz, BCLK for 1.536 MHz; the 288 MHz PIO divider is exactly 93.75. OLED should show 48k. Average/peak CPU percentages retain the same 5.333 ms budget.

Clock stays 288 MHz, saves are disabled, PCM capture/USB CDC are disabled, display and IMU remain enabled. Engine and preset hashes match the pre-test baseline. No interpolation, filters, preset rewriting or engine upgrade. Sample repetition preserves pitch/envelope timing but analog reconstruction and perceived tone may differ.

Compare Show Yourself, Let It Go, chords and previously affected presets at matching volume and power. Report pops, tonal changes and A/P/U/L/I. Require no stuck notes, U/L/I zero and peak below 80%; follow a successful comparison with 30 minutes of both songs and chord playing. Device results are pending; compilation is not evidence that pops are fixed.

Reference: `build-audit/cpu24-sram/Dodepan_CPU24_SRAM_288MHz_NoSaves.uf2` remains unchanged. The prior capture firmware also remains available. Flash the reference to roll back. Do not use the old regressed diagnostic build for comparison.

Verification: focused tests exercise the actual rendering loop and CPU formulas in default and repeated-sample modes: exact signed PCM, stereo ordering, output bounds, 128 synth calls per buffer, volume ramps across buffers, divider/deadline calculations and A/P values. Release build and automatic SRAM renderer check passed. Static SRAM 369,892 bytes; minimum heap/stacks 10,240 bytes. Full logs, hashes and the isolated change diff are under `build-audit/transport48-results`.

Build option `DODEPAN_TRANSPORT_48K` defaults OFF. ON requires 128 synth frames, 288 MHz, persistence disabled and capture OFF. Existing default builds retain 24 kHz transport. Test command: `python tools/pop-audit/test_transport.py`.

Overclock caveat: Pico 2's official board specification lists 150 MHz. 288 MHz is not guaranteed for every unit; zero deadline counters do not prove electrical stability. Keep it fixed for this transport comparison, then consider a separate rated-speed test with measured headroom. No clock/voltage adjustment was included here.
