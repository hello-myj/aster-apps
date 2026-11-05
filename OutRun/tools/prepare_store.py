#!/usr/bin/env python3
"""Stage seekable OPK1 chunks and audio, excluding duplicate loose images/maps."""
from pathlib import Path
import json,shutil,subprocess,sys
root=Path(__file__).resolve().parents[1]
subprocess.run([sys.executable,str(root/'tools/pack_resources.py')],check=True)
metadata=json.loads((root/'dist/resources.json').read_text())
stage=root/'dist/store-resources/resources';stage.mkdir(parents=True,exist_ok=True)
expected={x['name'] for x in metadata['chunks']}
for old in stage.glob('resources*.pak'):
    if old.is_file() and old.name not in expected:old.unlink()
for item in metadata['chunks']:
    source=root/'dist'/item['name']
    if source.stat().st_size!=item['bytes']:raise RuntimeError('Resource chunk size mismatch')
    shutil.copy2(source,stage/item['name'])
audio=root/'sdcard/aster/apps/aster.outrun/data/resources/audio'
if not audio.is_dir():raise RuntimeError('Prepare original audio resources before staging')
shutil.copytree(audio,stage/'audio',dirs_exist_ok=True)
print('Store resource chunks:',len(expected),'packed bytes:',metadata['bytes'])
