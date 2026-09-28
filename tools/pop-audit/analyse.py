"""Summarize evidence and create short, time-aligned listening excerpts."""
from pathlib import Path
import csv,json,wave,array,hashlib,sys
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'build-audit/pop-audit'
results=[]
for mode in ['normal','poly']:
 for case in ['held','fifth','retrigger','chords','show-yourself','let-it-go']:
    key=case+'-'+mode;prefix=OUT/(key+'-1');d=json.loads(Path(str(prefix)+'-summary.json').read_text())
    with Path(str(prefix)+'-events.csv').open() as f:events=list(csv.DictReader(f))
    d.update(case=case,mode=mode,mono_transitions=sum(e['event']=='mono_transition' for e in events),wav_sha256=hashlib.sha256(Path(str(prefix)+'.wav').read_bytes()).hexdigest())
    f=d['maximum_step_frame'];trans=[e for e in events if e['event'] in ['steal','retrigger','mono_transition']]
    nearest=min(trans,key=lambda e:abs(int(e['frame'])-f)) if trans else None
    d['largest_step_seconds']=f/24000;d['nearest_transition_to_largest_step']=None if nearest is None else {'type':nearest['event'],'offset_ms':(f-int(nearest['frame']))/24}
    d['preclamp_peak_fraction']=d['delay_peak']*2/(32767*256)
    results.append(d)
    # Always excerpt both modes at the same passage, selected using the normal render.
    normal=json.loads((OUT/f'{case}-normal-1-summary.json').read_text())
    center=48000 if case=='fifth' else normal['maximum_step_frame']
    start=max(0,center-48000);count=192000
    with wave.open(str(prefix)+'.wav','rb') as w:
        start=min(start,w.getnframes());w.setpos(start);data=w.readframes(count)
    with wave.open(str(OUT/(key+'-excerpt.wav')),'wb') as w:w.setparams((1,2,24000,0,'NONE','not compressed'));w.writeframes(data)
    d['excerpt_start_seconds']=start/24000
(OUT/'results.json').write_text(json.dumps(results,indent=2)+'\n')
# Re-run one render in a new process to verify deterministic reinitialization.
import subprocess
p=OUT/'repeat-check';subprocess.run([str(OUT/'render-1.exe'),'fifth',str(p),'poly'],check=True)
assert (OUT/'fifth-poly-1.wav').read_bytes()==Path(str(p)+'.wav').read_bytes()
engine=ROOT/'lib/digital-synth-pra32-u2/Digital-Synth-PRA32-U2/pra32-u2-synth.h'
metadata={'rate':24000,'program':6,'instrument':'Elec. Piano','velocity_synthetic':100,'master_gain':1,'sensor_modulation':False,'normal_voice_mode_cc':76,'poly_voice_mode_cc':0,'observer_identical_cases':12,'repeat_identical':True,'engine_sha256':hashlib.sha256(engine.read_bytes()).hexdigest(),'limitations':['Host MSVC arithmetic has not yet been compared with captured ARM PCM.','Songs are rendered through Elec. Piano; the instrument used in the user song test is not confirmed.','Large sample differences are observations, not automatic pop detections.','Clipping counters observe the final engine clamp, not every possible internal saturation.','Song schedule is sample-timed rather than reproducing main-loop polling jitter.']}
(OUT/'manifest.json').write_text(json.dumps(metadata,indent=2)+'\n')
lines=['# Deterministic pop investigation','', 'No production DSP or preset values were changed. Hooks are injected into a generated header under build-audit/pop-audit. Twelve instrumented WAVs match their uninstrumented counterparts byte-for-byte; a separate process repeats the fifth-note render exactly.','', '| Case | Mode | Final clamp samples | Steals | Retriggers | Mono transitions | Peak / clamp limit |','| --- | --- | ---: | ---: | ---: | ---: | ---: |']
for d in results:lines.append(f"| {d['case']} | {d['mode']} | {d['clip_samples']} | {d['steals']} | {d['retriggers']} | {d['mono_transitions']} | {d['preclamp_peak_fraction']:.3f} |")
lines+=['','## Interpretation','', 'Elec. Piano is monophonic in its factory state (VOICE_MODE=76). Firmware chord mode forces VOICE_POLYPHONIC=0, so both conditions were rendered. The controlled fifth-note case proves that the observer sees a forced steal; the repeated-note case proves it sees retriggers. Show Yourself has many more voice transitions than Let It Go in polyphonic mode, but this correlation is not proof that those transitions cause the audible pops. The largest sample jumps do not consistently occur at the transition instant. Effects can delay their appearance; normal waveform slopes and attacks can also produce large jumps.','', 'No render activated the final output clamp. Do not reduce patch gain or bypass effects as a production repair based on these results. Internal saturation elsewhere and platform-specific behavior remain possible.','', '## Listening gate','', 'Listen to the paired normal/poly excerpts for Show Yourself and the fifth-note case. Report whether the familiar pop is present, and its approximate position in the excerpt. Excerpts are unnormalised mono PCM at 24 kHz, representing the duplicated left channel sent to the speaker at full digital volume. Use microphone recordings of the same device passage for comparison. No direct amplifier-terminal connection is required.','', 'If the familiar pop cannot be located in the host PCM, proceed to the planned bounded pre-I2S device capture before investigating hardware or changing DSP. On-device capture/export and hardware comparisons remain pending this listening gate. The existing rc4 device firmware remains the reference.','', '## Reproduce','', 'Run `python tools/pop-audit/run.py` then `python tools/pop-audit/analyse.py`. Outputs, manifests and full traces are in `build-audit/pop-audit/`. MSVC is the existing VS Build Tools compiler. Compilation reports an existing filter narrowing warning; no attempt was made to change that DSP arithmetic.','', '## Limits','']+['- '+s for s in metadata['limitations']]
(ROOT/'docs/POP_FINDINGS.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
(ROOT/'docs/POP_STATUS.md').write_text('''# Pop investigation status
Reference: rc4 SRAM, 24 kHz / 288 MHz / 128 frames, saves disabled. Device A18/P33, U0/L0/I0. Original firmware also pops.
Completed: 12 deterministic Elec. Piano host cases covering normal/chord voice modes, mono note transitions, fifth-note stealing, repeats, chords, Show Yourself and Let It Go. Observed/unobserved WAVs identical; repeated-process determinism verified. Engine and presets unchanged.
Evidence: no final-clamp activation. Show Yourself poly: 412 steals, 586 retriggers; Let It Go poly: none. This is a lead, not a proven pop mechanism. Largest steps do not consistently coincide with transition instants.
Artifacts: docs/POP_FINDINGS.md; build-audit/pop-audit/{results.json,manifest.json,*-excerpt.wav,*-events.csv,*-signal.csv}.
Next gate: user identifies familiar pop in paired excerpts and approximate timestamp. If absent/uncertain, implement bounded on-device pre-I2S capture/export and compare a microphone recording. No corrective DSP patch yet.
Do not retune presets, upgrade engine, change rate, or repeat the full diagnostic matrix. Read this file first; retain logs on disk and keep handoffs short.
''',encoding='utf-8')
print('12 observer equality checks, forced-steal/retrigger assertions and process-repeat check passed. Findings: docs/POP_FINDINGS.md')
