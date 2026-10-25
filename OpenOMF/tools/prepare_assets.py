#!/usr/bin/env python3
"""OMF2097 AF/BK/SOUNDS asset conversion, based on OpenOMF MIT format readers.
Original game data keeps its original terms; converter/runtime GPL-3.0-or-later.
"""
from pathlib import Path
import struct,re,json,wave,subprocess,shutil,os
from PIL import Image
P=Path(__file__).resolve().parents[1];U=P/'vendor/upstream';D=next((P/'vendor/game-data').rglob('FIGHTR0.AF')).parent
O=P/'sdcard/aster/apps/aster.openomf/data/resources';O.mkdir(parents=True,exist_ok=True);PRE=P/'artifacts/previews';PRE.mkdir(parents=True,exist_ok=True)
class Reader:
 def __init__(self,b):self.b=b;self.p=0
 def take(self,n):
  a=self.b[self.p:self.p+n];self.p+=n
  if len(a)!=n:raise ValueError(('truncated',self.p,n))
  return a
 def get(self,fmt):n=struct.calcsize('<'+fmt);return struct.unpack('<'+fmt,self.take(n))
 def one(self,fmt):return self.get(fmt)[0]
 def string(self,terminated=True):
  n=self.one('H');raw=self.take(n+int(terminated));return raw[:n].split(b'\0')[0].decode('latin1')
 def anim(self):
  x,y,zero,nc,ns=self.get('hhIHB');coords=[]
  for _ in range(nc):
   a,b=self.get('HH');coords.append((((a&1023)^512)-512,((b&1023)^512)-512,b>>10,0))
  text=self.string();extra=[self.string() for _ in range(self.one('B'))];sprites=[]
  for _ in range(ns):
   n,sx,sy,w,h,idx,missing=self.get('HhhHHBB');raw=self.take(n) if not missing else None
   sprites.append(dict(x=sx,y=sy,w=w,h=h,index=idx,raw=raw,missing=missing))
  return dict(x=x,y=y,text=text,extra=extra,sprites=sprites,coords=coords)
def decode_sprite(s,cache):
 raw=s['raw']
 if raw is None:raw=cache.get(s['index'],b'')
 else:cache[s['index']]=raw
 w,h=s['w'],s['h'];
 if w>512 or h>512:
  if raw:raise ValueError(('sprite dimensions out of range',w,h))
  s['w']=s['h']=0;return b''
 out=bytearray(w*h);x=y=i=0
 while i+1<len(raw):
  c=struct.unpack_from('<H',raw,i)[0];i+=2;op=c%4;n=c//4
  if op==0:x=n
  elif op==2:y=n
  elif op==1:
   for j in range(n):
    if i>=len(raw):break
    if 0<=x<w and 0<=y<h:out[y*w+x]=raw[i]
    i+=1;x+=1
   x=0
  else:break
 return bytes(out)
tags=dict((k,int(v)) for k,v in re.findall(r'\[TAG_\w+\]\s*=\s*\{\s*"([^"\s]+)"\s*,\s*([01])', (U/'src/formats/tag_list.c').read_text()))
def parse_frames(text):
 frames=[];i=0;ts={}
 while i<len(text):
  if 'A'<=text[i]<='Z':
   sid=ord(text[i])-65;i+=1;m=re.match(r'[+-]?\d+',text[i:]);duration=int(m[0]) if m else 1;i+=len(m[0]) if m else 0
   flags=(1 if 'r' in ts else 0)|(2 if 'f' in ts else 0)|(4 if 'n' in ts else 0)|(8 if 'd' in ts else 0)|(16 if 'v' in ts else 0)|(32 if 'h' in ts else 0)|(64 if 'g' in ts else 0)|(128 if 'zz' in ts else 0)|(256 if 'bn' in ts else 0)|(512 if 'cp' in ts else 0)
   fields=[sid,max(1,duration),ts.get('ox',0),ts.get('oy',0),(ts.get('x+',0)-ts.get('x-',0)) if 'x+' in ts or 'x-' in ts else 32767,(ts.get('y+',0)-ts.get('y-',0)) if 'y+' in ts or 'y-' in ts else 32767,ts.get('m',-1),ts.get('mx',0),ts.get('my',0),ts.get('s',-1),flags,ts.get('q',0),ts.get('k',0),ts.get('bj',-1),ts.get('d',0),0,sum(1<<j for j,k in enumerate(('jg','jl','jm','jh','jj','jp','jz')) if k in ts),ts.get('jn',-1)]
   frames.append(fields);ts={}
   if i<len(text) and text[i]=='-':i+=1
  else:
   match=False
   for length in (3,2,1):
    key=text[i:i+length]
    if key not in tags:continue
    j=i+length;m=re.match(r'[+-]?\d+',text[j:])
    if not tags[key] and m:continue
    value=int(m[0]) if tags[key] and m else 0
    i=j+(len(m[0]) if tags[key] and m else 0);ts[key]=value;match=True;break
   if not match:i+=1
 return frames
