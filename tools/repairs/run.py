"""Compile and run repair regressions using the actual portable firmware modules."""
import sys,pathlib,shutil,subprocess,json
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'audit'))
from build_baseline import ROOT,OUT,vs_environment
out=OUT/'repair-tests';out.mkdir(parents=True,exist_ok=True)
env,_,_=vs_environment();results={}
for name in ['playback','storage_audio']:
    cmd=[shutil.which('cl.exe',path=env['PATH']),'/nologo','/std:c11','/W3','/Od','/I'+str(ROOT/'tools/audit/stubs'),'/I'+str(ROOT),str(ROOT/'tools/repairs'/f'{name}.c'),'/Fe:'+str(out/f'{name}.exe'),'/Fo:'+str(out/f'{name}.obj')]
    c=subprocess.run(cmd,env=env,cwd=out,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    (out/f'{name}-compile.log').write_text(c.stdout)
    if c.returncode:print(c.stdout);sys.exit(c.returncode)
    r=subprocess.run([str(out/f'{name}.exe')],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    (out/f'{name}.log').write_text(r.stdout);print(r.stdout);results[name]=r.returncode
(out/'result.json').write_text(json.dumps(results,indent=2))
sys.exit(any(results.values()))
