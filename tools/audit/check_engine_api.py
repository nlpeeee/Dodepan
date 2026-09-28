"""Compile the integration call surface against each pinned engine (no linking)."""
import json,subprocess
from build_baseline import ROOT,OUT,DOC
import sys
preamble='''#include <cstdint>
#include <climits>
#include <cmath>
#include <cstdio>
using boolean = bool;
using byte = uint8_t;
#define __not_in_flash_func(x) x
#include "config.h"
#include "pra32-u2-synth.h"
#include "instrument_preset.h"
PRA32_U2_Synth synth;
'''
legacy='''void smoke() { int16_t right; synth.initialize(); synth.note_on(60,100); synth.note_off(60); synth.control_change(74,64); synth.program_change(0); synth.current_controller_value(74); synth.secondary_core_process(); synth.process(right); }
'''
adapted=legacy.replace('synth.note_off(60)','synth.note_off(60,64)').replace('synth.process(right)','synth.process(0,0,right)')
results=[]
for name,path,body,defines in [('local','lib/digital-synth-pra32-u2',legacy,[]),('v2.18.2-old-api','build-audit/upstream-v2.18.2',legacy,[]),('v2.18.2-adapted-api','build-audit/upstream-v2.18.2',adapted,['-DPRA32_U2_ENABLE_POLY_ON_1_CORE'])]:
    source=OUT/(name+'-api.cpp');source.write_text(preamble+body)
    command=[str(OUT/'tools/bin/arm-none-eabi-g++.exe'),'-std=c++17','-mcpu=cortex-m33','-mthumb','-fsyntax-only','-I'+str(ROOT),'-I'+str(ROOT/path/'Digital-Synth-PRA32-U2'),*defines,str(source)]
    r=subprocess.run(command,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    (DOC/(name+'-api.log')).write_text(r.stdout)
    row={'test':name,'exit_code':r.returncode,'command':command}
    if r.returncode==0:
        obj=OUT/(name+'-size.o')
        obj_command=[x for x in command if x!='-fsyntax-only']+['-c','-O2','-o',str(obj)]
        obj_result=subprocess.run(obj_command,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
        row['object_compile_exit_code']=obj_result.returncode
        if obj_result.returncode==0:
            nm=subprocess.check_output([str(OUT/'tools/bin/arm-none-eabi-nm.exe'),'-S','--size-sort',str(obj)],text=True)
            for line in nm.splitlines():
                if line.split()[-1]=='synth':row['synth_object_bytes']=int(line.split()[1],16)
        else:print(obj_result.stdout)
    results.append(row)
    print(name, 'exit',r.returncode);print('\n'.join(r.stdout.splitlines()[:10]))
(DOC/'api-result.json').write_text(json.dumps(results,indent=2)+'\n')

expected=[0,1,0]
# The middle failure is the deliberate negative compatibility probe.
sys.exit(0 if [r['exit_code'] for r in results]==expected and all(r.get('object_compile_exit_code',0)==0 for r in results) else 1)