names=['Jaguar','Shadow','Thorn','Pyros','Electra','Katana','Shredder','Flail','Gargoyle','Chronos','Nova']
meta=[]
for fi in range(11):
 r=Reader((D/f'FIGHTR{fi}.AF').read_bytes());fid,window,endurance,jlim,health,fw,bw,jump,fall,ver,ai=r.get('HHIBHiiiiBB');moves={};cache={}
 while True:
  mid=r.one('B')
  if mid>=70:break
  a=r.anim();head=r.get('HH17B');inp=r.take(21).split(b'\0')[0].decode('latin1');footer=r.string(False);a.update(head=head,input=inp,footer=footer);moves[mid]=a
 sound=r.take(30);pixels=bytearray();pixel_offsets={};sprites=[];frames=[];coords=[];packedmoves=[];info=[]
 for mid in range(70):
  a=moves.get(mid);sf=len(sprites);ff=len(frames);cf=len(coords)
  if not a:packedmoves.append(bytes(64));continue
  for s in a['sprites']:
   decoded=decode_sprite(s,cache);raw=s['raw'] if s['raw'] is not None else cache.get(s['index'],b'');raw=raw if s['w'] and s['h'] else b'';
   if raw not in pixel_offsets:pixel_offsets[raw]=len(pixels);pixels.extend(raw)
   off=pixel_offsets[raw];sprites.append(struct.pack('<hhHHII',s['x'],s['y'],s['w'],s['h'],off,len(raw)))
  parsed=parse_frames(a['text']);frames.extend(parsed);coords.extend(a['coords']);h=a['head']
  # head entries: two u16, unknown4..11, play_if_hit,category,block_damage,block_stun,successor,damage,throw_duration,extra_selector,points
  damage,category,successor,playhit=h[15],h[11],h[14],h[10]
  packedmoves.append(struct.pack('<6I4h24s4h',sf,len(a['sprites']),ff,len(parsed),cf,len(a['coords']),damage,category,successor,playhit,a['input'].encode()[:23],h[1],h[12],h[13],h[16]))
  info.append(dict(id=mid,input=a['input'],category=category,damage=damage,frames=len(parsed),sprites=len(a['sprites']),script=a['text'],extra=a['extra']))
 header=bytearray(64);blob=bytearray(header);offsets=[]
 for part in (b''.join(packedmoves),b''.join(sprites),b''.join(struct.pack('<18h',*f) for f in frames),b''.join(struct.pack('<4h',*c) for c in coords),bytes(pixels)):
  blob+=bytes((-len(blob))%4);offsets.append(len(blob));blob+=part
 # fixed 64-byte header, little endian; extra 30-byte table is in generated C catalog.
 struct.pack_into('<4sII4f8I',blob,0,b'OMF2',len(blob),health,fw/256,bw/256,jump/256,fall/256,*offsets,len(sprites),len(frames),len(coords))
 (O/f'fighter{fi}.bin').write_bytes(blob)
 meta.append(dict(id=fi,name=names[fi],bytes=len(blob),health=health,forward=fw/256,reverse=bw/256,jump=jump/256,fall=fall/256,moves=info,sounds=list(sound)))
 print(names[fi],len(blob),'bytes',len(sprites),'sprites',len(frames),'frames')
