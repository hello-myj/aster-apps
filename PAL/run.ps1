param([string]$Resolution='480x480',[int]$Port=8858)
$ErrorActionPreference='Stop'
$Sim=if($env:ASTER_SIM){$env:ASTER_SIM}else{Join-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) 'aster-sim'}
$Node=if($env:ASTER_NODE){$env:ASTER_NODE}else{'node'}
& $Node --require (Join-Path $PSScriptRoot 'tools/sim-assets.cjs') (Join-Path $Sim 'src/cli.js') (Join-Path $PSScriptRoot 'build/sim/dist/riscv/aster.pal.ast') --resolution $Resolution --state (Join-Path $PSScriptRoot 'build/player-state') --port $Port @args
exit $LASTEXITCODE
