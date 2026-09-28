# Preset conversion report

Target: PRA32-U2 v2.18.2, native 48 kHz; reference: original customized v1.6.0 executed at 24 kHz. All render files are unnormalised. Original preset bytes remain available. Factory programs 16-31 are loaded directly, without legacy conversion.

## Method

- Tuning: target Coarse Tune=52 (-12 semitones), centred fine/stretch tuning and drift off restore the old execution-rate octave. MIDI notes are unchanged. Original saw shape is selected for converted patches.
- Envelopes/portamento: extract actual Q30 recurrence coefficients. Match time constants using the quarter-rate update frequency; at double rate the target coefficient is ideally the square root of the old coefficient. Search all 128 legal controls. This is coefficient matching, not a claim of identical envelope waveforms. Attack state transitions, velocity law and envelope routing changed upstream.
- LFO/chorus rates: match physical cycles/second from phase increments. Fade duration accounts for the old decrement-before-test and new test-before-decrement counters. Chorus depth matches fractional sample delay in seconds, capped to available controls.
- Filter: recover biquad centre frequency from actual denominator coefficients, then search all cutoff values. Lookup tables account for the -12 semitone tuning interacting with each target key-tracking value. This matches the base at input note 60, zero envelope/LFO and low resonance; nonlinear resonance and modulation sweeps are not guaranteed identical.
- Gain/chorus: evaluate amplifier and centre-pan scaling, including old output scaling. Convert the old crossfade wet/dry ratio to the new additive level and compensate dry gain using legal AMP_GAIN values. Saturation and changed nonlinear processing can still alter loudness.
- Release=decay: translate old release-table timing independently and disable the coupling flag, preserving separate target release values. Fixed legacy custom delay defaults are time=127, feedback=0, mode=1. Validated user records retain their supplied fields; missing legacy fields are filled by the existing loader.

## Remaining differences

The fixed chorus base delay has no public controller and halves in seconds at native 48 kHz; its filtering also changed. Long envelope, fade, portamento, chorus-depth and delay values can exceed the target's range. Converted controls saturate at the closest legal value and the errors below expose the difference. There is no interpolation/DSP rewrite to hide it. New velocity response, EG-to-amplitude semantics, oscillator waveforms/aliasing, filter saturation and output filtering remain algorithmic differences. No per-patch subjective tuning has been applied.

Pitch estimates below use autocorrelation near the expected legacy fundamental; low confidence or boundary results are not reliable pitch measurements. Raw comparison JSON retains confidence and measured full-scale sample counts. A changed spectrum can confuse a waveform pitch estimator.

## Dodepan

Peak 10786 -> 13538; RMS 1177.9 -> 1549.6; full-scale samples 0 -> 0.

Changes: EG_ATTACK 8->18, EG_DECAY 90->100, EG_RELEASE 0->100, LFO_RATE 24->12, AMP_GAIN 127->112, AMP_ATTACK 8->18, AMP_DECAY 98->108, AMP_RELEASE 0->108, REL_EQ_DECAY 64->0, CHORUS_MIX 116->105, CHORUS_RATE 20->8, CHORUS_DEPTH 90->126, DELAY_TIME 127->113.

| Physical measure | Reference | Converted | Error |
|---|---:|---:|---:|
| EG_ATTACK (attack) | 0.0032451 | 0.003332 | 2.7% |
| EG_DECAY (decay) | 0.37474 | 0.38477 | 2.7% |
| EG_RELEASE (release) | 0.37474 | 0.38477 | 2.7% |
| AMP_ATTACK (attack) | 0.0032451 | 0.003332 | 2.7% |
| AMP_DECAY (decay) | 0.6664 | 0.68423 | 2.7% |
| AMP_RELEASE (release) | 0.6664 | 0.68423 | 2.7% |
| LFO_RATE (lfo_rate) | 0.13518 | 0.13518 | 0.0% |
| LFO_FADE_TIME (lfo_fade) | 0 | 0 | n/a |
| CHORUS_RATE (chorus_rate) | 0.018954 | 0.018597 | -1.9% |
| CHORUS_DEPTH (chorus_depth) | 0.015 | 0.0105 | -30.0% |
| DELAY_TIME (delay_time) | 0.68 | 0.34 | -50.0% |
| PORTAMENTO (portamento) | 0 | 0 | n/a |
| FILTER_CUTOFF (Hz at input note60, no modulation) | 659.26 | 659.26 | -0.0% |

Times are seconds; envelope values are exponential time constants, not acoustic attack lengths. Rates are Hz.

low pitch estimate: unreliable for this modulated waveform; not reported.

