#!/usr/bin/env python3
"""Run in WSL/Linux after compiling render_music.c. Uses original synthesis."""
import concurrent.futures, json, pathlib, struct, subprocess, wave, array, shlex
ROOT=pathlib.Path(__file__).resolve().parents[1]
DATA=ROOT/'sdcard/aster/apps/aster.opentyrian/data'
OUT=DATA/'audio'
(OUT/'music').mkdir(parents=True,exist_ok=True)
(OUT/'sfx').mkdir(parents=True,exist_ok=True)
flags=shlex.split(subprocess.check_output(['sdl2-config','--cflags'],text=True))
subprocess.run(['gcc','-O2',*flags,'-I'+str(ROOT/'vendor/upstream/src'),str(ROOT/'tools/render_music.c'),*[str(ROOT/'vendor/upstream/src'/f) for f in ('lds_play.c','opl.c','memreader.c')],'-o',str(ROOT/'artifacts/render-music'),'-lm'],check=True)
def music(n):
    dest=OUT/'music'/f'{n:02}.mp3'
    wav=ROOT/'artifacts'/f'music-{n:02}.wav'
    result=subprocess.run([str(ROOT/'artifacts/render-music'),str(DATA/'resources/music.mus'),str(n),str(wav)],check=True,capture_output=True,text=True).stdout.strip()
    subprocess.run(['ffmpeg','-y','-hide_banner','-loglevel','error','-i',str(wav),'-af','volume=1.5','-codec:a','libmp3lame','-b:a','64k',str(dest)],check=True)
    wav.unlink()
    print(result,flush=True)
    return result
def sounds(filename,offset,trim,xmas):
    buf=(DATA/'resources'/filename).read_bytes()
    count=struct.unpack_from('<H',buf)[0]
    positions=list(struct.unpack_from('<'+'I'*count,buf,2))+[len(buf)]
    for i in range(count):
        raw=buf[positions[i]:positions[i+1]-(100 if trim else 0)]
        # Original files contain signed 8-bit mono at 11025 Hz.
        values=struct.unpack('<'+'b'*len(raw),raw)
        pcm=array.array('h',(x*256 for x in values))
        dest=OUT/'sfx'/f'{offset+i+(100 if xmas else 0):03}.wav'
        with wave.open(str(dest),'wb') as f:
            f.setnchannels(1);f.setsampwidth(2);f.setframerate(11025);f.writeframes(pcm.tobytes())
        if len(pcm)*2>65536:
            tmp=dest.with_suffix('.tmp.wav')
            subprocess.run(['ffmpeg','-y','-hide_banner','-loglevel','error','-i',str(dest),'-ar','8000',str(tmp)],check=True)
            tmp.replace(dest)
        if dest.stat().st_size>69632:raise RuntimeError(f'Sound exceeds Aster effect limit: {dest}')
sounds('tyrian.snd',0,False,False)
sounds('voices.snd',29,True,False)
sounds('tyrian.snd',0,False,True)
sounds('voicesc.snd',29,True,True)
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
    result=list(pool.map(music,range(41)))
(ROOT/'artifacts/music-render.json').write_text(json.dumps(result,indent=2))
