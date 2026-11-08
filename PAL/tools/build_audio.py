#!/usr/bin/env python3
"""Build the host-only bounded RIX/OPL converter; never shipped to the board."""
from pathlib import Path
import os
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]
SDK=Path(os.environ.get('ASTER_SDK',ROOT.parent.parent/'aster-sdk'))
sys.path.insert(0,str(SDK/'tools'))
import build

clang=Path(build.find_clang())
engine=ROOT/'sdlpal-aster';port=ROOT/'port';out=port/'build/audio'
out.mkdir(parents=True,exist_ok=True)
flags=['--target=wasm32-unknown-wasi','-std=c++17','-Os','-ffreestanding',
       '-fno-exceptions','-fno-rtti','-fno-threadsafe-statics',
       '-DPAL_NO_RUNTIME_HEAP=1','-DUSE_RIX_EXTRA_INIT=0','-DPAL_HAS_MP3=0',
       '-DPAL_HAS_OGG=0','-DPAL_HAS_OPUS=0','-DPAL_HAS_NATIVEMIDI=0',
       '-DPAL_HAS_GLSL=0','-DPAL_HAS_CONFIG_PAGE=0','-DPAL_NO_LAUNCH_UI=1',
       '-include',str(port/'include/pal_libc.h')]
includes=[port/'audio-tools/include',port/'include',engine/'esp32s3/native_engine_shim/include',
          engine/'esp32s3/engine_bridge/include',engine/'sdl_compat',engine]
flags+=['-I'+str(p) for p in includes]
sources=[engine/'adplug/player.cpp',engine/'adplug/rix.cpp',
         engine/'embedded/pal_mame_opl2_static.cpp',port/'audio-tools/render.cpp']
objects=[]
for i,source in enumerate(sources):
    obj=out/f'{i}.o';objects.append(obj)
    subprocess.run([str(clang)]+flags+['-c',str(source),'-o',str(obj)],check=True)
crt=out/'crt.o';objects.append(crt)
subprocess.run([str(clang),'--target=wasm32-unknown-wasi','-Os','-ffreestanding',
                '-I'+str(SDK/'include'),'-I'+str(SDK/'lib/aster'),'-c',
                str(SDK/'lib/aster/crt.c'),'-o',str(crt)],check=True)
exports=['__wasm_call_ctors','pal_rix_input','pal_rix_output','pal_rix_load','pal_rix_tick']
linker=clang.parent/('wasm-ld.exe' if os.name=='nt' else 'wasm-ld')
subprocess.run([str(linker),'--no-entry','--initial-memory=262144','--max-memory=262144']+
               ['--export='+e for e in exports]+[str(p) for p in objects]+
               ['-o',str(out/'rix-render.wasm')],check=True)
