$ErrorActionPreference = 'Stop'
$env:PYTHONUTF8 = '1'
$Project = Split-Path -Parent $PSScriptRoot
$SDK = if ($env:ASTER_SDK) {$env:ASTER_SDK} else {Join-Path (Split-Path -Parent (Split-Path -Parent $Project)) 'aster-sdk'}
$Python = if ($env:PIXELWHEELS_PYTHON) {$env:PIXELWHEELS_PYTHON} else {'python'}
& $Python "$Project\tools\prepare_assets.py"
if ($LASTEXITCODE -ne 0) {throw 'Asset conversion failed; install tools/requirements.txt'}
& "$SDK\aster.exe" build $Project --target xtensa --aot
if ($LASTEXITCODE -ne 0) {throw 'Build failed'}
& "$SDK\aster.exe" verify "$Project\dist\xtensa\aster.pixelwheels.ast" --target xtensa
if ($LASTEXITCODE -ne 0) {throw 'Package verification failed'}