mid pitch estimate: unreliable for this modulated waveform; not reported.

high pitch estimate: unreliable for this modulated waveform; not reported.

## Magic Bell

Peak 5839 -> 5752; RMS 651.5 -> 642.1; full-scale samples 0 -> 0.

Changes: EG_ATTACK 0->10, EG_DECAY 85->95, EG_RELEASE 64->95, LFO_RATE 60->48, LFO_FADE_TIME 64->83, AMP_GAIN 110->93, AMP_ATTACK 0->10, AMP_DECAY 90->100, AMP_RELEASE 70->100, REL_EQ_DECAY 64->0, CHORUS_RATE 50->38, CHORUS_DEPTH 80->126, DELAY_TIME 127->113.

| Physical measure | Reference | Converted | Error |
|---|---:|---:|---:|
| EG_ATTACK (attack) | 0.0018249 | 0.0018737 | 2.7% |
| EG_DECAY (decay) | 0.26151 | 0.26851 | 2.7% |
| EG_RELEASE (release) | 0.26151 | 0.26851 | 2.7% |
| AMP_ATTACK (attack) | 0.0018249 | 0.0018737 | 2.7% |
| AMP_DECAY (decay) | 0.37474 | 0.38477 | 2.7% |
| AMP_RELEASE (release) | 0.37474 | 0.38477 | 2.7% |
| LFO_RATE (lfo_rate) | 1.0825 | 1.0822 | -0.0% |
| LFO_FADE_TIME (lfo_fade) | 1.984 | 1.984 | 0.0% |
| CHORUS_RATE (chorus_rate) | 0.10729 | 0.10729 | 0.0% |
| CHORUS_DEPTH (chorus_depth) | 0.013333 | 0.0105 | -21.3% |
| DELAY_TIME (delay_time) | 0.68 | 0.34 | -50.0% |
| PORTAMENTO (portamento) | 0 | 0 | n/a |
| FILTER_CUTOFF (Hz at input note60, no modulation) | 2093 | 2093 | -0.0% |

Times are seconds; envelope values are exponential time constants, not acoustic attack lengths. Rates are Hz.

low pitch estimate: 65.86 -> 65.85 Hz; confidence 0.91/0.67.

mid pitch estimate: unreliable for this modulated waveform; not reported.

high pitch estimate: unreliable for this modulated waveform; not reported.

## Space Piano

Peak 6272 -> 10271; RMS 753.4 -> 923.5; full-scale samples 0 -> 0.

Changes: EG_ATTACK 8->18, EG_DECAY 80->90, EG_RELEASE 80->90, LFO_RATE 40->28, LFO_FADE_TIME 80->99, AMP_GAIN 115->97, AMP_ATTACK 10->20, AMP_DECAY 85->95, AMP_RELEASE 90->100, CHORUS_RATE 35->23, CHORUS_DEPTH 100->126, DELAY_TIME 127->113.

| Physical measure | Reference | Converted | Error |
|---|---:|---:|---:|
| EG_ATTACK (attack) | 0.0032451 | 0.003332 | 2.7% |
| EG_DECAY (decay) | 0.18249 | 0.18737 | 2.7% |
| EG_RELEASE (release) | 0.18249 | 0.18737 | 2.7% |
| AMP_ATTACK (attack) | 0.0037474 | 0.0038477 | 2.7% |
| AMP_DECAY (decay) | 0.26151 | 0.26851 | 2.7% |
| AMP_RELEASE (release) | 0.37474 | 0.38477 | 2.7% |
| LFO_RATE (lfo_rate) | 0.34082 | 0.34046 | -0.1% |
| LFO_FADE_TIME (lfo_fade) | 3.5413 | 3.5307 | -0.3% |
| CHORUS_RATE (chorus_rate) | 0.045061 | 0.045061 | 0.0% |
| CHORUS_DEPTH (chorus_depth) | 0.016667 | 0.0105 | -37.0% |
| DELAY_TIME (delay_time) | 0.68 | 0.34 | -50.0% |
| PORTAMENTO (portamento) | 0 | 0 | n/a |
| FILTER_CUTOFF (Hz at input note60, no modulation) | 1174.7 | 1174.7 | -0.0% |

Times are seconds; envelope values are exponential time constants, not acoustic attack lengths. Rates are Hz.

low pitch estimate: unreliable for this modulated waveform; not reported.

mid pitch estimate: unreliable for this modulated waveform; not reported.

high pitch estimate: unreliable for this modulated waveform; not reported.

## Robot Voice

