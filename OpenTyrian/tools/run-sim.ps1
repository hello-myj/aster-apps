param([int]$Port=8851)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$sdkRoot=Join-Path (Split-Path (Split-Path $projectRoot -Parent) -Parent) 'aster-sdk'
& node "$sdkRoot/simulator/src/cli.js" "$projectRoot/dist/xtensa/aster.opentyrian.ast" --resolution 480x320 --state "$projectRoot/sdcard/aster/apps" --port $Port
