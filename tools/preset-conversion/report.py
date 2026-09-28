from pathlib import Path
import json,re,numpy as np
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'build-audit/preset-conversion'
m=json.loads((OUT/'physical-mappings.json').read_text())
s=(ROOT/'instrument_preset.h').read_text().split('dodepan_program_parameters[] = {',1)[1].split('};',1)[0];controls=[x.strip() for x in re.sub(r'//[^\n]*','',s).split(',') if x.strip()]
names=['Dodepan','Magic Bell','Space Piano','Robot Voice','Synthwave','Bleep Bloop']
rows=[[[int(v) for v in line.split(',')] for line in (OUT/f'compare-{mode}/parameters.csv').read_text().splitlines()] for mode in [0,1]]
def pitch(x,sr,n):
    expected=440*2**((n-69)/12)/2
    if np.sqrt(np.mean(x*x))<2:return None
    x=x-x.mean();size=1<<(2*len(x)-1).bit_length();sp=np.fft.rfft(x,size);ac=np.fft.irfft(sp*np.conj(sp),size)[:len(x)]
    lo=max(2,int(sr/expected/1.2));hi=min(len(x)-2,int(sr/expected/.8));lag=lo+int(np.argmax(ac[lo:hi+1]));conf=ac[lag]/ac[0]
    if lag in [lo,hi] or conf<.5:return None
    den=ac[lag-1]-2*ac[lag]+ac[lag+1];delta=float(np.clip(.5*(ac[lag-1]-ac[lag+1])/den,-.5,.5)) if den else 0
    return {'hz':sr/(lag+delta),'confidence':float(conf)}
result=[]
for p,name in enumerate(names):
    obj={'name':name,'changes':[{'control':cc,'old':rows[0][p][j],'new':rows[1][p][j]} for j,cc in enumerate(controls) if rows[0][p][j]!=rows[1][p][j]],'render':[],'physical':[]}
    for mode,sr in [(0,24000),(1,48000)]:
        x=np.fromfile(OUT/f'compare-{mode}/custom-{p}.pcm',dtype='<i2').astype(float)
        obj['render'].append({'rate':sr,'peak':int(np.max(np.abs(x))),'rms':float(np.sqrt(np.mean(x*x))),'full_scale_samples':int(np.sum(np.abs(x)>=32767)),'pitch':[pitch(x[int((sec+.15)*sr):int((sec+.45)*sr)],sr,n) for sec,n in [(1,48),(4,60),(7,84)]]})
    for cc,metric in [('EG_ATTACK','attack'),('EG_DECAY','decay'),('EG_RELEASE','release'),('AMP_ATTACK','attack'),('AMP_DECAY','decay'),('AMP_RELEASE','release'),('LFO_RATE','lfo_rate'),('LFO_FADE_TIME','lfo_fade'),('CHORUS_RATE','chorus_rate'),('CHORUS_DEPTH','chorus_depth'),('DELAY_TIME','delay_time'),('PORTAMENTO','portamento')]:
        j=controls.index(cc);ov=rows[0][p][j];nv=rows[1][p][j]
        if cc.endswith('RELEASE') and rows[0][p][controls.index('REL_EQ_DECAY')]>=64:ov=rows[0][p][controls.index(cc.replace('RELEASE','DECAY'))]
        target=m['physical'][metric]['old'][ov];actual=m['physical'][metric]['new'][nv]
        obj['physical'].append({'control':cc,'metric':metric,'reference':target,'converted':actual,'error':None if not target else actual/target-1})
    j=controls.index('FILTER_CUTOFF');k=rows[0][p][controls.index('FILTER_KEY_TRK')];target=m['old_filter_hz'][rows[0][p][j]];actual=m['new_filter_hz_by_keytrack'][k][rows[1][p][j]]
    obj['physical'].append({'control':'FILTER_CUTOFF','metric':'Hz at input note60, no modulation','reference':target,'converted':actual,'error':actual/target-1});result.append(obj)
(OUT/'comparison.json').write_text(json.dumps(result,indent=2))
doc=ROOT/'docs/PRESET_CONVERSION.md';text=doc.read_text().split('## Dodepan\n')[0]
for p in result:
    a,b=p['render'];text+='## '+p['name']+'\n\n'+f"Peak {a['peak']} -> {b['peak']}; RMS {a['rms']:.1f} -> {b['rms']:.1f}; full-scale samples {a['full_scale_samples']} -> {b['full_scale_samples']}.\n\n"
    text+='Changes: '+', '.join(f"{v['control']} {v['old']}->{v['new']}" for v in p['changes'])+'.\n\n'
    text+='| Physical measure | Reference | Converted | Error |\n|---|---:|---:|---:|\n'
    for v in p['physical']:
        error='n/a' if v['error'] is None else f"{100*v['error']:.1f}%"
        text+=f"| {v['control']} ({v['metric']}) | {v['reference']:.5g} | {v['converted']:.5g} | {error} |\n"
    text+='\nTimes are seconds; envelope values are exponential time constants, not acoustic attack lengths. Rates are Hz.\n\n'
    for label,x,y in zip(['low','mid','high'],a['pitch'],b['pitch']):
        if x and y:text+=f"{label} pitch estimate: {x['hz']:.2f} -> {y['hz']:.2f} Hz; confidence {x['confidence']:.2f}/{y['confidence']:.2f}.\n\n"
        else:text+=label+' pitch estimate: unreliable for this modulated waveform; not reported.\n\n'
    print(p['name'],f"RMS {a['rms']:.1f}->{b['rms']:.1f}, peak {b['peak']}, full-scale samples {b['full_scale_samples']}")
text+='## Files and reproduction\n\nEach 18-second file contains notes 48/60/84 at velocities 48/100/127 (one second per case, release at 0.5s), repeated note60 at 9-11s, a four-note chord at 11s, fifth note at 12s, all released at 13s, then five seconds of tail. Reference WAVs: build-audit/preset-conversion/compare-0/custom-0..5.wav. Converted WAVs: compare-1/custom-0..5.wav. Order follows the six sections above. No level normalization.\n\nRun tools/preset-conversion/generate.py, build_bank.py, compare.py and report.py in order. Probes expose private state only in generated test copies. Factory settings are tested against direct upstream loading; user conversions are tested for source mutation and bounds. Full JSON metrics sit alongside the WAVs.\n'
doc.write_text(text)
