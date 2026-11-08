$ErrorActionPreference = 'Stop'
$taskRepo = Join-Path (Split-Path $PSScriptRoot -Parent) 'sdlpal-aster'
$taskWorkspace = Split-Path $taskRepo -Parent
. (Join-Path $taskWorkspace 'env.ps1')
& (Join-Path $taskWorkspace 'apply-patches.ps1')
. (Join-Path $PSScriptRoot 'paths.ps1')
$taskSdk = $env:ASTER_SDK
& (Join-Path $taskSdk 'runtime\python.exe') (Join-Path $PSScriptRoot 'localize-resources.py')
if($LASTEXITCODE){throw 'Resource localization failed'}
$taskClang = Join-Path $taskSdk 'toolchain\wasi-sdk-24.0-x86_64-windows\bin\clang.exe'
$taskOutput = Join-Path $PSScriptRoot 'build\objects'
$taskProfile=Get-Content (Join-Path $PSScriptRoot 'memory-profile.json') -Raw | ConvertFrom-Json
$taskResidentMask=0
foreach($taskArchiveId in $taskProfile.residentArchiveIds){
    if($taskArchiveId -lt 1 -or $taskArchiveId -gt 31){throw 'Invalid resident archive ID'}
    $taskResidentMask=$taskResidentMask -bor (1 -shl $taskArchiveId)
}
New-Item -ItemType Directory -Force (Join-Path $PSScriptRoot 'build\audit') | Out-Null
$taskAuditObjects=@()
foreach($taskAuditSource in @('aster-port/resource-audit.c','embedded/pal_pack.c','embedded/pal_level2_resident_pack.c')){
    $taskAuditObject=Join-Path $PSScriptRoot ('build\audit\'+($taskAuditSource -replace '[/\\]','_')+'.o')
    & $taskClang --target=wasm32-unknown-wasi -Os -ffreestanding -DPAL_ASTER_FONT16=1 ('-I'+(Join-Path $PSScriptRoot 'include')) ('-I'+(Join-Path $taskRepo 'embedded')) -c (Resolve-PalPath $taskAuditSource) -o $taskAuditObject
    if($LASTEXITCODE){exit $LASTEXITCODE}
    $taskAuditObjects+=$taskAuditObject
}
$taskAuditCrt=Join-Path $PSScriptRoot 'build\audit\crt.o'
& $taskClang --target=wasm32-unknown-wasi -Os -ffreestanding -DPAL_ASTER_FONT16=1 ('-I'+(Join-Path $taskSdk 'include')) ('-I'+(Join-Path $taskSdk 'lib\aster')) -c (Join-Path $taskSdk 'lib\aster\crt.c') -o $taskAuditCrt
if($LASTEXITCODE){exit $LASTEXITCODE}
& (Join-Path (Split-Path $taskClang -Parent) 'wasm-ld.exe') --no-entry --export=audit_resident --initial-memory=2359296 --max-memory=2359296 @taskAuditObjects $taskAuditCrt -o (Join-Path $PSScriptRoot 'build\resource-audit.wasm')
if($LASTEXITCODE){exit $LASTEXITCODE}
& node (Join-Path $PSScriptRoot 'audit-resident.mjs')
if($LASTEXITCODE){exit $LASTEXITCODE}
New-Item -ItemType Directory -Force $taskOutput | Out-Null
$taskSources = @('battle.c','ending.c','fight.c','game.c','global.c','input.c','itemmenu.c','magicmenu.c','main.c','map.c','overlay.c','palcfg.c','palcommon.c','paldebug.c','palette.c','play.c','res.c','rngplay.c','scene.c','script.c','ui.c','uibattle.c','uigame.c','util.c','video.c','sdl_compat/sdl_compat.c','unix/embedded_contract_stubs.c','embedded/pal_pack.c','embedded/pal_memory_profile.c','embedded/pal_text_cache.c','embedded/pal_font_cache.c','embedded/pal_font10_cache.c','embedded/pal_native_ui.c','embedded/pal_level2_resident_pack.c','esp32s3/native_engine_shim/sdl_shim.c','esp32s3/engine_bridge/pal_engine_pack_provider.c')
$taskDefines = @('PAL_HEADLESS_SDL_SHIM=1','PAL_HAS_JOYSTICKS=0','PAL_HAS_TOUCH=0','PAL_HAS_MP3=0','PAL_HAS_OGG=0','PAL_HAS_OPUS=0','PAL_HAS_NATIVEMIDI=0','PAL_HAS_GLSL=0','PAL_HAS_CONFIG_PAGE=0','PAL_NO_LAUNCH_UI=1','PAL_NO_RUNTIME_DECOMPRESS=1','PAL_NO_RUNTIME_HEAP=1','PAL_CONTRACT_NO_AUDIO=1','PAL_ESP_CORES3SE_NO_AUDIO=1','PAL_CONTRACT_TARGET_PACK_PROVIDER=1','PAL_ENGINE_BRIDGE_REQUIRE_TARGET_HOOKS=1','USE_RIX_EXTRA_INIT=0','USE_SDL3=0','MEM_LEVEL2=1','PAL_STORAGE_SD_ONLY=1','PAL_EXTREME_TWO_SCREENS=1','PAL_SDL_SHIM_EXTERNAL_SURFACES_ONLY=1','SHIM_MAX_SURFACES=8','SHIM_MAX_PALETTES=8','SHIM_MAX_TEXTURES=1','SHIM_SURFACE_PIXELS=65536u','main=PAL_EngineMain')
$taskIncludes = @('aster-port/include','esp32s3/engine_bridge','esp32s3/engine_bridge/include','esp32s3/native_engine_shim/include','.','embedded','sdl_compat')
$taskFlags = @('--target=wasm32-unknown-wasi','-std=c11','-Os','-ffreestanding','-fno-strict-aliasing','-Wno-incompatible-pointer-types')
$taskDefines += 'PAL_ASTER=1'
if($env:PAL_TRACE -eq '1'){$taskDefines += 'PAL_ASTER_TRACE=1'}
$taskDefines += 'PAL_ASTER_FONT16=1'
$taskDefines += 'PAL_ASTER_STACK_BYTES='+$taskProfile.stackBytes+'u'
$taskDefines += 'PAL_ASTER_RESIDENT_MASK='+$taskResidentMask+'u'
$taskDefines += 'PAL_MEM_LEVEL2_RESIDENT_PACK_BYTES='+$taskProfile.residentPackBytes+'u'
$taskFlags += '-I'+(Join-Path $taskSdk 'include')
$taskFlags += '-I'+(Join-Path $taskSdk 'lib\aster')
$taskSources += @('aster-port/platform.c','aster-port/libc.c','aster-port/debug.c','aster-port/audio.c')
foreach ($taskDefine in $taskDefines) { $taskFlags += '-D' + $taskDefine }
foreach ($taskInclude in $taskIncludes) { $taskFlags += '-I' + (Resolve-PalPath $taskInclude) }
$taskFailures = 0
$taskActiveObjects=@()
foreach ($taskSource in $taskSources) {
    $taskObject = Join-Path $taskOutput (($taskSource -replace '[/\\]','_') + '.o')
    & $taskClang @taskFlags -c (Resolve-PalPath $taskSource) -o $taskObject
    if ($LASTEXITCODE -ne 0) { $taskFailures++ }
    else{$taskActiveObjects+=$taskObject}
}
Write-Output "Engine compilation failures: $taskFailures / $($taskSources.Count)"
if ($taskFailures) { exit 1 }
Set-Content -LiteralPath (Join-Path $PSScriptRoot 'build\objects.list') -Value $taskActiveObjects -Encoding utf8
$taskSdkOutput=Join-Path $PSScriptRoot 'build\sdk'
New-Item -ItemType Directory -Force $taskSdkOutput | Out-Null
foreach($taskSdkSource in @('crt.c','aster_printf.c','aster_gfx.c')) {
    & $taskClang --target=wasm32-unknown-wasi -Os -ffreestanding -DPAL_ASTER_FONT16=1 ('-I'+(Join-Path $taskSdk 'include')) ('-I'+(Join-Path $taskSdk 'lib\aster')) -c (Join-Path $taskSdk ('lib\aster\'+$taskSdkSource)) -o (Join-Path $taskSdkOutput ($taskSdkSource.Replace('.c','.o')))
    if($LASTEXITCODE -ne 0){exit $LASTEXITCODE}
}



