from pathlib import Path
import sys,subprocess,shutil,json,hashlib
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/audit'))
from build_baseline import vs_environment
OUT=ROOT/'build-audit/pop-audit';OUT.mkdir(parents=True,exist_ok=True)
ENGINE=ROOT/'lib/digital-synth-pra32-u2/Digital-Synth-PRA32-U2'
s=(ENGINE/'pra32-u2-synth.h').read_text()
# Instrument a generated copy only. Never edit vendored DSP or shipped firmware.
for i in range(4):
    old=f'}} else if (m_note_on_number[{i}] == note_number) {{'
    assert s.count(old)>=1
    s=s.replace(old,old+f'\n      pop_voice({i}, note_number, note_number, true);',1)
old='      m_note_on_number[note_on_osc_index] = note_number;'
assert s.count(old)==1
s=s.replace(old,'      pop_voice(note_on_osc_index, m_note_on_number[note_on_osc_index], note_number, false);\n'+old)
old='    if ((m_voice_mode == VOICE_MONOPHONIC) ||\n        (m_voice_mode == VOICE_LEGATO) || (m_voice_mode == VOICE_LEGATO_PORTA)) {'
assert s.count(old)>=1
s=s.replace(old,old+'\n      pop_mono(m_note_on_number[0], note_number);',1)
old='    int32_t synth_output_r = delay_fx_output_r;'
assert s.count(old)==1
s=s.replace(old,'    pop_signal(voice_mixer_output, chorus_fx_output_l, delay_fx_output_l);\n'+old)
(OUT/'observed-synth.h').write_text(s)
env,_,_=vs_environment();cl=shutil.which('cl.exe',path=env['PATH'])
for observed in [0,1]:
    cmd=[cl,'/nologo','/std:c++17','/O2','/EHsc','/W3','/DOBSERVED='+str(observed),'/DBOARD_IS_PICO2','/I'+str(ROOT),'/I'+str(OUT),'/I'+str(ENGINE),str(ROOT/'tools/pop-audit/render.cpp'),'/Fe:'+str(OUT/f'render-{observed}.exe'),'/Fo:'+str(OUT/f'render-{observed}.obj')]
    r=subprocess.run(cmd,env=env,cwd=OUT,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    (OUT/f'compile-{observed}.log').write_text(r.stdout)
    if r.returncode:print(r.stdout[-4000:]);sys.exit(r.returncode)
for mode in ['normal','poly']:
 for case in ['held','fifth','retrigger','chords','show-yourself','let-it-go']:
    key=case+'-'+mode
    for observed in [0,1]:
        r=subprocess.run([str(OUT/f'render-{observed}.exe'),case,str(OUT/f'{key}-{observed}'),mode],cwd=OUT,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
        (OUT/f'{key}-{observed}.log').write_text(r.stdout)
        if r.returncode:print(r.stdout);sys.exit(r.returncode)
    a=(OUT/f'{key}-0.wav').read_bytes();b=(OUT/f'{key}-1.wav').read_bytes()
    assert a==b,f'observer altered audio: {key}'
    d=json.loads((OUT/f'{key}-1-summary.json').read_text())
    if mode=='poly' and case=='fifth':assert d['steals']==1,d
    if mode=='poly' and case=='retrigger':assert d['retriggers']==7,d
    print(key,'identical', 'clips',d['clip_samples'],'steals',d['steals'],'retrigger',d['retriggers'],flush=True)
