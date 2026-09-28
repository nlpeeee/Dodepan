"""Make sparse demos from the vendored online Mutopia MIDI sources."""
from pathlib import Path
import sys,re,json,hashlib
from collections import defaultdict
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools'))
from midi_to_c import parse_midi_file,MidiEvent,create_merged_midi,bytes_to_c_array
def _c_ident(s):
 s=re.sub(r'[^A-Za-z0-9_]','_',s);return s if (s[:1].isalpha() or s[:1]=='_') else '_'+s
OUT=ROOT/'build-audit/demo-classics';OUT.mkdir(parents=True,exist_ok=True)
specs=[('Bach Prelude C','wtk1-prelude1',72,[1,2],.45),('Fur Elise','fur_Elise_WoO59',72,[1,2],.65),('Greensleeves','GreensleevesAcc',132,[1,2],1.6),('Canon in D','CanonInD',88,[1],1.8)]
header=['// Derived from Mutopia sources; see tools/demo-classics/SOURCES.md.','#pragma once','#include <stdint.h>'];rows=[];reports=[]
for name,stem,bpm,chosen,cap in specs:
 path=Path(__file__).parent/'sources'/(stem+'.mid');fmt,ppq,tracks=parse_midi_file(str(path));notes=[]
 for ti in chosen:
  held={}
  for e in tracks[ti]:
   d=e.data;k=d[0]&240
   if k not in (128,144):continue
   key=(d[0]&15,d[1])
   if k==144 and d[2]:held[key]=(e.abs_time,d[2])
   elif key in held:
    start,vel=held.pop(key)
    if stem=='fur_Elise_WoO59' and start>=12*ppq:continue
    end=min(e.abs_time,start+round(cap*ppq))
    # Leave a short articulation gap; cap held basses to free release-tail voices.
    end=start+max(1,round((end-start)*.90))
    if stem=='GreensleevesAcc' and ti==2:
     # Keep only the root/bass on dotted-quarter pulses, omitting accordion chords.
     if (start-ppq)%(3*ppq):continue
    notes.append([start,end,d[1],68 if ti==chosen[0] else 52])
  assert not held
 if stem=='fur_Elise_WoO59':notes += [[s+13*ppq,e+13*ppq,n,v] for s,e,n,v in notes[:]]
 # At shared onsets keep outer voices, never duplicate a pitch on the mono channel.
 groups=defaultdict(list)
 for note in notes:groups[note[0]].append(note)
 arranged=[]
 for start,group in sorted(groups.items()):
  unique={x[2]:x for x in sorted(group,key=lambda x:x[3])};group=sorted(unique.values(),key=lambda x:x[2])
  if len(group)>2:group=[group[0],group[-1]]
  if stem=='GreensleevesAcc':
   # Accompaniment pitches stay below the melody; retain only the lowest bass tone.
   bass=[x for x in group if x[3]==52];melody=[x for x in group if x[3]!=52];group=bass[:1]+melody
  active=[x for x in arranged if x[1]>start]
  for old in active[:]:
   if any(x[2]==old[2] for x in group):old[1]=start;active.remove(old)
  while len(active)+len(group)>2:
   old=min(active,key=lambda x:(x[1],x[3]));old[1]=start;active.remove(old)
  arranged.extend(group)
 events=[]
 for s,e,n,v in arranged:
  assert e>s
  events.extend([(s,1,n,v),(e,0,n,0)])
 events.sort();active=set();peak=0
 for t,on,n,v in events:
  if on:assert n not in active;active.add(n);peak=max(peak,len(active))
  else:assert n in active;active.remove(n)
 assert not active and peak<=2
 tempo=round(60000000/bpm);end=events[-1][0]+round(2000000*ppq/tempo)
 midi_events=[MidiEvent(0,b'\xff\x51\x03'+tempo.to_bytes(3,'big'))]
 midi_events += [MidiEvent(t,bytes([144 if on else 128,n,v])) for t,on,n,v in events]
 midi_events.append(MidiEvent(end,b'\xff\x2f\x00'));data=create_merged_midi(midi_events,ppq)
 ident='classic_'+_c_ident(stem);(OUT/(name+'.mid')).write_bytes(data);header.append(bytes_to_c_array(ident,data));rows.append('    {"'+name+'",'+ident+'_data,sizeof('+ident+'_data)},')
 reports.append(dict(name=name,source_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),notes=len(arranged),max_held=peak,seconds=round(end*tempo/ppq/1e6,2)))
(ROOT/'demo_classics.h').write_text('\n'.join(header)+'\n')
p=ROOT/'demo_songs.h';s=p.read_text();s=re.sub(r'    /\* (?:Original|Classic) demos begin \*/.*?    /\* (?:Original|Classic) demos end \*/\n','',s,flags=re.S);s=s.replace('#include "demo_originals.h"\n','').replace('#include "demo_classics.h"\n','');s=s.replace('static const demo_song_t demo_songs','#include "demo_classics.h"\nstatic const demo_song_t demo_songs');a=s.index('static const demo_song_t demo_songs');end=s.index('\n};',a);s=s[:end]+'\n    /* Classic demos begin */\n'+'\n'.join(rows)+'\n    /* Classic demos end */'+s[end:];s=re.sub(r'#define DEMO_(MAX_SONGS|SONG_COUNT) \d+',lambda m:'#define DEMO_'+m[1]+' 13',s);p.write_text(s)
(OUT/'report.json').write_text(json.dumps(reports,indent=2)+'\n');print(json.dumps(reports))
