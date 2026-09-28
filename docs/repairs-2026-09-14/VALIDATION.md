# Validation record

- 81 host regression checks passed; 2 structural checks passed (83 total).
- Pico 2 Release candidate and all eight diagnostic variants configured and linked successfully. No compiler warning/error lines were present in these final build logs.
- SDK 2.2.0: `a1438dff1d38bd9c65dbd693f0e5db4b9ae91779`. Compiler: `arm-none-eabi-gcc.exe (Arm GNU Toolchain 14.2.Rel1 (Build arm-14.52)) 14.2.1 20241119`. CMake/Ninja command lines and artifact metadata are in `build-result.json` and `diagnostic-manifest.json`.
- Release static SRAM excluding reserved heap/stacks: 311,264 bytes. Minimum reserved heap/stacks: 10,240 bytes. See `sections.log`; runtime stack high-water and heap use are not measured by these numbers.
- Release UF2: 2,184,704 bytes; SHA-256 `9bf4a316e81faac70fcbd0bc8bccb6211e3ab7193ff1cf1758417a2cadd129d6`.
- 58 vendored engine files are byte-identical to the audit baseline. Existing staged patch is unchanged. No baseline file is missing. Pre-repair sources and original UF2/ELF/BIN remain under `build-audit/repair-baseline/`.

## Diagnostic UF2 identity

| Variant | Bytes | SHA-256 |
| --- | ---: | --- |
| baseline | 2,187,776 | `8f162f90ddefc64f0190943c81bd4396dc51f93a0fc9d13a0a3cdd2217942dd0` |
| clock150 | 2,187,776 | `4b3fd9ce8f7573c8f39229384969c78cbf0713aa159d2b3446cdfb37fbc11062` |
| clock192 | 2,187,776 | `2693d45e8de383aaafa7bac425994a4cc7182538469d44b25d99913c4cf296af` |
| clock240 | 2,187,776 | `6c71b75d7d9b6d2f98041b5dd2e17a0a5def3b9655702105a145081c13de9b22` |
| buffer256 | 2,187,776 | `bb21df335826b5e89f8ff6e21e2e7fe4ce35577a66d443af957ef5bfdde0f56b` |
| no-display | 2,147,840 | `c8c76ed55c7415cadbb02cc82d63d1d0757f973a22b6926774c1f6587fcaa7ff` |
| no-imu | 2,178,560 | `a77935798a99973db0564820141a403e71900bbc0bdd5e3539ee9c645b002260` |
| no-saves | 2,183,680 | `7565cfe2bb3cbb5301cd596b777f309483d7f9eb2e24397ff3290d5c34200a8f` |

## Reproduce

Run from the repository with the audited tools available:

```powershell
python tools/repairs/run.py
python tools/repairs/run_controls.py
$env:DODEPAN_AUDIT_RESULTS = "$PWD/build-audit/repair-results"
python tools/audit/check_structure.py
$env:DODEPAN_BUILD_DIR = "$PWD/build-audit/repaired"
$env:DODEPAN_CMAKE_ARGS = '["-DDODEPAN_UART_DIAGNOSTICS=OFF"]'
python tools/audit/build_baseline.py
python tools/repairs/build_diagnostics.py
```

The explicit build directory preserves the original audit firmware. The matrix runner copies each variant UF2 before reusing its build directory. Its manifest records the source build path as well as hashes; the retained UF2 lives under the named diagnostic variant directory. `existing-source-changes.patch` compares normalized text against the pre-repair snapshot; newly added modules are present directly in the working tree.

Host probes use controlled clocks and sinks. The command-queue test substitutes the synth engine and locks, so it validates ordering, capacity, transactions, and panic behavior rather than multicore scheduling. The buffer model test does not execute DMA hardware. All physical acceptance criteria remain pending; see `DEVICE_TESTS.md`.
