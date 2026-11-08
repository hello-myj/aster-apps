#!/usr/bin/env python3
"""Convert the pinned upstream PNG to a 48px ASTI RGB565+A8 launcher icon."""
import os
from pathlib import Path
import shutil
import struct
import subprocess

ROOT=Path(__file__).resolve().parents[1]
source=ROOT/'sdlpal-aster/Icon.png'
exe=os.environ.get('FFMPEG') or shutil.which('ffmpeg')
if exe:cmd=[exe]
elif os.name=='nt' and shutil.which('wsl'):
    cmd=['wsl','-d','Ubuntu-22.04','--exec','ffmpeg']
    p=str(source.resolve()).replace('\\','/')
    source='/mnt/'+p[0].lower()+p[2:]
else:raise SystemExit('Install ffmpeg or set FFMPEG')
raw=subprocess.check_output(cmd+['-hide_banner','-loglevel','error','-i',str(source),
    '-vf','scale=48:48:flags=lanczos','-pix_fmt','rgba','-f','rawvideo','pipe:1'])
if len(raw)!=48*48*4:raise ValueError('Unexpected icon dimensions')
rgb=bytearray();alpha=bytearray()
for i in range(0,len(raw),4):
    r,g,b,a=raw[i:i+4]
    rgb+=struct.pack('<H',((r>>3)<<11)|((g>>2)<<5)|(b>>3));alpha.append(a)
data=rgb+alpha
dest=ROOT/'assets/icon.bin';dest.parent.mkdir(parents=True,exist_ok=True)
dest.write_bytes(struct.pack('<4sHHHBBI',b'ASTI',1,48,48,4,0,len(data))+data)
print(dest)
