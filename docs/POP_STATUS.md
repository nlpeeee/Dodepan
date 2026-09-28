# Dodepan status
Current candidate: build-audit/preset-order/Dodepan_2.18.2_OrderedPresets_Saving.uf2
SHA256: 5122e7c817d67a8119ff552a324a188591f1bd62168b9a584b0049360c9abe41
Version 2.7.4-test-preset-order. PRA32-U2 v2.18.2, native48k, four voices,288MHz,256frames; capture OFF, settings saves ON. Prior UF2s preserved.
Menu: custom sounds, factory keyboards/bass/brass/strings/pads/leads/initialization, then user slots. All26 IDs verified once; saved IDs and sounds unchanged. Build/SRAM PASS, no compiler warnings. Details docs/INSTRUMENT_MENU.md. Next: browse both directions and verify saved selection.
Volume: 0 mute, new1 exactly equals prior20 (gain123/32768),100 unchanged maximum; logarithmic steps,10ms ramps,numeric display. Default30. Old-format settings initialize30 once; format3 then restores saved volume. Volume adjustments schedule delayed save after10 seconds and playback/tail idle. Native edited-patch storage remains disabled; legacy preset arrays retained.
Checks: focused curve/ramp/format migration/101-value roundtrip/corrupted-save fallback PASS. Release/SRAM check PASS, no compiler warnings. Device save/resume verification pending.
Next test: first boot30; change volume, stop playing, wait at least11 seconds, power-cycle and verify; repeat for mute. Check notes and U/L/I after save/resume. No headphone detection or maximum cap.
Engine bank: all16 upstream factory programs IDs6-13/18-25; custom0-5 converted; user14-17 stable. docs/PRESET_CONVERSION.md and Preset-Comparisons.zip retain comparison evidence. Prior device report engine218 A32/P59 in triad demo. Latest user reports jack clean; pops PARKED.
Workflow: one agent,targeted reads,short handoffs.288MHz remains comparison overclock. No engine/preset DSP changes in volume work.
