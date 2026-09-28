"""Compile real playback C modules with host clock and output stubs.
Failures are intentionally reported as failures until the firmware is repaired.
"""
import json, pathlib, subprocess, sys, shutil
from build_baseline import ROOT, OUT, DOC, vs_environment

def main():
    env,_,_=vs_environment()
    target=OUT/'host';target.mkdir(parents=True,exist_ok=True)
    command=[shutil.which('cl.exe',path=env['PATH']),'/nologo','/std:c11','/W3','/Od','/I'+str(ROOT/'tools/audit/stubs'),'/I'+str(ROOT),str(ROOT/'tools/audit/playback_probes.c'),'/Fe:'+str(target/'playback_probes.exe'),'/Fo:'+str(target/'playback_probes.obj')]
    compile_result=subprocess.run(command,env=env,cwd=target,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    (DOC/'host-compile.log').write_text(compile_result.stdout,encoding='utf8')
    if compile_result.returncode:
        print(compile_result.stdout);return compile_result.returncode
    result=subprocess.run([str(target/'playback_probes.exe')],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    (DOC/'host-probes.log').write_text(result.stdout,encoding='utf8')
    print(result.stdout)
    (DOC/'host-result.json').write_text(json.dumps({'compile_exit_code':compile_result.returncode,'test_exit_code':result.returncode,'tests':result.stdout.splitlines()},indent=2)+'\n',encoding='utf8')
    return result.returncode
if __name__=='__main__':sys.exit(main())
