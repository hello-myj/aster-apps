param([ValidateSet('xtensa','riscv')][string]$Target='xtensa',[switch]$SimulatorOnly)
$ErrorActionPreference='Stop'
$env:PYTHONUTF8='1'
$Project=Split-Path -Parent $PSScriptRoot
$SDK=if($env:ASTER_SDK){$env:ASTER_SDK}else{Join-Path (Split-Path -Parent (Split-Path -Parent $Project)) 'aster-sdk'}
$Python=if($env:GOLDMINER_PYTHON){$env:GOLDMINER_PYTHON}else{'python'}
$Mode=if($SimulatorOnly){@('--no-aot')}else{@()}
& $Python "$PSScriptRoot/build.py" --target $Target @Mode
if($LASTEXITCODE -ne 0){throw 'Build failed'}
