param([switch]$SimulatorOnly)
$ErrorActionPreference='Stop'
$env:PYTHONUTF8='1'
$Project=Split-Path -Parent $PSScriptRoot
$SDK=if($env:ASTER_SDK){$env:ASTER_SDK}else{Join-Path (Split-Path -Parent (Split-Path -Parent $Project)) 'aster-sdk'}
$Python=if($env:GOLDMINER_PYTHON){$env:GOLDMINER_PYTHON}else{'python'}
& $Python "$PSScriptRoot/prepare.py"
if($LASTEXITCODE -ne 0){throw 'Resource preparation failed'}
$Mode=if($SimulatorOnly){'--no-aot'}else{'--aot'}
& "$SDK/aster.exe" build $Project --target xtensa $Mode
if($LASTEXITCODE -ne 0){throw 'Build failed'}
& "$SDK/aster.exe" verify "$Project/dist/xtensa/aster.goldminer.ast" --target xtensa
if($LASTEXITCODE -ne 0){throw 'Package verification failed'}