Peak 5239 -> 6911; RMS 533.0 -> 727.8; full-scale samples 0 -> 0.

Changes: FILTER_CUTOFF 75->63, EG_ATTACK 0->10, EG_DECAY 60->70, EG_RELEASE 40->50, PORTAMENTO 30->40, LFO_RATE 70->58, AMP_GAIN 120->119, AMP_ATTACK 0->10, AMP_DECAY 70->80, AMP_RELEASE 50->60, CHORUS_MIX 80->57, CHORUS_RATE 80->68, CHORUS_DEPTH 60->120, DELAY_TIME 127->113.

| Physical measure | Reference | Converted | Error |
|---|---:|---:|---:|
| EG_ATTACK (attack) | 0.0018249 | 0.0018737 | 2.7% |
| EG_DECAY (decay) | 0.043275 | 0.044433 | 2.7% |
| EG_RELEASE (release) | 0.010262 | 0.010537 | 2.7% |
| AMP_ATTACK (attack) | 0.0018249 | 0.0018737 | 2.7% |
| AMP_DECAY (decay) | 0.088866 | 0.091244 | 2.7% |
| AMP_RELEASE (release) | 0.021073 | 0.021637 | 2.7% |
| LFO_RATE (lfo_rate) | 1.929 | 1.929 | 0.0% |
| LFO_FADE_TIME (lfo_fade) | 0 | 0 | n/a |
| CHORUS_RATE (chorus_rate) | 0.60761 | 0.60725 | -0.1% |
| CHORUS_DEPTH (chorus_depth) | 0.01 | 0.01 | 0.0% |
| DELAY_TIME (delay_time) | 0.68 | 0.34 | -50.0% |
| PORTAMENTO (portamento) | 0.0024986 | 0.0025655 | 2.7% |
| FILTER_CUTOFF (Hz at input note60, no modulation) | 493.88 | 493.88 | -0.0% |

Times are seconds; envelope values are exponential time constants, not acoustic attack lengths. Rates are Hz.

low pitch estimate: unreliable for this modulated waveform; not reported.

mid pitch estimate: unreliable for this modulated waveform; not reported.

high pitch estimate: unreliable for this modulated waveform; not reported.

## Synthwave

Peak 4675 -> 4831; RMS 819.9 -> 756.3; full-scale samples 0 -> 0.

Changes: FILTER_CUTOFF 85->81, EG_ATTACK 30->40, EG_DECAY 100->110, EG_RELEASE 100->110, PORTAMENTO 20->30, LFO_RATE 25->13, LFO_FADE_TIME 100->119, AMP_GAIN 110->93, AMP_ATTACK 40->50, AMP_DECAY 90->100, AMP_RELEASE 110->120, CHORUS_RATE 25->13, CHORUS_DEPTH 110->126, DELAY_TIME 127->113.

| Physical measure | Reference | Converted | Error |
|---|---:|---:|---:|
| EG_ATTACK (attack) | 0.015803 | 0.016226 | 2.7% |
| EG_DECAY (decay) | 0.76954 | 0.79014 | 2.7% |
| EG_RELEASE (release) | 0.76954 | 0.79014 | 2.7% |
| AMP_ATTACK (attack) | 0.032451 | 0.03332 | 2.7% |
| AMP_DECAY (decay) | 0.37474 | 0.38477 | 2.7% |
| AMP_RELEASE (release) | 1.5803 | 1.6226 | 2.7% |
| LFO_RATE (lfo_rate) | 0.14305 | 0.14305 | 0.0% |
| LFO_FADE_TIME (lfo_fade) | 7.296 | 7.2427 | -0.7% |
| CHORUS_RATE (chorus_rate) | 0.025034 | 0.025034 | 0.0% |
| CHORUS_DEPTH (chorus_depth) | 0.018333 | 0.0105 | -42.7% |
| DELAY_TIME (delay_time) | 0.68 | 0.34 | -50.0% |
| PORTAMENTO (portamento) | 0.0012168 | 0.0012493 | 2.7% |
| FILTER_CUTOFF (Hz at input note60, no modulation) | 880 | 905.79 | 2.9% |

Times are seconds; envelope values are exponential time constants, not acoustic attack lengths. Rates are Hz.

low pitch estimate: 65.49 -> 66.14 Hz; confidence 0.91/0.94.

mid pitch estimate: 131.42 -> 131.49 Hz; confidence 0.97/0.98.

high pitch estimate: unreliable for this modulated waveform; not reported.

## Bleep Bloop

Peak 11755 -> 7992; RMS 502.3 -> 476.0; full-scale samples 0 -> 0.

