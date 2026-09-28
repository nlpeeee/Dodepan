# PRA32-U2 v2.18.2 evaluation candidate

Firmware: `build-audit/engine218-native48/Dodepan_PRA32U2_2.18.2_Native48_NoSaves.uf2`
SHA256: `110fc3686fa5f4f907d6e1da7701dde1b277bd375663b19692d255fa673f4ab5`
Version: `2.7.0-test-engine218`
Pinned upstream: https://github.com/risgk/digital-synth-pra32-u2/tree/ad082d08d825e979023f1dc7cae9f6a0dab45af5
This is the previously assessed v2.18.2, not a claim to be the newest upstream release.

## Integration

Native 48000 synth samples and I2S frames per second; no sample repetition. 256 frames retain the 5.333 ms buffer period. Four-voice polyphony uses the upstream PRA32_U2_ENABLE_POLY_ON_1_CORE option. Core 1 owns the synth; control handling stays on core 0. 16-bit duplicated-left speaker output and SRAM renderer placement remain. Clock remains 288 MHz for comparison; no voltage change or Arduino architecture emulation. Runtime CPU demand is not yet measured.

A separate vendored upstream tree is byte-identical to the pinned candidate. DODEPAN_ENGINE_218 selects it; OFF retains the legacy engine. Release evaluation requires saves disabled, PCM capture OFF and the repetition experiment OFF. Existing UF2s and source snapshots are retained. Static SRAM is 374,080 bytes plus 10,240 bytes minimum heap/stacks.

## Instruments and saved data

The six custom instruments and original eight factory parameter sets remain associated with the existing UI IDs. Original factory rows are frozen in legacy_factory_presets.h so upstream program renumbering cannot silently substitute other sounds. Existing custom/user parameter bytes are not rewritten. Actual device flash was not backed up by this task; the candidate disables writes so existing stored data remains intact. Edits made while running this candidate will not survive reboot.

The adapter resets the full new-engine program state, centres coarse/fine/stretch tuning, pan and new envelope sensitivity controls, disables oscillator drift, then applies all 48 Dodepan parameters. This intentionally removes old delay-state inheritance from applying only 45 parameters. Other new program controls use the pinned factory-program-16 defaults. Note-off supplies release velocity 64 because the existing control API has no release velocity. Master gain ramp increments are halved to preserve ramp duration at twice the render rate. Engine default program changes are not exposed as new menu instruments.

These are starting patches, not completed sound matches. Native rendering changes pitch/envelope/effect timing versus the old 24 kHz engine execution; upstream also changed envelope, velocity, chorus and other behaviour. Preserving numeric parameters does not preserve audible character. Revoicing the bank requires listening comparison, not an assumed numerical conversion. The four physical user slots retain their existing data format; new upstream parameters are not yet editable/persisted by Dodepan. Production save migration and broader UI work are deferred.

## Validation and next test

Release build passes with the automatic SRAM check. Focused actual-engine tests pass for all 14 built-in instruments, all 48 mapped parameter values, deterministic controller reset after switching, four allocated voices, fifth-note replacement and balanced note releases. Seven-second native WAVs per UI instrument ID 0..13 are under build-audit/engine218-tests. They contain low/mid/high notes and a four-note chord; these are computer renders, not hardware recordings. Observed peaks were below output full scale in these cases. The old/default and repeated-transport focused tests still pass. Test tools: tools/pop-audit/test_engine218.py and tools/pop-audit/test_transport.py. Logs and isolated integration diff: build-audit/engine218-results.

Flash the labeled candidate and confirm the version and 48k display. Compare Dodepan, Magic Bell, Space Piano and Elec. Piano first, then the rest. Report which retain their identity and which need slower envelopes, different pitch, less brightness or changed effects; also report A/P/U/L/I during chords and demo playback. Require peak demand below 80%, no growing delivery counters or stuck notes before treating this as a usable foundation. Follow with a 30-minute run once sounds are acceptable. No pop fix, rated-clock stability, hardware performance or sound equivalence is claimed by compilation.

Rollback: build-audit/transport48-render24/Dodepan_T48_Render24_288MHz_NoSaves.uf2 (SHA 457b2ef228cb22c7277f0a91e042a56ef8e8ba71b9663c5fb5d9b147b4f96ccf), or the original rc4 SRAM reference. Both remain unchanged. Pop investigation is parked at the user's request; this candidate targets the upgraded instrument foundation.
