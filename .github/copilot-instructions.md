# Dodepan coding instructions

Read `AGENTS.md` first. `kidsmode` is the integration branch; work in a task branch and keep changes focused.

This firmware targets Pico 2/RP2350 with 4 MiB flash. `CMakeLists.txt` rejects RP2040. Core 0 in `main.cpp` handles controls and scheduling; Core 1 runs `synth_audio_main()` in `synth_service.cpp` for synthesis and I2S. Cross core synth commands use the queue in `synth_service.cpp`.

State accessors in `state.c` enforce UI bounds. Display and IMU share I2C1 through `i2c_mutex`; touch uses I2C0. Interrupt handlers defer work to the main loop. Keep audio rendering bounded and preserve SRAM placement of time critical functions.

Settings use the versioned dual sector format in `settings_store.c`, with flash reserved by `storage_reservation.ld`; preserve migration and recovery semantics. The default PRA32-U2 engine under `lib/digital-synth-pra32-u2` contains a local polyphony fix. `DODEPAN_ENGINE_218` selects the separate candidate under `lib/pra32-u2-v2.18.2`. Dependencies in `.gitmodules` are submodules.

Build using the Pico SDK with `cmake -B build -G Ninja -DPICO_BOARD=pico2` and `cmake --build build`. Use the relevant `tools/` checks for the touched behavior. Escalate audio, flash, and multicore changes and unresolved failures for GPT Sol review.
