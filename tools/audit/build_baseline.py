"""Build the untouched firmware in build-audit/firmware using isolated SDK/tools.
Run from any directory with Python 3.9+; requires VS C++ Build Tools and the
pinned SDK/compiler downloaded during the audit (see docs/audit-2026-09-13).
"""
import hashlib, json, os, pathlib, subprocess, sys
ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / 'build-audit'
# Keep the dated evidence immutable when probes are rerun after repairs.
DOC = pathlib.Path(os.environ.get('DODEPAN_AUDIT_RESULTS', str(OUT/'results-latest'))).resolve()
DOC.mkdir(parents=True, exist_ok=True)
def vs_environment():
    env = os.environ.copy()
    where = pathlib.Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
    install = subprocess.check_output([str(where), '-latest', '-products', '*', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
    if not install: raise RuntimeError('VS C++ Build Tools not found')
    vcvars = pathlib.Path(install) / 'VC/Auxiliary/Build/vcvars64.bat'
    result = subprocess.check_output(f'cmd.exe /d /s /c ""{vcvars}" >nul && set"', text=True)
    # Windows environment names are case-insensitive; normalize to avoid duplicate Path.
    env = {k.upper(): v for k,v in env.items()}
    for line in result.splitlines():
        if '=' in line:
            k,v = line.split('=',1);env[k.upper()] = v
    common = pathlib.Path(install) / 'Common7/IDE/CommonExtensions/Microsoft/CMake'
    return env, common/'CMake/bin/cmake.exe', common/'Ninja/ninja.exe'
def main():
    env,cmake,ninja = vs_environment()
    compiler = OUT/'tools/bin'
    if not (compiler/'arm-none-eabi-gcc.exe').is_file() or not (OUT/'pico-sdk/CMakeLists.txt').is_file():
        raise RuntimeError('Run python tools/audit/prepare_tools.py first')
    env['PATH'] = str(compiler)+os.pathsep+str(cmake.parent)+os.pathsep+str(ninja.parent)+os.pathsep+env['PATH']
    env['PICO_SDK_PATH'] = str(OUT/'pico-sdk')
    build = pathlib.Path(os.environ.get('DODEPAN_BUILD_DIR', str(OUT/'firmware'))).resolve()
    commands = [[str(cmake),'-S',str(ROOT),'-B',str(build),'-G','Ninja','-DPICO_BOARD=pico2','-DCMAKE_BUILD_TYPE=Release','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON','-DCMAKE_MAKE_PROGRAM='+ninja.as_posix(),'-DPICO_TOOLCHAIN_PATH='+compiler.as_posix()], [str(cmake),'--build',str(build),'--parallel','4']]
    commands[0].extend(json.loads(os.environ.get('DODEPAN_CMAKE_ARGS','[]')))
    status = {'commands': commands, 'sdk_commit': subprocess.check_output(['git','-C',str(OUT/'pico-sdk'),'rev-parse','HEAD'],text=True).strip(), 'compiler': subprocess.check_output([str(compiler/'arm-none-eabi-gcc.exe'),'--version'],text=True).splitlines()[0]}
    for stage,command in zip(['configure','build'], commands):
        print(stage,flush=True)
        with (DOC/(stage+'.log')).open('w',encoding='utf8') as log:
            p = subprocess.Popen(command,env=env,stdout=log,stderr=subprocess.STDOUT,cwd=ROOT)
            code=p.wait()
        status[stage+'_exit_code']=code
        print(stage,'exit',code,flush=True)
        if code: break
    if status.get('build_exit_code') == 0:
        status['size'] = subprocess.check_output([str(compiler/'arm-none-eabi-size.exe'),str(build/'Dodepan_RP2350.elf')],text=True)
        status['artifacts']={}
        for ext in ['elf','uf2','bin']:
            f=build/('Dodepan_RP2350.'+ext)
            if f.exists(): status['artifacts'][ext]={'path':f.relative_to(ROOT).as_posix(),'bytes':f.stat().st_size,'sha256':hashlib.sha256(f.read_bytes()).hexdigest()}
        print(status['size'],flush=True)
        sections=subprocess.check_output([str(compiler/'arm-none-eabi-size.exe'),'-A',str(build/'Dodepan_RP2350.elf')],text=True)
        (DOC/'sections.log').write_text(sections,encoding='utf8')
        rows=[line.split() for line in sections.splitlines() if line.startswith('.')]
        status['static_sram_bytes_excluding_heap_stack']=sum(int(r[1]) for r in rows if r[0] in ['.ram_vector_table','.uninitialized_data','.data','.tdata','.tbss','.bss','.scratch_x','.scratch_y'])
        status['minimum_reserved_heap_and_stacks_bytes']=sum(int(r[1]) for r in rows if r[0] in ['.heap','.stack1_dummy','.stack_dummy'])
    (DOC/'build-result.json').write_text(json.dumps(status,indent=2)+'\n',encoding='utf8')
    sys.exit(status.get('build_exit_code',status.get('configure_exit_code',1)))
if __name__ == '__main__': main()
