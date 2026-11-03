param([int]$Port=8850,[ValidateSet('xtensa','riscv')][string]$Target='xtensa')
$ErrorActionPreference='Stop'
$Project=Split-Path -Parent $PSScriptRoot
$SDK=if($env:ASTER_SDK){$env:ASTER_SDK}else{Join-Path (Split-Path -Parent (Split-Path -Parent $Project)) 'aster-sdk'}
$Node=if($env:GOLDMINER_NODE){$env:GOLDMINER_NODE}else{'node'}
$Simulator=Join-Path (Split-Path -Parent (Split-Path -Parent $Project)) 'aster-sim'
$Package=Join-Path $Project "build/sim/standalone/$Target/aster.goldminer.ast"
if(!(Test-Path -LiteralPath $Package)){throw 'Run tools/build.ps1 -SimulatorOnly with the same Target first'}
$Resolution=if($Target -eq 'riscv'){'480x480'}else{'320x240'}
& $Node "$Simulator/src/cli.js" $Package --resolution $Resolution --port $Port @args
