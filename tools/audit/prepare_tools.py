"""Prepare audit dependencies under ignored build-audit; never install globally.
Requires Python 3.9+, Git, and Visual Studio C++ Build Tools on Windows.
Existing source checkouts must already match their pins; this script will not
reset or overwrite another checkout. Downloads need network access.
"""
import hashlib,pathlib,subprocess,urllib.request,zipfile
ROOT=pathlib.Path(__file__).resolve().parents[2]
OUT=ROOT/'build-audit'
REPOS=[
 ('pico-sdk','https://github.com/raspberrypi/pico-sdk.git','2.2.0','a1438dff1d38bd9c65dbd693f0e5db4b9ae91779'),
 ('upstream-v1.6.0','https://github.com/risgk/digital-synth-pra32-u2.git','v1.6.0','60040e7a41f391bf60805afaecfa503a45954414'),
 ('upstream-v2.18.2','https://github.com/risgk/digital-synth-pra32-u2.git','v2.18.2','ad082d08d825e979023f1dc7cae9f6a0dab45af5'),
]
URL='https://armkeil.blob.core.windows.net/developer/Files/downloads/gnu/14.2.rel1/binrel/arm-gnu-toolchain-14.2.rel1-mingw-w64-x86_64-arm-none-eabi.zip'
SHA256='f074615953f76036e9a51b87f6577fdb4ed8e77d3322a6f68214e92e7859888f'
def digest(path):
 h=hashlib.sha256()
 with path.open('rb') as f:
  for part in iter(lambda:f.read(1024*1024),b''):h.update(part)
 return h.hexdigest()
def main():
 OUT.mkdir(exist_ok=True)
 for name,url,tag,pin in REPOS:
  dest=OUT/name
  if not dest.exists():subprocess.run(['git','clone','--depth','1','--branch',tag,url,str(dest)],check=True)
  actual=subprocess.check_output(['git','-C',str(dest),'rev-parse','HEAD'],text=True).strip()
  if actual!=pin:raise RuntimeError(f'{dest} is at {actual}, expected {pin}; left unchanged')
  print(name,pin,flush=True)
 subprocess.run(['git','-C',str(OUT/'pico-sdk'),'submodule','update','--init','--depth','1','lib/tinyusb'],check=True)
 tools=OUT/'tools';tools.mkdir(exist_ok=True);archive=tools/'arm-gnu-14.2.rel1.zip'
 if not archive.exists():
  partial=archive.with_suffix('.download');urllib.request.urlretrieve(URL,partial)
  if digest(partial)!=SHA256:raise RuntimeError('Compiler archive hash mismatch; not extracted')
  partial.replace(archive)
 if digest(archive)!=SHA256:raise RuntimeError('Compiler archive hash mismatch; not extracted')
 if not (tools/'bin/arm-none-eabi-gcc.exe').exists():
  with zipfile.ZipFile(archive) as z:
   for n in z.namelist():
    if not (tools/n).resolve().is_relative_to(tools.resolve()):raise RuntimeError('Unsafe archive entry')
   z.extractall(tools)
 print('Arm 14.2.Rel1 archive verified:',SHA256)
if __name__=='__main__':main()
