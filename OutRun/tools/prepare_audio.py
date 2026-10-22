#!/usr/bin/env python3
"""Prepare complete PCM16 mono effects for the SDK 4.1 64-KiB data limit."""
from pathlib import Path
import array,hashlib,json,sys,wave
P=Path(__file__).resolve().parents[1]
ROOT=P/'sdcard/aster/apps/aster.outrun/data/resources/audio'

def prepare():
    metadata=[]
    for name,kind,rate in [('engine','loop',16000),('brake','loop',22050),('crash','once',22050),('start','once',22050)]:
        source=ROOT/f'{name}.wav'
        with wave.open(str(source),'rb') as w:
            if w.getsampwidth()!=2 or w.getcomptype()!='NONE':raise ValueError(f'{source}: PCM16 required')
            channels=w.getnchannels();source_rate=w.getframerate();samples=array.array('h',w.readframes(w.getnframes()))
        if sys.byteorder!='little':samples.byteswap()
        mono=[sum(samples[i:i+channels])//channels for i in range(0,len(samples),channels)]
        frames=round(len(mono)*rate/source_rate)
        if not 440<=frames<=32768:raise ValueError(f'{source}: {frames} frames do not fit continuous effect limit')
        output=array.array('h')
        for i in range(frames):
            pos=i*source_rate/rate;lo=min(int(pos),len(mono)-1);frac=pos-int(pos);hi=min(lo+1,len(mono)-1)
            output.append(max(-32768,min(32767,round(mono[lo]*(1-frac)+mono[hi]*frac))))
        if sys.byteorder!='little':output.byteswap()
        dest=ROOT/f'{name}-{kind}.wav'
        with wave.open(str(dest),'wb') as w:
            w.setnchannels(1);w.setsampwidth(2);w.setframerate(rate);w.writeframes(output.tobytes())
        entry=dict(file=dest.name,source=source.name,rate=rate,channels=1,frames=frames,data_bytes=frames*2,seconds=frames/rate,sha256=hashlib.sha256(dest.read_bytes()).hexdigest())
        metadata.append(entry);print(json.dumps(entry))
    (ROOT/'continuous-audio.json').write_text(json.dumps(metadata,indent=2),encoding='utf8')
if __name__=='__main__':prepare()
