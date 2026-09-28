"""Validate a device capture, then create an unnormalised mono WAV."""
import argparse,json,struct,wave,zlib
from pathlib import Path

def decode(data):
    header,sep,pcm=data.partition(b'\n')
    if not sep or len(header)>1024: raise ValueError('Missing/oversized header')
    h=json.loads(header)
    if h.get('format')!='DODEPAN_PCM1' or h.get('rate')!=24000: raise ValueError('Unexpected format/rate')
    n=h.get('frames')
    if type(n)!=int or not 0<n<=32768 or len(pcm)!=n*2: raise ValueError('Truncated/oversized PCM')
    if f'{zlib.crc32(pcm):08x}'!=h.get('crc32'): raise ValueError('PCM checksum mismatch')
    return h,pcm

def main():
    parser=argparse.ArgumentParser();parser.add_argument('capture',type=Path);args=parser.parse_args()
    h,pcm=decode(args.capture.read_bytes())
    dest=args.capture.with_suffix('.wav')
    with wave.open(str(dest),'wb') as w:
        w.setnchannels(1);w.setsampwidth(2);w.setframerate(h['rate']);w.writeframes(pcm)
    args.capture.with_suffix('.json').write_text(json.dumps(h,indent=2)+'\n')
    peak=h.get('peak_render_us',0)*100/max(1,h.get('period_us',1))
    print(f"Saved {dest}; U{h.get('U')} L{h.get('L')} I{h.get('I')}, peak {peak:.1f}%")
if __name__=='__main__':main()
