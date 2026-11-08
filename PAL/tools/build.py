#!/usr/bin/env python3
"""Compile the pinned PAL engine, lower Asyncify, then use current SDK AOT/pack."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
ENGINE = ROOT/'sdlpal-aster'
SDK = Path(os.environ.get('ASTER_SDK', ROOT.parent.parent/'aster-sdk'))
sys.path.insert(0, str(SDK/'tools'))
import build
import aster_board

SOURCES = '''battle.c ending.c fight.c game.c global.c input.c itemmenu.c
magicmenu.c main.c map.c overlay.c palcfg.c palcommon.c paldebug.c palette.c
play.c res.c rngplay.c scene.c script.c ui.c uibattle.c uigame.c util.c video.c
sdl_compat/sdl_compat.c unix/embedded_contract_stubs.c embedded/pal_pack.c
embedded/pal_memory_profile.c embedded/pal_text_cache.c embedded/pal_font_cache.c
embedded/pal_font10_cache.c embedded/pal_native_ui.c embedded/pal_level2_resident_pack.c
esp32s3/native_engine_shim/sdl_shim.c esp32s3/engine_bridge/pal_engine_pack_provider.c'''.split()
DEFINES = '''PAL_HEADLESS_SDL_SHIM=1 PAL_HAS_JOYSTICKS=0 PAL_HAS_TOUCH=0
PAL_HAS_MP3=0 PAL_HAS_OGG=0 PAL_HAS_OPUS=0 PAL_HAS_NATIVEMIDI=0 PAL_HAS_GLSL=0
PAL_HAS_CONFIG_PAGE=0 PAL_NO_LAUNCH_UI=1 PAL_NO_RUNTIME_DECOMPRESS=1
PAL_NO_RUNTIME_HEAP=1 PAL_CONTRACT_NO_AUDIO=1 PAL_ESP_CORES3SE_NO_AUDIO=1
PAL_CONTRACT_TARGET_PACK_PROVIDER=1 PAL_ENGINE_BRIDGE_REQUIRE_TARGET_HOOKS=1
USE_RIX_EXTRA_INIT=0 USE_SDL3=0 MEM_LEVEL2=1 PAL_STORAGE_SD_ONLY=1
PAL_EXTREME_TWO_SCREENS=1 PAL_SDL_SHIM_EXTERNAL_SURFACES_ONLY=1 SHIM_MAX_SURFACES=8
SHIM_MAX_PALETTES=8 SHIM_MAX_TEXTURES=1 SHIM_SURFACE_PIXELS=65536u
main=PAL_EngineMain PAL_ASTER=1 PAL_ASTER_FONT16=1
PAL_ASTER_STACK_BYTES=65536u
PAL_MEM_LEVEL2_RESIDENT_PACK_BYTES=1048576u'''.split()


def run(cmd):
    subprocess.run([str(x) for x in cmd], cwd=ROOT, check=True)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--target', choices=['riscv', 'xtensa'], default='riscv')
    p.add_argument('--no-aot', action='store_true')
    p.add_argument('--node', default=os.environ.get('ASTER_NODE', shutil.which('node')))
    args = p.parse_args()
    if not args.node:
        p.error('Node.js required; set ASTER_NODE or pass --node')
    workspace = Path(os.environ.get('ASTER_WORKSPACE', ROOT))
    out = workspace/'build'/args.target/'aster.pal'
    out.mkdir(parents=True, exist_ok=True)
    app = build.App(ROOT)
    app.target = aster_board.get(args.target)
    if args.target == 'xtensa':
        # A native 320x240 canvas needs 150 KiB, not the P4's 450 KiB.
        # The game owns fixed arenas; the SDK heap is only for canvas/events.
        app.initial_memory=3670016
        app.max_memory=4194304
        app.heap=262144
    clang = Path(build.find_clang())
    linker = clang.parent/('wasm-ld.exe' if os.name == 'nt' else 'wasm-ld')
    includes = [ROOT/'port/include', SDK/'include', SDK/'lib/aster',
                ENGINE/'esp32s3/engine_bridge', ENGINE/'esp32s3/engine_bridge/include',
                ENGINE/'esp32s3/native_engine_shim/include', ENGINE, ENGINE/'embedded',
                ENGINE/'sdl_compat']
    flags = ['--target=wasm32-unknown-wasi', '-std=c11', '-Os', '-ffreestanding',
             '-fno-strict-aliasing', '-Wno-incompatible-pointer-types']
    flags += ['-D'+d for d in DEFINES]+['-I'+str(d) for d in includes]
    flags.append('-DPAL_ASTER_RESIDENT_MASK='+str(sum(1<<i for i in [3,12,15,17,18]))+'u')
    flags.append('-DPAL_ASTER_NATIVE_UI='+('0' if args.target=='xtensa' else '1'))
    if args.target=='xtensa':flags.append('-DPAL_MAX_FRAME_PIXELS=76800')
    sources = [ENGINE/s for s in SOURCES]
    sources += [ROOT/'port'/s for s in ['platform.c','libc.c','debug.c','audio.c','native_ui.c','native_ui_hooks.c']]
    sources += [SDK/'lib/aster'/s for s in ['crt.c','aster_printf.c','aster_gfx.c']]
    objects = [out/f'{i:02d}_{s.stem}.o' for i,s in enumerate(sources)]
    def compile_one(pair):
        s,o = pair
        run([clang]+flags+['-c',s,'-o',o])
    with ThreadPoolExecutor(max_workers=6) as pool:
        list(pool.map(compile_one, zip(sources,objects)))
    exports = '''aster_app_init aster_app_start aster_app_event aster_app_stop
aster_app_destroy aster_app_pause aster_app_resume __heap_base PAL_IsInDialog
pal_debug_main_menu pal_debug_system_menu pal_debug_viewport pal_debug_scene
pal_async_start_unwind pal_async_start_rewind pal_async_stop_unwind
pal_async_stop_rewind pal_async_get_state PAL_EngineMain'''.split()
    wraps = '''PAL_MKFOpenPackArchive PAL_MKFIsPackArchive PAL_MKFMapChunk
PAL_MKFGetChunkCount PAL_MKFGetChunkSize PAL_MKFReadChunk fclose'''.split()
    if args.target=='riscv':
        wraps += '''SDL_BlitSurface SDL_BlitScaled SDL_FillRect SDL_FreeSurface
PAL_RLEBlitToSurface PAL_RLEBlitToSurfaceWithHeight PAL_RLEBlitToSurfaceWithShadow
PAL_RLEBlitWithColorShift PAL_RLEBlitMonoColor PAL_FBPBlitToSurface
PAL_FBPBlitChunkToSurface PAL_MakeScene'''.split()
    raw = out/'raw.wasm'
    run([linker,'--no-entry','--error-limit=0',f'--initial-memory={app.initial_memory}',
         f'--max-memory={app.max_memory}','-z',f'stack-size={app.stack}',
         '--Map='+str(out/'app.map')]+['--export='+e for e in exports]
        +['--wrap='+w for w in wraps]+objects+['-o',raw])
    # The transform resolves its files relative to this module. Use a relative
    # script argument so Windows node.exe can also be invoked from WSL.
    subprocess.run([args.node,'port/asyncify.mjs',os.path.relpath(out,ROOT)], cwd=ROOT, check=True)
    wasm = out/'app.wasm'
    manifest = out/'manifest.json'
    manifest.write_text(json.dumps(build.make_manifest(app),ensure_ascii=False),encoding='utf8')
    package = workspace/'dist'/args.target/'aster.pal.ast'
    package.parent.mkdir(parents=True,exist_ok=True)
    cmd = [sys.executable,SDK/'tools/pack_ast.py','--manifest',manifest,
           '--wasm',wasm,'-o',package]
    if app.icon:cmd += ['--icon',ROOT/app.icon]
    if not args.no_aot:
        aot = out/'app.aot'
        build.build_aot(app,wasm,aot,app.target['aot_target'])
        cmd += ['--aot',aot,'--aot-target',aster_board.pack_key_for(app.target['aot_target'])]
    run(cmd)
    run([sys.executable,SDK/'tools/verify_ast.py','--target',args.target,package])
    print(package)


if __name__ == '__main__':
    main()
