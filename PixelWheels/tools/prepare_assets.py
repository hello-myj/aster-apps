#!/usr/bin/env python3
"""Convert original Pixel Wheels Aseprite/TMX assets. No hand-drawn substitute maps.
Original assets: Aurélien Gâteau et al., CC BY-SA 4.0. Converter: GPL-3.0-or-later.
"""
from pathlib import Path
import struct, zlib, base64, math, json, shutil, wave, xml.etree.ElementTree as ET
from PIL import Image, ImageDraw
P=Path(__file__).resolve().parents[1]
U=P/'vendor/upstream'; A=U/'android/assets'; S=U/'core/assets-src'
O=P/'sdcard/aster/apps/aster.pixelwheels/data/resources'; O.mkdir(parents=True,exist_ok=True)
PRE=P/'artifacts/assets'; PRE.mkdir(parents=True,exist_ok=True)

def ase(path):
    b=path.read_bytes(); _,magic,n,w,h,depth=struct.unpack_from('<I5H',b)
    if magic!=0xa5e0 or depth not in (8,16,32):raise ValueError(path)
    transparent=b[28]; pos=128; layers=[]; palette=[(0,0,0,255)]*256; old={}; frames=[]
    for fi in range(n):
        size,magic,chunks,duration=struct.unpack_from('<I3H',b,pos)
        nc=struct.unpack_from('<I',b,pos+12)[0] or chunks
        at=pos+16; cels={}; new_palette=False
        for _ in range(nc):
            cs,ct=struct.unpack_from('<IH',b,at); c=b[at+6:at+cs];at+=cs
            if ct==0x2004:
                flags,typ,level,_,_,blend=struct.unpack_from('<6H',c)
                ln=struct.unpack_from('<H',c,16)[0]
                if blend!=0:raise ValueError('Unsupported layer blend: '+str(path))
                visible=bool(flags&1)
                if level:
                    parents=[l for l in layers if l['level']<level]
                    visible=visible and (parents[-1]['visible'] if parents else True)
                layers.append(dict(visible=visible,opacity=c[12],level=level,name=c[18:18+ln].decode(),flags=flags))
            elif ct==0x2019:
                new_palette=True; count,first,last=struct.unpack_from('<3I',c); j=20
                if len(palette)<count:palette.extend([(0,0,0,255)]*(count-len(palette)))
                for k in range(first,last+1):
                    flags=struct.unpack_from('<H',c,j)[0];palette[k]=tuple(c[j+2:j+6]);j+=6
                    if flags&1:ln=struct.unpack_from('<H',c,j)[0];j+=2+ln
            elif ct in (4,17):
                # Old palettes are only fallback; new palette, when present, takes priority.
                if new_palette:continue
                j=2;index=0
                for _ in range(struct.unpack_from('<H',c)[0]):
                    skip,num=c[j:j+2];j+=2;index+=skip;num=num or 256
                    for k in range(num):
                        rgb=tuple(c[j:j+3]);j+=3
                        if ct==17:rgb=tuple(round(v*255/63) for v in rgb)
                        palette[index]=(*rgb,255);index+=1
            elif ct==0x2005:
                li,x,y,opacity,typ=struct.unpack_from('<HhhBH',c)
                if typ==1:
                    linked=struct.unpack_from('<H',c,16)[0]
                    image=old[(linked,li)][2].copy()
                elif typ in (0,2):
                    cw,ch=struct.unpack_from('<HH',c,16);raw=c[20:]
                    if typ==2:raw=zlib.decompress(raw)
                    if depth==8:
                        rgba=bytearray()
                        for v in raw:
                            color=palette[v]
                            rgba.extend((*color[:3],0 if v==transparent and not (layers[li]['flags']&8) else color[3]))
                        image=Image.frombytes('RGBA',(cw,ch),bytes(rgba))
                    elif depth==32:image=Image.frombytes('RGBA',(cw,ch),raw)
                    else:
                        rgba=bytes(v for i in range(0,len(raw),2) for v in (raw[i],raw[i],raw[i],raw[i+1]))
                        image=Image.frombytes('RGBA',(cw,ch),rgba)
                else:raise ValueError('Unsupported cel type '+str(typ))
                old[(fi,li)]=(x,y,image);cels[li]=(x,y,image,opacity)
        out=Image.new('RGBA',(w,h))
        for li,(x,y,im,opacity) in sorted(cels.items()):
            l=layers[li]
            if not l['visible']:continue
            im=im.copy(); alpha=opacity*l['opacity']//255
            if alpha!=255:im.putalpha(im.getchannel('A').point(lambda a:a*alpha//255))
            out.alpha_composite(im,(x,y))
        frames.append(out);pos+=size
    return frames

def pixels(im,alpha=False):
    data=bytearray();aa=bytearray()
    for r,g,b,a in im.convert('RGBA').getdata():
        data+=struct.pack('<H',((r>>3)<<11)|((g>>2)<<5)|(b>>3));aa.append(a)
    return bytes(data+aa) if alpha else bytes(data)

sheets={}; tsprops={}; tile_cache={}
materials={'ROAD':0,'TURBO':1,'SAND':2,'SNOW':3,'DEEP_WATER':4,'WATER':5,'AIR':6,'ICE':7}
for name in ('country','snow','city'):
    image=ase(S/f'maps/{name}.ase')[0]
    # Same special shadow/highlight colors as upstream core/assets-src/Makefile.
    image.putdata([(34,32,52,96) if p[:3]==(255,0,255) else (255,255,255,32) if p[:3]==(0,255,255) else p for p in image.getdata()])
    sheets[name]=image; root=ET.parse(A/f'maps/{name}.tsx').getroot()
    props={int(t.get('id')):t for t in root.findall('tile')};tsprops[name]=props
    image.save(PRE/f'{name}-tiles.png')

def gettile(name,gid):
    key=(name,gid)
    if key in tile_cache:return tile_cache[key]
    index=(gid&0x0fffffff)-1
    if index<0:return Image.new('RGBA',(32,32))
    im=sheets[name];cols=im.width//64
    im=im.crop(((index%cols)*64,(index//cols)*64,(index%cols+1)*64,(index//cols+1)*64))
    if gid&0x20000000:im=im.transpose(Image.Transpose.TRANSPOSE)
    if gid&0x80000000:im=im.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
    if gid&0x40000000:im=im.transpose(Image.Transpose.FLIP_TOP_BOTTOM)
    im=im.resize((32,32),Image.Resampling.NEAREST);tile_cache[key]=im
    return im

def decode(layer):
    d=layer.find('data');enc=d.get('encoding')
    if enc=='csv':return [int(v) for v in d.text.replace('\n','').split(',') if v.strip()]
    raw=base64.b64decode(d.text)
    if d.get('compression')=='zlib':raw=zlib.decompress(raw)
    elif d.get('compression'):raise ValueError(d.get('compression'))
    return list(struct.unpack('<%dI'%(len(raw)//4),raw))

def shape(obj,origin=(0,0),size=64):
    ox,oy=origin;x=float(obj.get('x','0'));y=float(obj.get('y','0'))
    w=float(obj.get('width','0'));h=float(obj.get('height','0'));ang=math.radians(float(obj.get('rotation','0')))
    node=obj.find('polygon')
    if node is not None:pts=[tuple(map(float,p.split(','))) for p in node.get('points').split()]
    elif obj.find('ellipse') is not None:pts=[(w*.5+w*.5*math.cos(i*math.pi/12),h*.5+h*.5*math.sin(i*math.pi/12)) for i in range(24)]
    elif w and h:pts=[(0,0),(w,0),(w,h),(0,h)]
    else:return []
    return [(ox+x+px*math.cos(ang)-py*math.sin(ang),oy+y+px*math.sin(ang)+py*math.cos(ang)) for px,py in pts]

def obstacle_polygons(definition):
    typ=definition['type']
    if typ=='multi':
        return [pair for d in definition['obstacles'] for pair in obstacle_polygons(d)]
    if typ=='full':return [([(0,0),(64,0),(64,64),(0,64)],True)]
    if typ=='circle':
        x=definition['x']*64;y=(1-definition['y'])*64;r=definition['radius']*64
        return [([(x+r*math.cos(i*math.pi/16),y+r*math.sin(i*math.pi/16)) for i in range(32)],False)]
    if typ=='rectangle':
        x=definition['x']-.5;y=definition['y']-.5;w=definition['width'];h=definition['height'];a=math.radians(definition.get('angle',0))
        pts=[]
        for px,py in [(x,y),(x+w,y),(x+w,y+h),(x,y+h)]:
            rx=px*math.cos(a)-py*math.sin(a);ry=px*math.sin(a)+py*math.cos(a)
            pts.append(((rx+.5)*64,(.5-ry)*64))
        return [(pts,True)]
    raise ValueError('Unknown obstacle type '+typ)
tracks=[]
for champ in range(3):
    c=ET.parse(A/f'championships/{champ}.xml').getroot()
    for t in c.findall('track'):tracks.append((t.get('id'),t.get('name'),c.get('id')))
metadata=[]
for mi,(name,title,music) in enumerate(tracks):
    root=ET.parse(A/f'maps/{name}.tmx').getroot();w=int(root.get('width'));h=int(root.get('height'));cells=w*h
    ts=root.find('tileset').get('source').split('.')[0]; layers=[(l.get('name'),decode(l)) for l in root.findall('layer') if l.get('name','').startswith(('bg','fg'))]
    groups={g.get('name'):g for g in root.findall('objectgroup')}
    bgatlas=[];fgatlas=[];bgids=[];fgids=[];material=[];bgcache={};fgcache={}
    backcolor=root.get('backgroundcolor','#808080')[-6:];rgb=tuple(bytes.fromhex(backcolor))
    coll=Image.new('L',(w*8,h*8));draw=ImageDraw.Draw(coll)
    for ob in groups['Obstacles'].findall('object'):
        pts=shape(ob)
        if pts:draw.polygon([(x/8,y/8) for x,y in pts],fill=1)
    for j in range(cells):
        bgkey=tuple(data[j] for ln,data in layers if ln.startswith('bg'));fgkey=tuple(data[j] for ln,data in layers if ln.startswith('fg'))
        for key,cache,atlas,ids,foreground in ((bgkey,bgcache,bgatlas,bgids,False),(fgkey,fgcache,fgatlas,fgids,True)):
            if key not in cache:
                im=Image.new('RGBA',(32,32),(0,0,0,0) if foreground else (*rgb,255))
                for gid in key:
                    if gid:im.alpha_composite(gettile(ts,gid))
                cache[key]=len(atlas);atlas.append(im)
            ids.append(cache[key])
        top=next((gid for gid in reversed(bgkey) if gid),0)&0x0fffffff
        tile=tsprops[ts].get(top-1);mat=0
        if tile is not None:
            prop=tile.find("properties/property[@name='material']")
            if prop is not None:mat=materials[prop.get('value')]
        material.append(mat)
        # Tiled editor objectgroup shapes are not what this upstream runtime uses.
        # TiledObstacleCreator reads the legacy JSON `obstacle` properties instead.
        for ln,data in layers:
            gid=data[j];tile=tsprops[ts].get((gid&0x0fffffff)-1)
            if tile is None:continue
            prop=tile.find("properties/property[@name='obstacle']")
            if prop is None:continue
            definition=json.loads(prop.get('value') or prop.text)
            for pts,transform in obstacle_polygons(definition):
                transformed=[]
                for x,y in pts:
                    if transform:
                        if gid&0x20000000:x,y=y,x
                        if gid&0x80000000:x=64-x
                        if gid&0x40000000:y=64-y
                    transformed.append(((j%w)*8+x/8,(j//w)*8+y/8))
                if transformed:draw.polygon(transformed,fill=1)
    ob=groups['Waypoints'].find('object');ox=float(ob.get('x'));oy=float(ob.get('y'))
    way=[((ox+float(p.split(',')[0]))*.5,(oy+float(p.split(',')[1]))*.5) for p in ob.find('polyline').get('points').split()]
    bonuses=[((float(o.get('x'))+float(o.get('width','0'))*.5)*.5,(float(o.get('y'))+float(o.get('height','0'))*.5)*.5) for o in groups.get('BonusSpots',[])]
    payload=bytearray(76);offsets=[]
    blocks=[struct.pack('<%dH'%cells,*bgids),struct.pack('<%dH'%cells,*fgids),bytes(material),coll.tobytes(),b''.join(struct.pack('<ff',*p) for p in way),b''.join(struct.pack('<ff',*p) for p in bonuses),b''.join(pixels(im) for im in bgatlas),b''.join(pixels(im,True) for im in fgatlas)]
    for block in blocks:
        payload+=bytes((-len(payload))%4);offsets.append(len(payload));payload+=block
    struct.pack_into('<4s18I',payload,0,b'PWT1',w,h,len(bgatlas),len(fgatlas),len(way),len(bonuses),w*8,h*8,*offsets,len(payload),0)
    (O/f'{name}.pwt').write_bytes(payload)
    preview=Image.new('RGBA',(w*32,h*32))
    for j in range(cells):preview.paste(bgatlas[bgids[j]],(j%w*32,j//w*32));preview.alpha_composite(fgatlas[fgids[j]],(j%w*32,j//w*32))
    preview.thumbnail((512,384),Image.Resampling.NEAREST);preview.save(PRE/f'{name}.png')
    metadata.append(dict(id=name,title=title,music=music,w=w,h=h,bytes=len(payload),bg=len(bgatlas),fg=len(fgatlas),waypoints=way))
    print(name,title,len(payload),'bytes',len(bgatlas),'bg',len(fgatlas),'fg')

cars=[];spriteblob=bytearray()
for xml in sorted((A/'vehicles').glob('*.xml')):
    vehicle=ET.parse(xml).getroot();name=vehicle.find('main').get('image');src=S/f'sprites/vehicles/{name}.ase'
    if not src.exists():continue
    body=ase(src)[0];im=Image.new('RGBA',(body.width+40,body.height+40))
    for axle in vehicle.findall('axle'):
        tire=ase(S/('sprites/tires/'+axle.get('tireSize','NORMAL')+'.still.ase'))[0].transpose(Image.Transpose.ROTATE_90)
        for side in (-1,1):
            cx=20+body.width*.5+side*float(axle.get('width'))*.5
            cy=20+body.height-float(axle.get('y'))
            im.alpha_composite(tire,(round(cx-tire.width*.5),round(cy-tire.height*.5)))
    im.alpha_composite(body,(20,20))
    im=im.resize((max(1,im.width//2),max(1,im.height//2)),Image.Resampling.NEAREST)
    off=len(spriteblob);spriteblob+=pixels(im,True)
    cars.append(dict(id=xml.stem,name=vehicle.get('name'),w=im.width,h=im.height,offset=off,speed=float(vehicle.get('speed','1'))))
    im.resize((im.width*4,im.height*4),Image.Resampling.NEAREST).save(PRE/f'car-{xml.stem}.png')
(O/'cars.bin').write_bytes(spriteblob)
logo=ase(S/'ui/title.still.ase')[0];(O/'title.bin').write_bytes(pixels(logo,True))
bonus=ase(S/'sprites/gift.still.ase')[0].resize((20,20),Image.Resampling.NEAREST);(O/'bonus.bin').write_bytes(pixels(bonus,True))
shutil.copyfile(A/'musics/championships/country.mp3',O/'country.mp3')
for name in ('snow','city'):shutil.copyfile(A/f'musics/championships/{name}.mp3',O/f'{name}.mp3')
header=['/* Generated from upstream TMX/Aseprite; do not edit. */','#pragma once',f'#define TRACK_COUNT {len(metadata)}',f'#define CAR_COUNT {len(cars)}',f'#define CAR_BYTES {len(spriteblob)}',f'#define LOGO_W {logo.width}',f'#define LOGO_H {logo.height}',f'#define MAP_CAP {((max(m["bytes"] for m in metadata)+65535)//65536)*65536}', 'static const TrackDef track_defs[]={']
header += ['{"%s","%s","%s"},'%(m['id'],m['title'],m['music']) for m in metadata]
header+=['};','static const CarDef car_defs[]={']+['{"%s","%s",%d,%d,%d,%.3ff},'%(c['id'],c['name'],c['w'],c['h'],c['offset'],c['speed']) for c in cars]+['};','static const float sin_table[256]={']
header += [','.join('%.7ff'%math.sin(i*2*math.pi/256) for i in range(start,start+16))+',' for start in range(0,256,16)]
header+=['};']
(P/'src/catalog.h').write_text('\n'.join(header)+'\n',encoding='utf8')
(O/'catalog.json').write_text(json.dumps(dict(upstream='agateau/pixelwheels',commit='0ed0376a6b1e4ba0f055b788fb84cb08d20cf8ea',tracks=metadata,cars=cars),indent=2),encoding='utf8')
print('Cars',len(cars),'sprite bytes',len(spriteblob),'map cap',max(m['bytes'] for m in metadata))
# Prepare all audio after original graphics and map conversion.
import subprocess, sys
subprocess.run([sys.executable,str(P/'tools/prepare_audio.py')],check=True)
