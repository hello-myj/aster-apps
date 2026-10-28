#!/usr/bin/env python3
"""Fetch pinned tool dependencies and original game data; never alters the SDK."""
from pathlib import Path
import hashlib, shutil, subprocess, sys, tarfile, urllib.request, zipfile
ROOT=Path(__file__).resolve().parents[1]
DEP=ROOT/'artifacts/deps'
DEP.mkdir(parents=True,exist_ok=True)
FILES=[
 ('wasi-sysroot-24.0.tar.gz','https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-24/wasi-sysroot-24.0.tar.gz','35172f7d2799485b15a46b1d87f50a585d915ec662080f005d99153a50888f08'),
 ('libclang_rt.builtins-wasm32-wasi-24.0.tar.gz','https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-24/libclang_rt.builtins-wasm32-wasi-24.0.tar.gz','7e33c0df758b90469b1de3ca158e2d0a7f71934d5884525ba6a372de0b3b0ec7'),
 ('SDL2-2.32.10.zip','https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/SDL2-2.32.10.zip','12b2dc2eb8f2836100a7916b5d394a0c82f1f7e32693f95f98305403af242f08'),
 ('binaryen-version_133-x86_64-windows.tar.gz','https://github.com/WebAssembly/binaryen/releases/download/version_133/binaryen-version_133-x86_64-windows.tar.gz','17a2cbeac6b5693c5fbafab3838d3c65fd9c1eb38b05f5baec6c657e8c84995b'),
 ('tyrian21.zip','https://camanis.net/tyrian/tyrian21.zip','7790d09a2a3addcd33c66ef063d5900eb81cc9c342f4807eb8356364dd1d9277')]
opener=urllib.request.build_opener(urllib.request.ProxyHandler({}))
for name,url,digest in FILES:
    dest=DEP/name
    if not dest.exists():
        # Existing verified archives from the initial local preparation can be reused.
        candidate=ROOT/'artifacts'/name
        if candidate.exists():shutil.copyfile(candidate,dest)
        else:
            part=dest.with_suffix(dest.suffix+'.part')
            with opener.open(url,timeout=120) as src,part.open('wb') as out:shutil.copyfileobj(src,out)
            if hashlib.sha256(part.read_bytes()).hexdigest()!=digest:raise RuntimeError('SHA256 mismatch: '+name)
            part.replace(dest)
    if hashlib.sha256(dest.read_bytes()).hexdigest()!=digest:raise RuntimeError('SHA256 mismatch: '+name)
    print('Verified',name,flush=True)
def write(rel,data):
    target=DEP/rel
    if not target.resolve().is_relative_to(DEP.resolve()):raise ValueError(rel)
    target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
with tarfile.open(DEP/FILES[0][0]) as archive:
    for item in archive:
        rel=Path(item.name).relative_to('wasi-sysroot-24.0')
        if item.isfile() and (rel.as_posix().startswith('include/') or rel.as_posix().startswith('lib/wasm32-wasi/')):write(Path('wasi')/rel,archive.extractfile(item).read())
with tarfile.open(DEP/FILES[1][0]) as archive:
    item=next(x for x in archive if x.isfile() and x.name.endswith('.a'))
    write('libclang_builtins.a',archive.extractfile(item).read())
with zipfile.ZipFile(DEP/FILES[2][0]) as archive:
    for name in archive.namelist():
        rel=Path(name).relative_to('SDL2-2.32.10')
        if not name.endswith('/') and (rel.as_posix().startswith('include/') or rel.as_posix()=='LICENSE.txt'):write(Path('SDL2')/rel,archive.read(name))
shutil.copyfile(ROOT/'port/include/SDL_config.h',DEP/'SDL2/include/SDL_config.h')
with tarfile.open(DEP/FILES[3][0]) as archive:
    for item in archive:
        if item.isfile() and (item.name.endswith('.exe') or 'LICENSE' in item.name):write(item.name,archive.extractfile(item).read())
resources=ROOT/'sdcard/aster/apps/aster.opentyrian/data/resources'
resources.mkdir(parents=True,exist_ok=True)
with zipfile.ZipFile(DEP/FILES[4][0]) as archive:
    for name in archive.namelist():
        rel=Path(name).name.lower()
        if rel and Path(rel).suffix in ('.dat','.lvl','.shp','.pic','.snd','.mus','.hdt','.cdt','.doc','.txt','.1','.2','.3','.4','.5','.anm'):
            (resources/rel).write_bytes(archive.read(name))
ar=ROOT.parent.parent/'aster-sdk/toolchain/wasi-sdk-24.0-x86_64-windows/bin/llvm-ar.exe'
shutil.copyfile(DEP/'wasi/lib/wasm32-wasi/libc.a',DEP/'libasterc.a')
subprocess.run([str(ar),'d',str(DEP/'libasterc.a'),'__wasilibc_real.o'],check=True)
subprocess.run([sys.executable,str(ROOT.parent.parent/'aster-sdk/tools/icon_convert.py'),str(ROOT/'vendor/upstream/linux/icons/tyrian-48.png'),'-o',str(ROOT/'assets/icon.bin'),'--format','rgb565a8','--size','48'],check=True)
print('Dependencies/data ready. Render audio in WSL using tools/prepare_audio.py.')
