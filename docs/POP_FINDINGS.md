# Deterministic pop investigation

No production DSP or preset values were changed. Hooks are injected into a generated header under build-audit/pop-audit. Twelve instrumented WAVs match their uninstrumented counterparts byte-for-byte; a separate process repeats the fifth-note render exactly.

| Case | Mode | Final clamp samples | Steals | Retriggers | Mono transitions | Peak / clamp limit |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| held | normal | 0 | 0 | 0 | 1 | 0.248 |
| fifth | normal | 0 | 0 | 0 | 5 | 0.248 |
| retrigger | normal | 0 | 0 | 0 | 8 | 0.248 |
| chords | normal | 0 | 0 | 0 | 18 | 0.240 |
| show-yourself | normal | 0 | 0 | 0 | 1421 | 0.269 |
| let-it-go | normal | 0 | 0 | 0 | 1171 | 0.276 |
| held | poly | 0 | 0 | 0 | 0 | 0.166 |
| fifth | poly | 0 | 1 | 0 | 0 | 0.198 |
| retrigger | poly | 0 | 0 | 7 | 0 | 0.166 |
| chords | poly | 0 | 0 | 0 | 0 | 0.289 |
| show-yourself | poly | 0 | 412 | 586 | 0 | 0.351 |
| let-it-go | poly | 0 | 0 | 0 | 0 | 0.325 |

## Interpretation

Elec. Piano is monophonic in its factory state (VOICE_MODE=76). Firmware chord mode forces VOICE_POLYPHONIC=0, so both conditions were rendered. The controlled fifth-note case proves that the observer sees a forced steal; the repeated-note case proves it sees retriggers. Show Yourself has many more voice transitions than Let It Go in polyphonic mode, but this correlation is not proof that those transitions cause the audible pops. The largest sample jumps do not consistently occur at the transition instant. Effects can delay their appearance; normal waveform slopes and attacks can also produce large jumps.

No render activated the final output clamp. Do not reduce patch gain or bypass effects as a production repair based on these results. Internal saturation elsewhere and platform-specific behavior remain possible.

## Listening gate

Listen to the paired normal/poly excerpts for Show Yourself and the fifth-note case. Report whether the familiar pop is present, and its approximate position in the excerpt. Excerpts are unnormalised mono PCM at 24 kHz, representing the duplicated left channel sent to the speaker at full digital volume. Use microphone recordings of the same device passage for comparison. No direct amplifier-terminal connection is required.

If the familiar pop cannot be located in the host PCM, proceed to the planned bounded pre-I2S device capture before investigating hardware or changing DSP. On-device capture/export and hardware comparisons remain pending this listening gate. The existing rc4 device firmware remains the reference.

## Reproduce

Run `python tools/pop-audit/run.py` then `python tools/pop-audit/analyse.py`. Outputs, manifests and full traces are in `build-audit/pop-audit/`. MSVC is the existing VS Build Tools compiler. Compilation reports an existing filter narrowing warning; no attempt was made to change that DSP arithmetic.

## Limits

- Host MSVC arithmetic has not yet been compared with captured ARM PCM.
- Songs are rendered through Elec. Piano; the instrument used in the user song test is not confirmed.
- Large sample differences are observations, not automatic pop detections.
- Clipping counters observe the final engine clamp, not every possible internal saturation.
- Song schedule is sample-timed rather than reproducing main-loop polling jitter.

## Device capture stage

The user heard no pops in the paired Show Yourself host excerpts. This supersedes the listening gate above. A diagnostic device PCM capture is now built and checked on the computer; see POP_CAPTURE.md. Hardware transfer and matched recordings remain untested. No causal conclusion or sound correction follows from the negative host listening result.
