$ErrorActionPreference = 'Stop'
$Project = Split-Path -Parent $PSScriptRoot
$SDK = Join-Path (Split-Path -Parent (Split-Path -Parent $Project)) 'aster-sdk'
& "$SDK\aster.exe" build $Project --target xtensa --aot
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
& "$SDK\aster.exe" verify "$Project\dist\xtensa\aster.touchtest.ast" --target xtensa
if ($LASTEXITCODE -ne 0) { throw 'Package verification failed' }