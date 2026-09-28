from pathlib import Path
import sys,shutil,subprocess
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools/audit'))
from build_baseline import vs_environment
out=ROOT/'build-audit/preset-conversion';env,_,_=vs_environment();cl=shutil.which('cl.exe',path=env['PATH'])
cmd=[cl,'/nologo','/std:c++17','/EHsc','/DDODEPAN_ENGINE_218=1','/I'+str(ROOT),'/I'+str(ROOT/'lib/pra32-u2-v2.18.2/Digital-Synth-PRA32-U2'),str(Path(__file__).with_name('bank.cpp')),'/Fe:'+str(out/'bank.exe'),'/Fo:'+str(out/'bank.obj')]
a=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(out/'bank-compile.log').write_bytes(a.stdout);a.check_returncode()
rows=subprocess.check_output([str(out/'bank.exe')]).decode().splitlines();assert len(rows)==6
(ROOT/'converted_custom_presets.h').write_text('#pragma once\n#include <stdint.h>\n// Generated native bank, legacy reference delay defaults: 127,0,1.\nstatic const uint8_t converted_custom_presets[6][48]={\n'+''.join('{'+row+'},\n' for row in rows)+'};\n')
print('Generated converted custom bank.')
