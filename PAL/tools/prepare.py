#!/usr/bin/env python3
"""Rebuild native PAL data/audio from project-local sources and stage resources."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--pack',action='store_true')
    p.add_argument('--audio',action='store_true')
    p.add_argument('--node',default=os.environ.get('ASTER_NODE',shutil.which('node')))
    args=p.parse_args()
    if args.pack:
        port=ROOT/'port';out=port/'resources';out.mkdir(parents=True,exist_ok=True)
        engine=ROOT/'sdlpal-aster'
        subprocess.run([sys.executable,str(engine/'tools/pal_chapter_pack_build.py'),
                        str(ROOT/'pal-resources/pal'),'--out-dir',str(out),
                        '--font10-archive',str(engine/'third_party/fusion-pixel-font/fusion-pixel-font-10px-monospaced-bdf-v2026.07.20.zip')],check=True)
        shutil.copyfile(out/'pal_full.pak',out/'pal_full.source.pak')
        subprocess.run([sys.executable,str(port/'localize-resources.py')],check=True)
    if args.audio:
        if not args.node:p.error('Set ASTER_NODE or pass --node for offline audio conversion')
        subprocess.run([sys.executable,str(ROOT/'tools/build_audio.py')],check=True)
        subprocess.run([args.node,'port/prepare-audio.mjs'],cwd=ROOT,check=True)
    subprocess.run([sys.executable,str(ROOT/'tools/prepare_store.py')],check=True)


if __name__=='__main__':main()
