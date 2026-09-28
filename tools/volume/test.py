from pathlib import Path
import sys,shutil,subprocess
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools/audit'))
from build_baseline import vs_environment
out=ROOT/'build-audit/volume-tests';out.mkdir(exist_ok=True)
env,_,_=vs_environment();cl=shutil.which('cl.exe',path=env['PATH'])
cmd=[cl,'/nologo','/std:c++17','/EHsc','/I'+str(ROOT),str(Path(__file__).with_name('test.cpp')),'/Fe:'+str(out/'test.exe'),'/Fo:'+str(out/'test.obj')]
p=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(out/'compile.log').write_bytes(p.stdout)
if p.returncode: print(p.stdout.decode(errors='replace'));p.check_returncode()
subprocess.run([str(out/'test.exe')],check=True)
