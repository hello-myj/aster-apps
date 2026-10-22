#!/usr/bin/env python3
"""OPK1 version 2: 4 MiB SD chunks, resource index only in first chunk."""
from pathlib import Path
import struct,json,zlib
P=Path(__file__).resolve().parents[1];root=P/'sdcard/aster/apps/aster.outrun/data/resources'
ni=len(json.loads((root/'catalog.json').read_text())['images']);nm=16;limit=4*1024*1024
files=[root/f'img/{i:04}.r16' for i in range(ni)]+[root/f'map{i:02}.bin' for i in range(nm)]+[root/'font.r16']
chunks=[bytearray(32+(ni+nm)*8)];entries=[]
for f in files:
 data=f.read_bytes();n=len(data);assert n<=limit
 if len(chunks[-1])+n+3>limit:chunks.append(bytearray())
 offset=len(chunks[-1]);entries.append(((len(chunks)-1)<<24|offset,n));chunks[-1].extend(data);chunks[-1].extend(bytes((-n)%4))
total=sum(map(len,chunks));struct.pack_into('<8I',chunks[0],0,0x314b504f,2,ni,nm,*entries[-1],total,len(chunks))
for i,e in enumerate(entries[:-1]):struct.pack_into('<2I',chunks[0],32+i*8,*e)
metadata=[]
for i,data in enumerate(chunks):
 name='resources'+(f'{i:02}' if i else '')+'.pak';out=P/'dist'/name;out.write_bytes(data);metadata.append({'name':name,'bytes':len(data),'crc32':f'{zlib.crc32(data):08x}'})
(P/'dist/resources.json').write_text(json.dumps({'format':'OPK1v2','bytes':total,'images':ni,'maps':nm,'chunks':metadata},indent=2))
print(json.dumps(metadata,indent=2))
