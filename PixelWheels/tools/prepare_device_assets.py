#!/usr/bin/env python3
"""Build a device SD resource set, retaining full original music duration."""
from pathlib import Path
import argparse,subprocess,shutil,json,hashlib
p=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser();parser.add_argument('--ffmpeg',default='ffmpeg');args=parser.parse_args()
src=p/'sdcard/aster/apps/aster.pixelwheels/data/resources';out=p/'dist/device-resources';out.mkdir(parents=True,exist_ok=True)
records=[]
for original in sorted(src.iterdir()):
    if not original.is_file():continue
    dest=out/original.name
    if original.suffix=='.mp3':
        subprocess.run([args.ffmpeg,'-hide_banner','-loglevel','error','-y','-i',str(original),'-vn','-map_metadata','-1','-c:a','libmp3lame','-b:a','128k','-ar','44100','-ac','2',str(dest)],check=True)
    else:shutil.copyfile(original,dest)
    if dest.stat().st_size>=4*1024*1024:raise SystemExit('File exceeds device transfer limit: '+dest.name)
    records.append(dict(file=dest.name,bytes=dest.stat().st_size,sha256=hashlib.sha256(dest.read_bytes()).hexdigest(),music_bitrate=128000 if original.suffix=='.mp3' else None))
(out/'device-resources.json').write_text(json.dumps(records,indent=2),encoding='utf8')
print(json.dumps({'files':len(records),'bytes':sum(r['bytes'] for r in records),'music':[{k:r[k] for k in ('file','bytes')} for r in records if r['file'].endswith('.mp3')]},indent=2))