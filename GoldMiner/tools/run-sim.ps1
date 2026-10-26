param([int]$Port=8850)
$ErrorActionPreference='Stop'
$Project=Split-Path -Parent $PSScriptRoot
$SDK=if($env:ASTER_SDK){$env:ASTER_SDK}else{Join-Path (Split-Path -Parent (Split-Path -Parent $Project)) 'aster-sdk'}
$Node=if($env:GOLDMINER_NODE){$env:GOLDMINER_NODE}else{'node'}
& $Node "$SDK/simulator/src/cli.js" "$Project/dist/xtensa/aster.goldminer.ast" --resolution 480x320 --state "$Project/sdcard/aster/apps" --port $Port @args
