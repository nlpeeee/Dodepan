# Reproduce the audit

## Files and scope

Start with [AUDIT.md](AUDIT.md). `baseline.json` records the original HEAD, staged/working summaries, untracked files, and SHA-256 of each pre-existing file. The audit added `docs/audit-2026-09-13/` and `tools/audit/`; production source was preserved. Git's large status list includes pre-existing work and EOL-only engine changes.

The dated JSON/log/patch files are archived observations. Rerunning the tools writes to ignored `build-audit/results-latest/` by default; it does not replace dated evidence. `DODEPAN_AUDIT_RESULTS` can explicitly select a different result directory. Run from the repository root, `E:\PICO\dodepan\Dodepan`, or invoke scripts by absolute path.

## Dependencies and build

Windows requirements: Python 3.9+, Git, and Visual Studio C++ Build Tools (MSVC x64 plus bundled CMake/Ninja). No global SDK installation is made. The preparation script downloads official sources/tools into `build-audit/` and verifies pinned Git revisions and the captured compiler archive SHA-256. It leaves a mismatched existing checkout unchanged.

```powershell
python tools/audit/prepare_tools.py
python tools/audit/build_baseline.py
```

Pins:

- Pico SDK 2.2.0: `a1438dff1d38bd9c65dbd693f0e5db4b9ae91779`; TinyUSB submodule: `86ad6e56c1700e85f1c5678607a762cfe3aa2f47`.
- Arm GNU 14.2.Rel1 Windows x64 archive: `f074615953f76036e9a51b87f6577fdb4ed8e77d3322a6f68214e92e7859888f`. This is the captured content hash of the official download, not a claim of a separately verified vendor signature.
- Upstream engine pins are recorded in `UPSTREAM_ASSESSMENT.md`.
- The audit host used MSVC 14.50.35717, CMake 4.2 from Visual Studio, and its bundled Ninja. The SDK fetches/builds its matching picotool 2.2.0 during the first firmware build.

Official tool source: [Raspberry Pi's supported-toolchain manifest](https://github.com/raspberrypi/pico-vscode/blob/main/data/0.18.0/supportedToolchains.ini), which identifies the Arm-hosted archive used by `prepare_tools.py`. SDK source: [Pico SDK 2.2.0](https://github.com/raspberrypi/pico-sdk/tree/a1438dff1d38bd9c65dbd693f0e5db4b9ae91779).

The runner configures `PICO_BOARD=pico2`, Release, C11/C++17 via the existing project, and builds all configured targets, including the vendored MPU6050 example. The new baseline outputs are in `build-audit/firmware/`. It does not flash a device. Build exit 0 verifies compilation/linking/output generation, not hardware audio quality. Repeating on another machine may change embedded paths/build metadata and therefore hashes; always record the actual output hash rather than assuming byte-identical reproducibility.

## Defect probes

Run each command separately and inspect its exit status:

```powershell
python tools/audit/run_host_probes.py
python tools/audit/run_main_probes.py
python tools/audit/check_structure.py
python tools/audit/check_engine_api.py
```

| Tool | Archived baseline result | Meaning |
| --- | --- | --- |
| Playback probes | 16 checks, 5 pass / 11 fail; exit 1 | Real looper/arp/demo C code, simulated clock and output hooks. |
| Main probes | 11 checks, 0 pass / 11 fail; exit 1 | Exact selected `main.cpp` bodies and relevant `state.c` behavior, with hardware/synth event sinks. |
| Structural checks | 2 checks, 0 pass / 2 fail; exit 1 | Source-derived 64-cycle I2S timing and 48-versus-45 parameter table count. |
| Engine API checks | Expected compile outcomes: pass / fail / pass; exit 0 | Existing API succeeds locally, intentionally fails against new engine, and adapted calls succeed. |

The three failing suites deliberately assert desired behavior; do not treat their nonzero exits as a tool-install problem or relabel their failures as passes. They should turn green as the corresponding repairs land. Engine API's middle compilation is an explicit negative compatibility control; the runner checks that the rejection occurs.

Host limits: no RP2350 interrupt scheduling, DMA, I2C, analogue audio, or electrical behavior is emulated. Main probes generate a test translation unit under `build-audit/host/`; they omit unused context/selection functions whose enum return types differ under host C++ versus ARM's short-enum ABI. Functions under test are extracted unchanged. The preset out-of-range probe supplies two sentinel slots beyond four real slots to detect the erroneous index safely, rather than crash the host. Synth/MIDI sinks verify event lifetimes, not the DSP engine's voice allocation or audible output.

The structural simulator intentionally supports the current PIO instruction subset and fails on an unsupported instruction; update it if the PIO program changes. Its result assumes the configured system clock is attained and the measured clock equals nominal; confirm actual device clocks separately.

The API tool syntax-compiles and object-compiles a minimal call surface. Its `synth_object_bytes` is one static object symbol, not the entire candidate's SRAM footprint. Candidate firmware and hardware tests remain separate gates.

## Evidence index

- `baseline.json`: source inventory and original artifact hashes.
- `build-result.json`, `configure.log`, `build.log`, `sections.log`: fresh firmware build and memory evidence.
- `host-result.json`, `host-probes.log`, `host-compile.log`: playback regression results.
- `main-probes.log`, `main-compile.log`: routing, preset, and flash validation probes.
- `structural-result.json`: timing/parameter findings and EOL-normalized comparison.
- `engine-local.patch`: exact v1.6.0 customization.
- `api-result.json`, `*-api.log`: candidate API results and static synth sizes.
- `verification.json`: final pre-existing-file preservation and deliverable checks.

Full source patches and downloaded dependencies are local ignored artifacts, not duplicated into the report. Preserve the working tree plus its untracked demo/music assets when handing the audit to another machine; the original HEAD by itself is not the audited baseline.
