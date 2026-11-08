#!/usr/bin/env python3
"""Stage PAL's complete segmented pack and audio for the current app store."""
import hashlib
import json
from pathlib import Path
import shutil
from prepare_native_font import main as prepare_native_font

ROOT=Path(__file__).resolve().parents[1]
PART_BYTES=3*1048576


def main():
    pack=ROOT/'port/resources/pal_full.pak'
    music=ROOT/'port/sim-state/shared/music/pal'
    if not pack.is_file() or not music.is_dir():
        raise SystemExit('Prepare the PAL pack and audio first; see README.md')
    out=ROOT/'dist/resources'
    resources=out/'resources';resources.mkdir(parents=True,exist_ok=True)
    records=[]
    with pack.open('rb') as f:
        index=0
        while data:=f.read(PART_BYTES):
            p=resources/f'pal_full.{index:02d}';p.write_bytes(data)
            records.append({'name':p.name,'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()})
            index+=1
    (resources/'parts.json').write_text(json.dumps({'bytes':pack.stat().st_size,'part_bytes':PART_BYTES,'parts':records},indent=2),encoding='utf8')
    for name in ['localization.json']:
        if (pack.parent/name).is_file():shutil.copyfile(pack.parent/name,resources/name)
    for source in music.rglob('*'):
        if source.is_file() and source.suffix in ['.mp3','.wav']:
            dest=out/'audio'/source.relative_to(music)
            dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(source,dest)
    prepare_native_font()
    files=[p for p in out.rglob('*') if p.is_file()]
    if any(p.stat().st_size>4*1048576 for p in files):
        raise ValueError('An asset exceeds the device file limit')
    print(json.dumps({'files':len(files),'bytes':sum(p.stat().st_size for p in files),'pack_sha256':hashlib.sha256(pack.read_bytes()).hexdigest()},indent=2))


if __name__=='__main__':main()
