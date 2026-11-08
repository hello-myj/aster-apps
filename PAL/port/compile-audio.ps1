$ErrorActionPreference='Stop'
$taskRepo=Join-Path (Split-Path $PSScriptRoot -Parent) 'sdlpal-aster'
. (Join-Path (Split-Path $taskRepo -Parent) 'env.ps1')
. (Join-Path $PSScriptRoot 'paths.ps1')
$taskSdk=$env:ASTER_SDK
$taskBin=Join-Path $taskSdk 'toolchain\wasi-sdk-24.0-x86_64-windows\bin'
$taskOutput=Join-Path $PSScriptRoot 'build\audio'
New-Item -ItemType Directory -Force $taskOutput | Out-Null
$taskFlags=@('--target=wasm32-unknown-wasi','-std=c++17','-Os','-ffreestanding','-fno-exceptions','-fno-rtti','-fno-threadsafe-statics','-DPAL_NO_RUNTIME_HEAP=1','-DUSE_RIX_EXTRA_INIT=0','-DPAL_HAS_MP3=0','-DPAL_HAS_OGG=0','-DPAL_HAS_OPUS=0','-DPAL_HAS_NATIVEMIDI=0','-DPAL_HAS_GLSL=0','-DPAL_HAS_CONFIG_PAGE=0','-DPAL_NO_LAUNCH_UI=1')
foreach($taskInclude in @('aster-port/audio-tools/include','aster-port/include','esp32s3/native_engine_shim/include','esp32s3/engine_bridge/include','sdl_compat','.')){$taskFlags+='-I'+(Resolve-PalPath $taskInclude)}
$taskFlags+=@('-include',(Join-Path $PSScriptRoot 'include\pal_libc.h'))
foreach($taskSource in @('adplug/player.cpp','adplug/rix.cpp','embedded/pal_mame_opl2_static.cpp','aster-port/audio-tools/render.cpp')){
 $taskObject=Join-Path $taskOutput (($taskSource -replace '[/\\]','_')+'.o')
 & (Join-Path $taskBin 'clang.exe') @taskFlags -c (Resolve-PalPath $taskSource) -o $taskObject
 if($LASTEXITCODE){exit $LASTEXITCODE}
}
$taskObjects=(Get-ChildItem (Join-Path $taskOutput '*.o')).FullName
$taskObjects=@($taskObjects | Where-Object { (Split-Path $_ -Leaf) -ne 'crt.o' })
& (Join-Path $taskBin 'clang.exe') --target=wasm32-unknown-wasi -Os -ffreestanding ('-I'+(Join-Path $taskSdk 'include')) ('-I'+(Join-Path $taskSdk 'lib\aster')) -c (Join-Path $taskSdk 'lib\aster\crt.c') -o (Join-Path $taskOutput 'crt.o')
if($LASTEXITCODE){exit $LASTEXITCODE}
& (Join-Path $taskBin 'wasm-ld.exe') --no-entry --export=__wasm_call_ctors --export=pal_rix_input --export=pal_rix_output --export=pal_rix_load --export=pal_rix_tick --initial-memory=262144 --max-memory=262144 @taskObjects (Join-Path $taskOutput 'crt.o') -o (Join-Path $taskOutput 'rix-render.wasm')
exit $LASTEXITCODE
