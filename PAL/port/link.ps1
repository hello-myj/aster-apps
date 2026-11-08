$ErrorActionPreference='Stop'
$taskRepo=Join-Path (Split-Path $PSScriptRoot -Parent) 'sdlpal-aster'
$taskWorkspace=Split-Path $taskRepo -Parent
. (Join-Path $taskWorkspace 'env.ps1')
$taskSdk=$env:ASTER_SDK
$taskLinker=Join-Path $taskSdk 'toolchain\wasi-sdk-24.0-x86_64-windows\bin\wasm-ld.exe'
$taskObjects=Get-Content (Join-Path $PSScriptRoot 'build\objects.list')
$taskSdkObjects=Join-Path $PSScriptRoot 'build\sdk'
$taskExtraExports=@('--export=PAL_IsInDialog','--export=pal_debug_main_menu','--export=pal_debug_system_menu','--export=pal_debug_viewport','--export=pal_debug_scene')
$taskProfile=Get-Content (Join-Path $PSScriptRoot 'memory-profile.json') -Raw | ConvertFrom-Json
$taskExtraExports+='--export=aster_app_start'
$taskFlags=@('--no-entry','--error-limit=0','--export=aster_app_init','--export=aster_app_event','--export=aster_app_stop','--export=aster_app_destroy','--export=__heap_base','--export=pal_async_start_unwind','--export=pal_async_start_rewind','--export=pal_async_stop_unwind','--export=pal_async_stop_rewind','--export=pal_async_get_state','--export=PAL_EngineMain',('--initial-memory='+$taskProfile.linearBytes),('--max-memory='+$taskProfile.linearBytes),'-z',('stack-size='+$taskProfile.stackBytes),('--Map='+ (Join-Path $PSScriptRoot 'build\app.map')))
foreach($taskWrap in @('PAL_MKFOpenPackArchive','PAL_MKFIsPackArchive','PAL_MKFMapChunk','PAL_MKFGetChunkCount','PAL_MKFGetChunkSize','PAL_MKFReadChunk','fclose')){$taskFlags+='--wrap='+$taskWrap}
& $taskLinker @taskFlags @taskExtraExports --export=aster_app_pause --export=aster_app_resume @taskObjects (Join-Path $taskSdkObjects 'crt.o') (Join-Path $taskSdkObjects 'aster_printf.o') (Join-Path $taskSdkObjects 'aster_gfx.o') -o (Join-Path $PSScriptRoot 'build\raw.wasm')
exit $LASTEXITCODE