for arena in range(5):
 r=Reader((D/f'ARENA{arena}.BK').read_bytes());id,unk,w,h=r.get('IBHH');cache={};animations=[]
 while True:
  nxt=r.one('I');aid=r.one('B')
  if aid>=50:break
  spec=r.get('4BHB');footer=r.string(False);anim=r.anim();anim.update(id=aid,spec=spec,footer=footer);animations.append(anim)
 bg=r.take(w*h);n=r.one('B');pals=[]
 for j in range(n):
  rgb=r.take(768);pal=[tuple((v<<2)|((v&48)>>4) for v in rgb[k:k+3]) for k in range(0,768,3)];pals.append(pal);r.take(19*256)
 pal=pals[0];p565=[((cr>>3)<<11)|((cg>>2)<<5)|(cb>>3) for cr,cg,cb in pal];data=struct.pack('<4sI256H',b'OMB1',w*h,*p565)+bg;(O/f'arena{arena}.bin').write_bytes(data)
 image=Image.new('P',(w,h));image.putpalette([c for rgb in pal for c in rgb]);image.putdata(bg);image.convert('RGB').resize((640,400)).save(PRE/f'arena{arena}.png')
 print('arena',arena,'animations',len(animations),'palettes',n)
# All original PCM effects, preserving full samples when the ABI budget permits.
r=Reader((D/'SOUNDS.DAT').read_bytes());zero,header=r.get('II');count=header//4-2;r.take(count*4);soundmeta=[]
for sid in range(count+1):
 n=r.one('H')
 if not n:continue
 key=r.one('B');raw=r.take(n);rate=round(1000000/(256-key));outRate=max(8000,min(48000,rate));count2=min(32768,round(n*outRate/rate));samples=[]
 for j in range(count2):
  at=min(n-1,int(j*rate/outRate));samples.append((raw[at]-128)*190)
 with wave.open(str(O/f'sound{sid}.wav'),'wb') as w:w.setnchannels(1);w.setsampwidth(2);w.setframerate(outRate);w.writeframes(struct.pack('<%dh'%len(samples),*samples))
 soundmeta.append(dict(id=sid,frames=count2,rate=outRate,truncated=count2<round(n*outRate/rate)))
(O/'catalog.json').write_text(json.dumps(dict(fighters=meta,sounds=soundmeta),indent=2),encoding='utf8')
header=['/* Generated from original OMF2097 resources. */','#pragma once','static const char *fighter_names[11]={'+','.join(json.dumps(x) for x in names)+'};','static const int8_t sound_tables[11][30]={']
for m in meta:header.append('{'+','.join(str(x if x<128 else x-256) for x in m['sounds'])+'},')
header+=['};']
alt=(D/'ALTPALS.DAT').read_bytes()[:768]
header.append('static const uint16_t player_colors[2][48]={')
for ramps in ((5,11,8),(10,15,7)):
 rgb=[]
 for ramp in ramps:
  for i in range(16):
   at=(ramp*16+i)*3;cr,cg,cb=[(v<<2)|((v&48)>>4) for v in alt[at:at+3]];rgb.append(((cr>>3)<<11)|((cg>>2)<<5)|(cb>>3))
 header.append('{'+','.join(str(v) for v in rgb)+'},')
header+=['};',f'#define FIGHTER_CAP {max(m["bytes"] for m in meta)}'];(P/'src/catalog.h').write_text('\n'.join(header)+'\n',encoding='utf8')
print('Converted',len(soundmeta),'original PCM effects')




# Decode original tracker music offline, keeping complete tracks.
ffmpeg=os.environ.get('FFMPEG') or shutil.which('ffmpeg')
for aid in range(5):
 src=D/f'ARENA{aid}.PSM';dst=O/f'arena{aid}.mp3'
 if dst.exists():continue
 params=['-hide_banner','-loglevel','error','-y','-i',str(src),'-af','volume=0.5','-c:a','libmp3lame','-b:a','96k','-ar','44100','-ac','2',str(dst)]
 if ffmpeg:cmd=[ffmpeg]+params
 elif os.name=='nt':
  def linux(path):
   t=str(path.resolve()).replace(chr(92),'/');return '/mnt/'+t[0].lower()+t[2:]
  params[params.index('-i')+1]=linux(src);params[-1]=linux(dst);cmd=['wsl','-d','Ubuntu-22.04','--exec','ffmpeg']+params
 else:raise SystemExit('ffmpeg required')
 subprocess.run(cmd,check=True)
 print('Music',dst.name,dst.stat().st_size)