Changes: EG_ATTACK 0->10, EG_DECAY 50->60, EG_RELEASE 30->60, PORTAMENTO 50->60, LFO_RATE 90->78, AMP_GAIN 120->127, AMP_ATTACK 0->10, AMP_DECAY 60->70, AMP_RELEASE 40->70, REL_EQ_DECAY 64->0, CHORUS_MIX 50->31, CHORUS_RATE 60->48, CHORUS_DEPTH 40->80, DELAY_TIME 127->113.

| Physical measure | Reference | Converted | Error |
|---|---:|---:|---:|
| EG_ATTACK (attack) | 0.0018249 | 0.0018737 | 2.7% |
| EG_DECAY (decay) | 0.021073 | 0.021637 | 2.7% |
| EG_RELEASE (release) | 0.021073 | 0.021637 | 2.7% |
| AMP_ATTACK (attack) | 0.0018249 | 0.0018737 | 2.7% |
| AMP_DECAY (decay) | 0.043275 | 0.044433 | 2.7% |
| AMP_RELEASE (release) | 0.043275 | 0.044433 | 2.7% |
| LFO_RATE (lfo_rate) | 6.1247 | 6.1247 | 0.0% |
| LFO_FADE_TIME (lfo_fade) | 0 | 0 | n/a |
| CHORUS_RATE (chorus_rate) | 0.19133 | 0.19097 | -0.2% |
| CHORUS_DEPTH (chorus_depth) | 0.0066667 | 0.0066667 | 0.0% |
| DELAY_TIME (delay_time) | 0.68 | 0.34 | -50.0% |
| PORTAMENTO (portamento) | 0.010537 | 0.010819 | 2.7% |
| FILTER_CUTOFF (Hz at input note60, no modulation) | 3729.3 | 3729.3 | -0.0% |

Times are seconds; envelope values are exponential time constants, not acoustic attack lengths. Rates are Hz.

low pitch estimate: 65.72 -> 65.80 Hz; confidence 0.58/0.51.

mid pitch estimate: unreliable for this modulated waveform; not reported.

high pitch estimate: unreliable for this modulated waveform; not reported.

## Files and reproduction

Each 18-second file contains notes 48/60/84 at velocities 48/100/127 (one second per case, release at 0.5s), repeated note60 at 9-11s, a four-note chord at 11s, fifth note at 12s, all released at 13s, then five seconds of tail. Reference WAVs: build-audit/preset-conversion/compare-0/custom-0..5.wav. Converted WAVs: compare-1/custom-0..5.wav. Order follows the six sections above. No level normalization.

Run tools/preset-conversion/generate.py, build_bank.py, compare.py and report.py in order. Probes expose private state only in generated test copies. Factory settings are tested against direct upstream loading; user conversions are tested for source mutation and bounds. Full JSON metrics sit alongside the WAVs.

## Firmware and data protection

Candidate: `build-audit/bank218-native48/Dodepan_2.18.2_Factory16_ConvertedCustom_NoSaves.uf2`
SHA256: `18e7e7aa75e6089e86a4cb9dd0ebc7977a64f477d89d6bed2bb1e02e900f0193`
Version: `2.7.0-test-bank218`. Native 48 kHz, 256 frames, four voices, 288 MHz, saves/capture OFF. OLED reports 48k; CPU percentages use the 5.333ms transport period. Static SRAM 374,848 bytes plus 10,240 bytes reserved minimum heap/stacks. Release build/SRAM check pass; hardware playback remains untested.

Custom IDs 0-5 retain their names. Factory IDs 6-13 load upstream programs16-23; IDs18-25 load programs24-31. User IDs14-17 remain stable. Factory program31 is named --- upstream, and is exposed as supplied. Existing factory names at IDs6-13 are intentionally replaced by the upstream bank names.

Original custom arrays and upstream DSP source are unchanged. Legacy user slots are converted only into temporary controller values; stored records and RAM originals are not overwritten. Preset-store requests are disabled for this evaluation to prevent converted values being stored as legacy data; the existing store screen is still present but does not save. No new persisted format is introduced. MIDI controller edits operate in native target units. Editing and saving the new extended controls needs a separate persistence/UI migration.

The earlier candidate and reference UF2s remain available for rollback. Device acceptance: compare musical character and A/P/U/L/I; require peak<80%, no stuck notes/growing delivery counters, then a 30-minute run. Pop investigation remains parked.

Comparison bundle: `build-audit/preset-conversion/Preset-Comparisons.zip`. Source generator requires the installed MSVC toolchain; report analysis uses NumPy.
