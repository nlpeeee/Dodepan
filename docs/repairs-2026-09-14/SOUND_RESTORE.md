# Restore legacy sound behavior

The user reported distorted, out-of-key presets with excessive reverb after installing the linked rc1 candidate. This correction restores the original effective 24 kHz output and original 45-control preset application. It does not retune the synth or edit patch values.

- Version: `2.6.0-rc2-legacy-sound`.
- Candidate: `build-audit/legacy-sound/Dodepan_RP2350.uf2`.
- Output: 24,000 Hz LRCLK, 768,000 Hz BCLK calculated from the PIO program; 128 frames take 5,333 microseconds. Timing diagnostics use that actual configured rate.
- Custom and user preset selection applies the first 45 controls, retaining the engine's current delay time, feedback, and mode, as the original loader did. Factory program selection is unchanged. All 48 values continue to be stored and captured; the last three are deliberately not applied on preset selection in this compatibility build.
- Saved-data layout and migration code are unchanged. No device settings have been erased or rewritten by this task.
- The current buffering, queue, and other repairs remain in this candidate. It is not a complete rollback to the original firmware. Both the original firmware and rc1 remain preserved in their existing directories.

The focused control/queue/touch checks and structural timing check pass. Build evidence is under `build-audit/legacy-sound-results/`. Physical sound equivalence and the reported distortion require listening on the affected unit; this correction is not a claim that the DMA transport has been hardware validated.

Rebuild with the audited toolchain:

```powershell
$env:DODEPAN_BUILD_DIR = "$PWD/build-audit/legacy-sound"
$env:DODEPAN_AUDIT_RESULTS = "$PWD/build-audit/legacy-sound-results"
$env:DODEPAN_CMAKE_ARGS = '["-DDODEPAN_UART_DIAGNOSTICS=OFF"]'
python tools/audit/build_baseline.py
```

Build succeeded. UF2 SHA-256: `48860f3d51207745575782b4c57e5ba1bb1548441738dde147c5df6205c5ef31`. Compiler warning/error lines: 0.
