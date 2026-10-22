#!/usr/bin/env python3
"""Port source geometry/placements, not a resampled approximation. GPL-3.0-or-later."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import numpy as np
import struct, math, re, json, subprocess, shutil
P=Path(__file__).resolve().parents[1]; U=P/'vendor'/'upstream'; R=U/'Resources'
OUT=P/'sdcard/aster/apps/aster.outrun/data/resources'; OUT.mkdir(parents=True,exist_ok=True)
INDEX=[]; ids={}
def image(src,key,cap=(320,320)):
    if key in ids:return ids[key]
    im=Image.open(src).convert('RGBA'); ow,oh=im.size
    im.thumbnail(cap,Image.Resampling.NEAREST)
    a=np.array(im,dtype=np.uint16); px=((a[:,:,0]>>3)<<11)|((a[:,:,1]>>2)<<5)|(a[:,:,2]>>3)
    px=np.where(a[:,:,3]==0,0,np.maximum(px,1)).astype('<u2')
    name=f'img/{len(INDEX):04}.r16'; dest=OUT/name; dest.parent.mkdir(exist_ok=True)
    partial=bool(np.any((a[:,:,3]>0)&(a[:,:,3]<255)))
    dest.write_bytes(struct.pack('<4H',im.width|(0x8000 if partial else 0),im.height,ow,oh)+px.tobytes()+(a[:,:,3].astype('uint8').tobytes() if partial else b''))
    ids[key]=len(INDEX); INDEX.append({'file':name,'source':str(src.relative_to(U)), 'w':ow,'h':oh,'decode':im.size})
    return ids[key]
def props(src,w):
    p={'SCALE':[1.], 'WIDTH_COLLISION':[w], 'COLLISION':[1], 'PIVOT_LEFT':[1.,1.], 'PIVOT_RIGHT':[0.,1.], 'PIVOT_COL_LEFT':[.5,1.], 'PIVOT_COL_RIGHT':[.5,1.], 'SHOW_TERRAIN':[0]}
    if src.exists():
        for line in src.read_text().splitlines():
            if ':' in line:
                k,v=line.split(':',1)
                try:p[k]=[float(x) for x in v.split()]
                except ValueError:pass
    return p
def ease(a,b,t):return a+(b-a)*(-math.cos(t*math.pi)*.5+.5)
step=(1600*16//27)+(1600//18)
D={n:(n-3)*step for n in range(3,9)};D[2]=D[8]+(1600*16//27)*20+(1600//18)*7
def add(lines,e,h,l,c,y,mirror,dist):
    total=e+h+l
    if not total:return
    first=lines[-1][1] if lines else 0.; old=lines[-1][3] if lines else float(dist)
    delta=int(((D[2] if mirror else dist)-old)/total); end=first+y*150
    for phase,num in enumerate((e,h,l)):
        for n in range(num):
            idx=n+(0 if phase==0 else e if phase==1 else e+h)
            cv=c*(n/e)**2 if phase==0 else c if phase==1 else ease(c,0,n/l)
            old+=delta
            yy=ease(first,end,idx/total); y0=lines[-1][1] if lines else 0.
            lines.append((y0,yy,cv,old,int(mirror)))
def emit_map(mid):
    folder=R/'Maps/MapLevels'/f'Map{mid}'
    text=(folder/'map.txt').read_text(); lines=[]; colors=[]; metadata=[]; placements={}; checkpoint=0
    for line in text.split('START_RELIEF')[0].splitlines():
        if line.startswith('COLOR_'):
            rgb=[int(x) for x in line.split(':')[1].split()]
            colors.append(((rgb[0]>>3)<<11)|((rgb[1]>>2)<<5)|(rgb[2]>>3))
    for line in text.split('START_RELIEF')[1].split('SPRITES')[0].splitlines():
        if ':' not in line:continue
        typ,val=line.split(':',1); v=[float(x) for x in val.split()]
        if not v:continue
        e,h,l=map(int,v[:3]); c=y=0.; factor=1
        if typ=='STRAIGHT':mirror,lanes=map(int,v[3:5])
        elif typ.startswith('CURVE_'):
            c,mirror,lanes=v[3:6];factor=int(v[6]) if len(v)>6 else 1
        elif typ=='HILL_STRAIGHT':
            y,lanes=v[3:5];mirror=0;factor=int(v[5]) if len(v)>5 else 1
        else:
            y,c,mirror,lanes=v[3:7]
            l=e # Preserve upstream checkReliefHillCurve's literal addBiome(enter,hold,enter,...).
        add(lines,e,h*factor,l,c,y,int(mirror),D[int(lanes)])
    body_count=len(lines)
    # Same fork sequence as Map::setMapDistanceAndTrackLength ORIGINAL_MODE (flat deterministic gradient).
    for args in [(10,300,50,-2,0,1,D[3]),(100,100,100,0,0,1,D[2]),(100,100,100,0,0,0,D[5]),(100,150,150,0,0,0,D[3]),(100,100,100,0,0,0,D[3]),(100,100,100,0,0,0,D[3])]:add(lines,*args)
    sources=([(R/'Maps/MapStartGoal'/f'{i}.png') for i in range(1,46)] if mid==1 else [])+sorted([x for x in folder.glob('*.png') if x.stem.isdigit()],key=lambda p:int(p.stem))
    for src in sources:
        aid=image(src,str(src)); im=Image.open(src); pr=props(src.with_suffix('.txt'),im.width)
        metadata.append((aid,pr))
    def put(ln,slot,obj,x,y,side):
        if 0<=ln<len(lines) and 0<=obj<len(metadata):placements[ln,slot]=(ln,obj,float(x),float(y),int(side))
    # Original static start positions and crowd, extracted from Logger::loadStartBiomeSprites.
    if mid==1:
        logger=(U/'src/Logger/Logger.cpp').read_text()
        start=logger.split('void Logger::loadStartBiomeSprites')[1].split('void Logger::loadGoalBiomeSprites')[0]
        variables={}
        for m in re.finditer(r'SpriteInfo\*\s*(\w+)\s*=\s*new SpriteInfo\(&m.objects\[(\d+)\](.*?);',start,re.S):
            tail=m[3].rsplit('],',1)[-1].strip().rstrip(')')
            vals=re.findall(r'(-?\d+(?:\.\d+)?)(?:f)?',tail)
            if len(vals)>=2:variables[m[1]]=(int(m[2]),float(vals[0]),float(vals[1]),0)
        variables.update({'startFlag':(37,-1.33,-1.6,0),'trafficLightLeftPanel':(38,-.8,0,0),'trafficLightRightPanel':(42,.8,0,0)})
        slots={'FAR_LEFT':0,'NEAR_LEFT':1,'CENTER':2,'FAR_RIGHT':3,'NEAR_RIGHT':4}
        for m in re.finditer(r'm.addSpriteInfo\((\d+),\s*(\w+),\s*Sprite_Position::(\w+)',start):
            if m[2] in variables:put(int(m[1]),slots[m[3]],*variables[m[2]])
        # Original road-side start palms appear every 50 segments.
        for i in range(0,301,50):
            put(i,0,35,-.8,0,0);put(i,3,36,.8,0,1)
    slotmap={'FAR_LEFT':0,'NEAR_LEFT':1,'CENTER':2,'FAR_RIGHT':3,'NEAR_RIGHT':4}
    active=[]
    for line in text.split('SPRITES',1)[1].splitlines():
        if ':' not in line:continue
        typ,val=line.split(':',1);v=val.split()
        if typ=='GROUP_LINES':
            st,en,inc,freq=map(int,v);active=[i for i in range(st,en,inc) if i%freq==0]
        elif typ in ('LINE','LINE_CHECKPOINT'):
            active=[int(v[0])]
            if typ=='LINE_CHECKPOINT':checkpoint=int(v[0])
        elif typ.startswith('SPRITE_'):
            ob=int(v[0])-1;x,y=float(v[1]),float(v[2]);side=int(v[3])
            for i in active:put(i,slotmap[typ[7:]],ob,x,y,side)
    objects=[placements[k] for k in sorted(placements)]
    dest=OUT/f'map{mid:02}.bin'
    tm=int(re.search(r'TIME:\s*(\d+)',text)[1])
    with dest.open('wb') as f:
        f.write(struct.pack('<10I',0x3252544f,len(lines),len(metadata),len(objects),tm,body_count,checkpoint,mid,int(re.search(r'(?m)^TERRAIN:\s*(\d+)',text)[1]),int(re.search(r'(?m)^ROAD_TERRAIN:\s*(\d+)',text)[1])))
        f.write(struct.pack('<12H',*(colors+[0])[:12]))
        for y0,y1,c,d,mir in lines:f.write(struct.pack('<4fI',y0,y1,c,d,mir))
        for aid,p in metadata:
            f.write(struct.pack('<I12f',aid,p['SCALE'][0],p['WIDTH_COLLISION'][0],p['COLLISION'][0],*p['PIVOT_LEFT'],*p['PIVOT_RIGHT'],*p['PIVOT_COL_LEFT'],*p['PIVOT_COL_RIGHT'],p['SHOW_TERRAIN'][0]))
        for ln,obj,x,y,side in objects:f.write(struct.pack('<IIffI',ln,obj,x,y,side))
    bg=[]
    for name in ('back','front'):bg.append(image(folder/f'{name}.png',str(folder/f'{name}.png'),(1200,128)))
    return {'map':mid,'lines':len(lines),'objects':len(objects),'textures':len(metadata),'background':bg,'body':body_count}
maps=[emit_map(i) for i in range(1,16)]
# Original Biome::setGoalBiome straight relief and Logger goal-side crowd/flags.
goal_lines=[];add(goal_lines,350,350,350,0,0,False,D[3])
raw=(OUT/'map01.bin').read_bytes();count=struct.unpack_from('<I',raw,4)[0]
goal_meta=raw[40+24+count*20:40+24+count*20+45*52]
goal_obj={}
def gput(ln,slot,obj,x,y=0):goal_obj[ln,slot]=(ln,obj,x,y,0)
for ln in range(250,669,3):gput(ln,0,33,-1.31);gput(ln,3,34,1.31)
for ln in range(270,669):
    if ln%52==0:gput(ln,1,24,-.5);gput(ln,4,25,.5)
    elif ln%60==0:gput(ln,1,14,-.69)
    elif ln%55==0:gput(ln,4,30,.6)
    elif ln%70==0:gput(ln,1,26,-.6);gput(ln,4,17,.65)
    elif ln%80==0:gput(ln,1,19,-.66)
    elif ln%90==0:gput(ln,1,20,-.61);gput(ln,4,21,.7)
for ln in range(668,682):o=1.31-(ln-668)*.1;gput(ln,0,33,-o);gput(ln,3,34,o)
gput(504,1,0,-.32)
for ln in (505,682):gput(ln,1,44,-1.2);gput(ln,4,44,1.2);gput(ln+1,0,43,-1.35,-1.6)
goal_colors=[(0,207,205),(255,235,221),(251,229,215),(156,156,156),(148,148,148),(156,156,156),(247,247,247),(156,156,156),(247,247,247),(247,247,247),(148,148,148),(0,0,0)]
with (OUT/'map00.bin').open('wb') as f:
    f.write(struct.pack('<10I',0x3252544f,len(goal_lines),45,len(goal_obj),80,1050,504,0,1,0))
    f.write(struct.pack('<12H',*(((r>>3)<<11)|((g>>2)<<5)|(b>>3) for r,g,b in goal_colors)))
    for row in goal_lines:f.write(struct.pack('<4fI',*row))
    f.write(goal_meta)
    for key in sorted(goal_obj):f.write(struct.pack('<IIffI',*goal_obj[key]))
goal_bg=[image(R/f'Maps/MapStartGoal/{n}.png',f'goal-{n}',(1200,128)) for n in ('back','front')]
hud={f.stem:image(f,str(f),(256,128)) for f in (R/'Huds/HudRound').glob('*.png')}
logo=image(R/'Menus/MainMenu/LogoMain1.png','logo',(320,180))
wheels=[image(R/f'Vehicles/PlayerCar/Wheels/c{i}.png',f'wheel{i}',(96,96)) for i in range(1,29)]
player=[image(R/f'Vehicles/PlayerCar/Ferraris/Ferrari1/c{i}.png',f'player{i}',(192,128)) for i in range(1,149)]
traffic=[]
for model in range(1,18):
    fol=R/f'Vehicles/TrafficCars/Car{model}'
    if fol.exists():traffic.append([image(fol/f'c{i}.png',f'traffic{model}-{i}',(160,128)) for i in range(1,17) if (fol/f'c{i}.png').exists()])
# Source font and HUD image resources remain available for visual fidelity.
font=ImageFont.truetype(str(R/'Fonts/DisposableDroid.ttf'),16)
atlas=Image.new('RGBA',(16*16,16*6),(0,0,0,0));draw=ImageDraw.Draw(atlas)
for i in range(96):draw.text(((i%16)*16,(i//16)*16-1),chr(32+i),font=font,fill='white',stroke_width=0)
tmp=OUT/'font.png';atlas.save(tmp)
# font atlas header is normal r16.
a=np.array(atlas,dtype=np.uint16);px=np.where(a[:,:,3]>96,65535,0).astype('<u2')
(OUT/'font.r16').write_bytes(struct.pack('<4H',256,96,256,96)+px.tobytes())
audio={}
try:
    import imageio_ffmpeg
    ff=imageio_ffmpeg.get_ffmpeg_exe()
    ad=OUT/'audio';ad.mkdir(exist_ok=True)
    audio_sources={'music':R/'Soundtracks/Magical_Sound_Shower.ogg','crash':R/'SoundEffects/Ferrari_Crash.ogg','start':R/'SoundEffects/Countdown.ogg','engine':R/'SoundEffects/Ferrari_Engine_Run.ogg','brake':R/'SoundEffects/Ferrari_Engine_Brake.ogg'}
    for name,src in audio_sources.items():
        dst=ad/(name+('.mp3' if name=='music' else '.wav'))
        if not dst.exists():
            args=[ff,'-v','error','-y','-i',str(src),'-ar','22050','-ac','2']
            args+=['-codec:a','libmp3lame','-b:a','96k'] if name=='music' else ['-codec:a','pcm_s16le']
            subprocess.run(args+[str(dst)],check=True)
        audio[name]=str(dst.relative_to(OUT))
        if name!='music':
            for rate in (16000,22050,44100,48000):
                target=ad/f'{name}-{rate}.wav'
                if not target.exists():
                    subprocess.run([ff,'-v','error','-y','-i',str(src),'-t','0.315','-ar',str(rate),'-ac','2','-codec:a','pcm_s16le',str(target)],check=True)
# Canonical PCM WAV (fmt + data); omit ffmpeg LIST metadata for device compatibility.
    import wave
    for target in ad.glob('*-*.wav'):
        with wave.open(str(target),'rb') as w:params=w.getparams();pcm=w.readframes(w.getnframes())
        with wave.open(str(target),'wb') as w:w.setparams(params);w.writeframes(pcm)
except (ImportError,subprocess.CalledProcessError) as ex:print('Audio conversion unavailable:',ex)
manifest={'source':'ZgzInfinity/OutRun','maps':maps,'goal':{'map':0,'lines':1050,'objects':len(goal_obj),'background':goal_bg},'images':INDEX,'hud':hud,'logo':logo,'wheels':wheels,'player':player,'traffic':traffic,'audio':audio}
(OUT/'catalog.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf8')
# Small lookup generated at build time; geometry is external, only current map stays loaded.
header=P/'src/port/catalog.h'
header.write_text('#ifndef OUTRUN_CATALOG_H\n#define OUTRUN_CATALOG_H\n'+
'#define LOGO_ID '+str(logo)+'\n'+
''.join('#define HUD_'+k.upper()+' '+str(v)+'\n' for k,v in hud.items())+
'static const unsigned short goal_bg_ids[2]={'+','.join(map(str,goal_bg))+'};\n'+
'static const unsigned short wheel_ids[28]={'+','.join(map(str,wheels))+'};\n'+
'static const unsigned short player_ids[148]={'+','.join(map(str,player))+'};\n'+
'static const unsigned short bg_ids[15][2]={'+','.join('{'+','.join(map(str,x['background']))+'}' for x in maps)+'};\n'+
'static const unsigned short traffic_ids['+str(len(traffic))+'][16]={'+','.join('{'+','.join(map(str,(x+[x[0]]*16)[:16]))+'}' for x in traffic)+'};\n'+
'static const float traffic_scale[]={'+','.join(str(float((R/f'Vehicles/TrafficCars/Car{i+1}/Car{i+1}.txt').read_text().split(':')[1].strip())) if (R/f'Vehicles/TrafficCars/Car{i+1}/Car{i+1}.txt').exists() else '1.0' for i in range(len(traffic)))+'};\n'+
'static const unsigned short image_size[][2]={'+','.join('{'+str(x['w'])+','+str(x['h'])+'}' for x in INDEX)+'};\n'+
'#define IMAGE_COUNT '+str(len(INDEX))+'\n#define TRAFFIC_MODELS '+str(len(traffic))+'\n#endif\n',encoding='ascii')
print('Converted',len(maps),'full maps,',sum(x['objects'] for x in maps),'placements,',len(INDEX),'images; audio',list(audio))








