"""Prepare pinned sources and original assets for the Aster port."""
from pathlib import Path
import ast, hashlib, io, json, os, re, shutil, struct, subprocess, sys, tarfile, urllib.request, wave
from PIL import Image, ImageFont

P = Path(__file__).resolve().parents[1]
SDK = Path(os.environ.get('ASTER_SDK', P.parents[1] / 'aster-sdk'))
UP = P/'vendor/upstream'

def prepare_levels():
    source=(UP/'levels.lua').read_text(encoding='utf-8').split('local levels =',1)[1]
    tokens=re.findall(r"--[^\n]*|'(?:\\.|[^'\\])*'|-?\d+(?:\.\d+)?|[{}\[\]=,;]",source)
    tokens=[t for t in tokens if not t.startswith('--')];pos=0
    def value():
        nonlocal pos
        t=tokens[pos];pos+=1
        if t!='{':return ast.literal_eval(t)
        pairs={};items=[]
        while tokens[pos]!='}':
            if tokens[pos]=='[':
                pos+=1;k=value();assert tokens[pos:pos+2]==[']','='];pos+=2;pairs[k]=value()
            else:items.append(value())
            if tokens[pos] in (',',';'):pos+=1
        pos+=1
        return pairs if pairs else items
    levels=value()
    types=['MiniGold','NormalGold','NormalGoldPlus','BigGold','MiniRock','NormalRock','BigRock','Diamond','QuestionBag','Mole','MoleWithDiamond','Skull','Bone','TNT']
    out=['/* Generated exactly from upstream levels.lua. */']
    for i,(name,level) in enumerate(levels.items()):
        out.append('static const Seed level_%d[] = {'%i)
        for e in level['entities']:
            out.append('{%d,%d,%d,%d},'%(types.index(e['type']),e['pos']['x'],e['pos']['y'],-1 if e.get('dir')=='Left' else 1))
        out.append('};')
    out.append('static const Level levels[] = {')
    for i,(name,level) in enumerate(levels.items()):
        out.append('{"%s",%d,sizeof(level_%d)/sizeof(Seed),level_%d},'%(name,ord(level['type'][-1])-ord('A'),i,i))
    out.append('};')
    (P/'port/levels.h').write_text('\n'.join(out)+'\n',encoding='utf-8')
    print('Converted',len(levels),'original level layouts; no Lua runtime')

def prepare_libc():
    out=P/'artifacts/deps';out.mkdir(parents=True,exist_ok=True)
    # The sibling project's pinned archives are only a download cache. All
    # needed headers/libraries are extracted into this project independently.
    files=[('wasi-sysroot-24.0.tar.gz','35172f7d2799485b15a46b1d87f50a585d915ec662080f005d99153a50888f08'),
           ('libclang_rt.builtins-wasm32-wasi-24.0.tar.gz','7e33c0df758b90469b1de3ca158e2d0a7f71934d5884525ba6a372de0b3b0ec7')]
    for name,sha in files:
        cached=P.parent/'OpenOMF/artifacts/engine-deps'/name
        dest=out/name
        if not dest.exists():
            if cached.exists():shutil.copyfile(cached,dest)
            else: dest.write_bytes(urllib.request.urlopen('https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-24/'+name,timeout=90).read())
        if hashlib.sha256(dest.read_bytes()).hexdigest()!=sha:raise ValueError(name)
        if name.startswith('wasi') and not (out/'wasi/include/wasm32-wasi/stdlib.h').exists():
            with tarfile.open(dest) as t:
                for m in t:
                    rel=Path(m.name).relative_to('wasi-sysroot-24.0')
                    if m.isfile() and (rel.as_posix().startswith('include/') or rel.as_posix().startswith('lib/wasm32-wasi/')):
                        target=out/'wasi'/rel;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(t.extractfile(m).read())
        if name.startswith('libclang') and not (out/'libclang_builtins.a').exists():
            with tarfile.open(dest) as t:
                m=next(m for m in t if m.isfile() and m.name.endswith('.a'))
                (out/'libclang_builtins.a').write_bytes(t.extractfile(m).read())
    shutil.copyfile(out/'wasi/lib/wasm32-wasi/libc.a',out/'libasterc.a')
    ar=SDK/'toolchain/wasi-sdk-24.0-x86_64-windows/bin/llvm-ar.exe'
    subprocess.run([str(ar),'d',str(out/'libasterc.a'),'__wasilibc_real.o'],check=True)

