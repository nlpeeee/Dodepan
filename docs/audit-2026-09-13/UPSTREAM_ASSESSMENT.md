# PRA32-U2 upgrade assessment

## Recommendation

**Defer migration while repairing the current engine integration.** The customized v1.6.0 build has independently confirmed transport, event-routing, and persistence defects; replacing DSP will not resolve those application defects. No selective DSP backport is recommended until a specific upstream fix is tied to a reproduced failure. Performance and sound changes need a controlled comparison.

The plan named v2.18.2, which remains this assessment's fixed candidate. Direct Git inspection during the audit also found v2.19.0, v2.20.0, and v2.20.1 tags. The earlier search result calling v2.18.2 current was stale; this report does not claim it is the latest release.

## Exact sources

| Source | Immutable revision | Purpose |
| --- | --- | --- |
| [Upstream v1.6.0](https://github.com/risgk/digital-synth-pra32-u2/tree/60040e7a41f391bf60805afaecfa503a45954414) | `60040e7a41f391bf60805afaecfa503a45954414` | Baseline for identifying local customization |
| [Upstream v2.18.2](https://github.com/risgk/digital-synth-pra32-u2/tree/ad082d08d825e979023f1dc7cae9f6a0dab45af5) | `ad082d08d825e979023f1dc7cae9f6a0dab45af5` | Fixed migration candidate |
| Local vendored engine | Working-copy hashes in `baseline.json` | v1.6.0 plus single-core polyphony table patch |

Comparing every upstream v1.6.0 file to the local tree with CRLF normalized found no missing upstream files and only one changed file: `pra32-u2-synth.h`. The local patch changes the first two single-core voice-mode table entries from monophonic to polyphonic. [Exact patch](engine-local.patch).

## Interface and behavior changes

| Area | Local baseline | v2.18.2 implications |
| --- | --- | --- |
| Sample generation | `process(int16_t &right)` | `process(0, 0, right)` for no external audio input; additional optional wide output arguments and template options exist. |
| Note release | `note_off(note)` | `note_off(note, release_velocity)`; use 64 when the current input path has no release velocity, then verify envelope behavior. |
| Polyphony | Two local table-entry edits | Use `PRA32_U2_ENABLE_POLY_ON_1_CORE`; do not mechanically carry forward the old edit. Keep two-core DSP disabled while the application assigns core 0 to controls and core 1 to synthesis. |
| Synth type | Concrete `PRA32_U2_Synth` | Template with default configuration. The existing declaration works in the minimal C++17 probe, but any explicit type aliases/signatures introduced later must use the intended specialization. |
| Program numbering | Eight user/eight factory engine programs | Sixteen user/sixteen factory programs; preserve Dodepan's six custom/eight selected factory/four user UI rather than exposing new program IDs unintentionally. |
| Audio width | Dodepan emits 16-bit duplicated-left frames | Upstream sketch uses a 24-bit DAC path, but the engine still provides a 16-bit return/right pair. Keep Dodepan's format during the initial candidate port; changing the PIO frame width is a separate feature. |
| Hardware acceleration | Existing generic engine under Pico SDK | Upstream interpolator setup and use are gated on Arduino architecture macros. The current Pico SDK build does not define them. Do not define `ARDUINO_ARCH_RP2040` just to get acceleration: it also pulls in Arduino EEPROM/I2S integration. A native SDK adapter would need to initialize the interpolator on the rendering core and preserve its register ownership. |
| Controller meanings and defaults | Existing 45-value persisted subset of a 48-entry table | Changes include chorus level behavior, envelope/velocity response, filter/modulation behavior, tuning and voice-assignment defaults. Preserving numeric CC values alone does not preserve sound. |

Supporting code: [candidate synth](https://github.com/risgk/digital-synth-pra32-u2/blob/ad082d08d825e979023f1dc7cae9f6a0dab45af5/Digital-Synth-PRA32-U2/pra32-u2-synth.h), [candidate constants](https://github.com/risgk/digital-synth-pra32-u2/blob/ad082d08d825e979023f1dc7cae9f6a0dab45af5/Digital-Synth-PRA32-U2/pra32-u2-constants.h), and [pinned change history](https://github.com/risgk/digital-synth-pra32-u2/blob/ad082d08d825e979023f1dc7cae9f6a0dab45af5/PRA32-U2-Change-History.md).

## Compile and memory evidence

The API probes use Arm GCC 14.2.Rel1 and real engine headers, not a fabricated API:

- Existing application call surface against the local engine: passes syntax compilation.
- Same calls against v2.18.2: fails on `note_off` and `process` argument changes.
- Calls adapted to `note_off(note, 64)` and `process(0, 0, right)`, with single-core polyphony enabled: passes syntax and object compilation.
- The standalone ARM object reserves **139,400 bytes** for the local synth and **142,700 bytes** for the candidate, an increase of **3,300 bytes**. This excludes the rest of firmware, RAM-resident tables/code, stacks, heap, and runtime execution cost. It is not a full candidate memory or timing budget.

[Commands and results](api-result.json). Minimal compilation establishes an integration route, not a working candidate firmware, equivalent sound, four-voice runtime timing, or a crackling fix.

## Sound-preservation gate

First retain acoustic/PCM baselines for all six custom instruments (Dodepan, Magic Bell, Space Piano, Robot Voice, Synthwave, Bleep Bloop) and four distinct user slots. Record their effective complete controller values, not only the 45 serialized values: the omitted delay fields can inherit prior state. Record instrument-switch order as part of the baseline.

Separate two comparisons:

1. **Transport correction:** original 24 kHz-predicted output versus the same engine at verified 48 kHz. This necessarily changes pitch/time relative to the defective baseline and must be documented independently of an engine update.
2. **Engine compatibility:** repaired v1.6.0 versus adapted v2.18.2 at identical verified 48 kHz output, controller state, note/velocity sequence, and output gain.

For each instrument, compare single notes at low/mid/high pitch, two/four-note chords, attack/release tails, repeated notes, maximum resonance, delay/chorus, and volume changes. Use identical deterministic input events; reset or account for noise/dither state. Compare pitch, envelope timing, peak/RMS level, and listening impressions. Do not normalize away clipping or level differences in the primary comparison.

New parameters need explicit compatibility values, and factory sounds should be represented by pinned controller tables rather than an assumed equivalent upstream program number. If a preset cannot be acceptably matched, keep the current engine available and present the specific audible difference for review. Saved-data conversion must use the versioned storage layer from repair batch 2, not reinterpret an old binary layout.

No recordings or physical listening comparison were available during this audit; all sound-preservation and runtime-performance gates are open.
