param([switch]$Aot, [switch]$SimulatorOnly)
$ErrorActionPreference = 'Stop'
$env:PYTHONUTF8 = '1'
$Project = Split-Path -Parent $PSScriptRoot
$SDK = if ($env:ASTER_SDK) {$env:ASTER_SDK} else {Join-Path (Split-Path -Parent (Split-Path -Parent $Project)) 'aster-sdk'}
$Python = if ($env:OPENOMF_PYTHON) {$env:OPENOMF_PYTHON} else {'python'}
& $Python "$PSScriptRoot/prepare-engine-deps.py" --sdk $SDK
if ($LASTEXITCODE -ne 0) {throw 'Engine dependency preparation failed'}
& $Python "$PSScriptRoot/prepare-engine-assets.py" --app-id aster.openomf
if ($LASTEXITCODE -ne 0) {throw 'Original resource preparation failed'}
if ($Aot -and $SimulatorOnly) {throw 'Choose AOT or SimulatorOnly, not both'}
$Mode = if ($SimulatorOnly) {'--no-aot'} else {'--aot'}
& "$SDK/aster.exe" build $Project --target xtensa $Mode
if ($LASTEXITCODE -ne 0) {throw 'Original engine build failed'}
& "$SDK/aster.exe" verify "$Project/dist/xtensa/aster.openomf.ast" --target xtensa
if ($LASTEXITCODE -ne 0) {throw 'Original engine verification failed'}
