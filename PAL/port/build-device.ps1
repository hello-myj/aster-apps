param([switch]$SkipCompile)
$ErrorActionPreference='Stop'
$taskWorkspace=Split-Path $PSScriptRoot -Parent
. (Join-Path $taskWorkspace 'env.ps1')
if(-not $SkipCompile){
    & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'compile-engine.ps1')
    if($LASTEXITCODE){throw 'Engine compilation failed'}
    & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'link.ps1')
    if($LASTEXITCODE){throw 'Engine link failed'}
    & node (Join-Path $PSScriptRoot 'asyncify.mjs')
    if($LASTEXITCODE){throw 'Asyncify failed'}
}
$taskWasm=Join-Path $PSScriptRoot 'build\app.wasm'
$taskAot=Join-Path $PSScriptRoot 'build\app.aot'
& (Join-Path $env:ASTER_SDK 'toolchain\wamrc.exe') --target=xtensa --target-abi=gnu --cpu=esp32s3 --opt-level=3 --size-level=0 -o $taskAot $taskWasm
if($LASTEXITCODE){throw 'Xtensa AOT compilation failed'}
$taskProfile=Get-Content (Join-Path $PSScriptRoot 'memory-profile.json') -Raw | ConvertFrom-Json
$taskManifest=[ordered]@{
    id='com.example.palresearch';name='仙剑奇侠传';version='0.11.1'
    abi=@{min='3.7';target='3.7.0'};product='aster';target_id='xtensa'
    orientation='landscape';category='game';requires=@('display','touch');optional=@('audio-out');permissions=@('fs')
    memory=@{initial=$taskProfile.linearBytes;max=$taskProfile.linearBytes;stack=$taskProfile.stackBytes;heap=$taskProfile.surfaceHeapBytes}
    wasm_sha256=(Get-FileHash -LiteralPath $taskWasm -Algorithm SHA256).Hash.ToLowerInvariant()
}
$taskManifestPath=Join-Path $PSScriptRoot 'build\device-manifest.json'
[IO.File]::WriteAllText($taskManifestPath,($taskManifest | ConvertTo-Json -Depth 5),[Text.UTF8Encoding]::new($false))
$taskPackage=Join-Path $PSScriptRoot 'build\pal-device.ast'
& (Join-Path $env:ASTER_SDK 'runtime\python.exe') (Join-Path $env:ASTER_SDK 'tools\pack_ast.py') --manifest $taskManifestPath --wasm $taskWasm --aot $taskAot --aot-target xtensa-esp32s3 -o $taskPackage
if($LASTEXITCODE){throw 'SDK package creation failed'}
& aster verify $taskPackage --target xtensa
if($LASTEXITCODE){throw 'SDK verification failed'}
Get-Item -LiteralPath $taskWasm,$taskAot,$taskPackage | Select-Object Name,Length
