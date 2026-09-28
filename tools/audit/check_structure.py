"""Audit structural invariants and compare pinned engine source, without edits."""
import difflib,json,pathlib,re,subprocess,sys
from build_baseline import ROOT,OUT,DOC

def table(text,name):
    body=re.search(r'\b'+name+r'\[\]\s*=\s*\{(.*?)\};',text,re.S).group(1)
    return [x.strip() for x in re.sub(r'//[^\n]*','',body).split(',') if x.strip()]

def main():
    preset=(ROOT/'instrument_preset.h').read_text();params=table(preset,'dodepan_program_parameters')
    count=int(re.search(r'#define PROGRAM_PARAMS_NUM\s+(\d+)',preset).group(1))
    pio=(ROOT/'lib/sound_i2s/sound_i2s_16bits.pio').read_text().split('% c-sdk')[0]
    labels={};code=[]
    for line in pio.splitlines():
        line=line.split(';')[0].strip()
        if not line or line.startswith('.'):continue
        if line.endswith(':'):labels[line[:-1]]=len(code)
        else:code.append(line.split(' side ')[0].strip())
    pc=cycles=bits=0;x=0
    while True:
        ins=code[pc];next_pc=pc+1;cycles+=1
        if ins.startswith('set x,'):x=int(ins.split(',')[1])
        elif ins.startswith('out pins,'):bits+=int(ins.split(',')[1])
        elif ins.startswith('jmp x-- '):
            if x!=0:next_pc=labels[ins.split()[-1]]
            x=(x-1)&0xffffffff
        else:raise ValueError('Unhandled PIO instruction: '+ins)
        pc=next_pc%len(code)
        if pc==0:break
        if cycles>1000:raise ValueError('PIO does not wrap')
    cfg=(ROOT/'config.h').read_text()
    hz=int(re.search(r'#define SOUND_OUTPUT_FREQUENCY\s+(\d+)',cfg).group(1))
    clock=int(re.search(r'#define SYSTEM_CLOCK_KHZ\s+(\d+)',cfg).group(1))
    buffer_length=int(re.search(r'#define AUDIO_BUFFER_LENGTH\s+(\d+)',cfg).group(1))
    divider_expression=re.search(r'sm_config_set_clkdiv\(&sm_config, (.*)\);',(ROOT/'lib/sound_i2s/sound_i2s_16bits.pio').read_text()).group(1)
    # Evaluate only the captured numeric arithmetic with builtins disabled.
    arithmetic=divider_expression.replace('clock_get_hz(clk_sys)',str(clock*1000)).replace('64.0f','64.0').replace('f_clk_sys',str(clock)).replace('sample_rate',str(hz)).replace('1000.f','1000.0').replace('2.0f','2.0')
    if not re.fullmatch(r'[0-9.()+*/ -]+',arithmetic):raise ValueError('Unexpected divider expression')
    divider=eval(arithmetic,{'__builtins__':{}},{})
    actual=clock*1000/divider/cycles
    local=ROOT/'lib/digital-synth-pra32-u2';old=OUT/'upstream-v1.6.0';new=OUT/'upstream-v2.18.2'
    changed=[];missing=[];patch=[]
    for p in old.rglob('*'):
        rel=p.relative_to(old)
        if not p.is_file() or '.git' in rel.parts:continue
        q=local/rel
        if not q.exists():missing.append(rel.as_posix());continue
        if p.read_bytes().replace(b'\r\n',b'\n')!=q.read_bytes().replace(b'\r\n',b'\n'):
            changed.append(rel.as_posix())
            try:patch.extend(difflib.unified_diff(p.read_text().splitlines(True),q.read_text().splitlines(True),fromfile='v1.6.0/'+rel.as_posix(),tofile='local/'+rel.as_posix()))
            except UnicodeError:pass
    result={'parameter_declared_count':count,'parameter_actual_count':len(params),'omitted_parameters':params[count:],'legacy_flash_bytes':244,'full_parameter_flash_bytes':16+4*len(params)+4*12,'pio_cycles_per_frame':cycles,'pio_bits_per_frame':bits,'configured_hz':hz,'calculated_lrclk_hz':actual,'calculated_bclk_hz':actual*bits,'configured_buffer_us':buffer_length/hz*1e6,'calculated_buffer_us':buffer_length/actual*1e6,'local_changes_from_v1.6.0':changed,'missing_from_local':missing,'pins':{'v1.6.0':'60040e7a41f391bf60805afaecfa503a45954414','v2.18.2':'ad082d08d825e979023f1dc7cae9f6a0dab45af5'}}
    (DOC/'structural-result.json').write_text(json.dumps(result,indent=2)+'\n')
    (DOC/'engine-local.patch').write_text(''.join(patch))
    print(json.dumps(result,indent=2))
    failures=0
    for name,ok in [('parameter-table-count',count==len(params)),('i2s-sample-rate',abs(actual-hz)<1)]:
        print(('PASS' if ok else 'FAIL')+' '+name);failures+=not ok
    return int(bool(failures))
if __name__=='__main__':sys.exit(main())
