# Dodepan coding guide

The integration branch is `kidsmode`. Start each assignment from its current commit on a new `agent-zero/<task>` branch. Keep the archived Agent Zero snapshot read only; the Git checkout is the source for code changes.

Before editing, read the relevant files and the project architecture notes. Keep reads bounded to named files or symbols. Each assignment should have one defined behavior, a short file list, and a relevant test target. Do not refactor unrelated code. Commit the focused change and report the diff, checks, and remaining uncertainty.

Hardware changes need extra care. Escalate changes to audio rendering/I2S, flash storage, or multicore synchronization for GPT Sol review. Escalate unresolved test or build failures. A reviewed pull request requires user approval before merge.

## Current architecture

- Pico 2/RP2350 only. Build with the Pico SDK and `-DPICO_BOARD=pico2`; `CMakeLists.txt` rejects other boards.
- Core 0 in `main.cpp` handles touch, UI, IMU, looper, arpeggiator, demo, MIDI, and persistence. Core 1 enters `synth_audio_main()` in `synth_service.cpp` and owns synthesis and I2S output. The command queue crosses the cores.
- `state.c` and `state.h` own UI state; use accessors. The display and IMU share I2C1 through `i2c_mutex`; touch uses I2C0.
- `settings_store.c` owns the versioned, CRC checked dual sector settings format. `storage_reservation.ld` reserves the last two flash sectors. Preserve migration and power loss behavior.
- `synth_backend.h` selects the default PRA32-U2 engine or the `DODEPAN_ENGINE_218` candidate. The default engine has a local polyphony fix; verify it when changing that library.
- Libraries listed in `.gitmodules` are submodules. Other synth and audio code under `lib/` is vendored or local.

## Checks

Use focused checks from `tools/` for the touched behavior. `python tools/audit/check_structure.py` checks repository structure. On Windows, `cmake --build build` checks the existing Pico 2 configuration when its SDK/toolchain is available. Do not flash hardware as a side effect of a coding task.
