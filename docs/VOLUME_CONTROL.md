# Volume control and saving

Version 2.7.3-test-volume-save: 0 is mute; 1 exactly matches the previous level20 (Q15 gain123); 100 remains full scale (32768). Intermediate values are logarithmically spaced: round(123 * (32768/123)^((level-1)/99)). Default volume is30 on this revised scale. Existing 10ms ramps and numeric display remain.

Settings saves are enabled in this candidate. Encoder volume changes restart the10-second delayed-save timer; saving waits for playback/touches to stop and the audio tail to become quiet. The audio core ramps down and suspends before the existing coordinated flash transaction. Alternating sectors, CRC, generation selection, and readback verification protect settings writes. Wait at least10 seconds after the last adjustment, stop playback, then allow another second before power-off. Device verification of save/resume remains required.

Settings format3 distinguishes this curve. First load of DODE, DOD2-v1 or v2 initializes volume30 and schedules a version3 record while retaining other settings and original legacy preset parameter arrays. Later boots restore the saved value, including mute. Invalid old volume values are rejected. Old firmware cannot read format3; old firmware artifacts and the original legacy flash sector remain retained, but older dual-sector records may be rotated out by subsequent saves.

This enables general settings/volume and scale saving. Native edited synth-patch storage remains disabled; temporary converted legacy patches must not be written back as legacy controller data.

Validation: python tools/volume/test.py passes curve endpoints/monotonicity,24/48k ramps and retargeting, old-format defaults without source mutation, all101 format3 volume roundtrips, and corrupted-newest-record fallback. Device acceptance: first boot30; set a low value, wait idle for save, power-cycle and confirm; repeat for0 and another level. Play/release notes before and after saves, checking no stuck notes and U/L/I remain stable.