def prepare_assets():
    assets=P/'assets';assets.mkdir(exist_ok=True)
    records=[]
    def add(name,data,w=0,h=0,kind=0):records.append((name,data,w,h,kind))
    sheets={'miner_sheet':(32,40),'shopkeeper_sheet':(80,80),'hook_sheet':(13,15),
            'mole_sheet':(18,13),'mole_with_diamond_sheet':(18,13),
            'gold_big_fx_sheet':(54,54),'explosive_fx_sheet':(64,64),'bigger_explosive_fx_sheet':(100,100)}
    for f in sorted((UP/'images').glob('*.png')):
        if f.stem in ('icon','items_sheet','light'):continue
        im=Image.open(f).convert('RGBA');w,h=im.size
        fw,fh=sheets.get(f.stem,(w,h))
        add('image/'+f.name,struct.pack('<HHHH',w,h,fw,fh),w,h,2)
        for yy in range(0,h,fh):
            for xx in range(0,w,fw):
                tile=im.crop((xx,yy,xx+fw,yy+fh))
                # Resize once offline with premultiplied alpha; runtime copies native pixels.
                tile=tile.convert('RGBa').resize((round(fw*1.2),round(fh*1.2)),Image.Resampling.LANCZOS).convert('RGBA')
                opaque=f.name.startswith('bg_')
                if opaque:
                    black=Image.new('RGBA',tile.size,(0,0,0,255));black.alpha_composite(tile);tile=black
                raw=bytearray()
                for r,g,b,a in tile.getdata():
                    c=((r>>3)<<11)|((g>>2)<<5)|(b>>3)
                    raw.extend(struct.pack('<H',c))
                    if not opaque:raw.append(a)
                add(f'pixel/{f.name}/{xx//fw+(yy//fh)*(w//fw)}',raw,tile.width,tile.height,0 if opaque else 1)
    # Rasterize text at its final screen size, with an open sans face for small UI.
    font_defs=[(P/'vendor/fonts/DejaVuSans.ttf',12),(P/'vendor/fonts/DejaVuSans.ttf',12),
               (UP/'fonts/Kurland.ttf',24),(UP/'fonts/Kurland.ttf',12),
               (P/'vendor/fonts/DejaVuSans.ttf',12),(UP/'fonts/Kurland.ttf',19)]
    for fid,(file,size) in enumerate(font_defs):
        font=ImageFont.truetype(str(file),size)
        ascent,descent=font.getmetrics(); line=ascent+descent
        glyphs=[];pixels=bytearray()
        for code in range(32,127):
            ch=chr(code);x0,y0,x1,y1=font.getbbox(ch);gw=max(0,x1-x0);gh=max(0,y1-y0)
            image=Image.new('L',(max(1,gw),max(1,gh)));from PIL import ImageDraw
            ImageDraw.Draw(image).text((-x0,-y0),ch,font=font,fill=255)
            glyphs.append(struct.pack('<hhhhHHI',x0,y0,gw,gh,round(font.getlength(ch)*64),0,len(pixels)))
            if gw and gh:pixels.extend(image.tobytes())
        add('font/'+str(fid),struct.pack('<HH',line,ascent)+b''.join(glyphs)+pixels,kind=3)
    # Keep each sample's complete duration; all original effects fit the
    # public 64 KiB PCM budget after conversion to 22.05 kHz mono PCM16.
    ffmpeg=shutil.which('ffmpeg')
    durations={}
    for f in sorted((UP/'audios').iterdir()):
        if f.suffix not in ('.wav','.mp3'):continue
        target=assets/'audio'/f.with_suffix('.wav').name;target.parent.mkdir(exist_ok=True)
        if ffmpeg:
            cmd=[ffmpeg,'-v','error','-y','-i',str(f),'-ac','1','-ar','22050','-c:a','pcm_s16le',str(target)]
        else:
            win=lambda p:'/mnt/'+str(p.resolve())[0].lower()+str(p.resolve())[2:].replace('\\','/')
            cmd=['wsl','-d','Ubuntu-22.04','--exec','ffmpeg','-v','error','-y','-i',win(f),'-ac','1','-ar','22050','-c:a','pcm_s16le',win(target)]
        subprocess.run(cmd,check=True)
        with wave.open(str(target)) as wav:duration=wav.getnframes()/wav.getframerate()
        durations[f.name]=duration
        if f.name in ('goal.mp3','made_goal.mp3'):shutil.copyfile(f,target.parent/f.name)
        # Goal jingles use the background-file API; effects keep full samples.
    names=['money.wav','hook_reset.wav','grab_start.mp3','grab_back.wav','explosive.wav','high_value.wav','normal_value.wav','low_value.wav','goal.mp3','made_goal.mp3']
    (P/'port/audio_meta.h').write_text('static const unsigned sound_ms[]={'+','.join(str(round(durations[n]*1000)) for n in names)+'};\n',encoding='utf-8')
    offset=8+len(records)*80;table=bytearray();payload=bytearray()
    for name,data,w,h,kind in records:
        encoded=name.encode();assert len(encoded)<64
        table.extend(struct.pack('<64sIIHHI',encoded,offset,len(data),w,h,kind));payload.extend(data);offset+=len(data)
    (assets/'game.gma').write_bytes(struct.pack('<4sI',b'GMA1',len(records))+table+payload)
    icon=Image.open(UP/'images/icon.png').convert('RGBA').resize((64,64),Image.Resampling.LANCZOS)
    icon.save(assets/'icon.png')
    subprocess.run([sys.executable,str(SDK/'tools/icon_convert.py'),str(assets/'icon.png'),'-o',str(assets/'icon.bin'),'--format','rgb565a8','--size','48'],check=True)
    print('Prepared',len(records),'records,',offset,'asset bytes; audio seconds',durations)

if __name__=='__main__':
    prepare_libc();prepare_levels();prepare_assets()
