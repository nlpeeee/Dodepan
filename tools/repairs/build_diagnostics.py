"""Build one-variable diagnostic variants; this does not flash hardware."""
import os,sys,json,pathlib,subprocess,shutil
root=pathlib.Path(__file__).resolve().parents[2]
out=root/'build-audit/diagnostics';out.mkdir(parents=True,exist_ok=True)
variants={'baseline':{},'clock150':{'DODEPAN_CLOCK_KHZ':150000},'clock192':{'DODEPAN_CLOCK_KHZ':192000},'clock240':{'DODEPAN_CLOCK_KHZ':240000},'buffer256':{'DODEPAN_BUFFER_FRAMES':256},'no-display':{'DODEPAN_DISABLE_DISPLAY':'ON'},'no-imu':{'DODEPAN_DISABLE_IMU':'ON'},'no-saves':{'DODEPAN_DISABLE_PERSISTENCE':'ON'}}
results={}
for name,changes in variants.items():
    settings=dict(DODEPAN_CLOCK_KHZ=288000,DODEPAN_BUFFER_FRAMES=128,DODEPAN_DISABLE_DISPLAY='OFF',DODEPAN_DISABLE_IMU='OFF',DODEPAN_DISABLE_PERSISTENCE='OFF',DODEPAN_UART_DIAGNOSTICS='ON');settings.update(changes)
    dest=out/name;dest.mkdir(exist_ok=True)
    env=os.environ.copy();env.update(DODEPAN_BUILD_DIR=str(root/'build-audit/diagnostic-build'),DODEPAN_AUDIT_RESULTS=str(dest),DODEPAN_CMAKE_ARGS=json.dumps([f'-D{k}={v}' for k,v in settings.items()]))
    print(name,flush=True)
    r=subprocess.run([sys.executable,str(root/'tools/audit/build_baseline.py')],env=env)
    if r.returncode:sys.exit(r.returncode)
    shutil.copy2(root/'build-audit/diagnostic-build/Dodepan_RP2350.uf2',dest/'Dodepan_RP2350.uf2')
    results[name]={'settings':settings,'result':json.loads((dest/'build-result.json').read_text())}
(out/'manifest.json').write_text(json.dumps(results,indent=2))
