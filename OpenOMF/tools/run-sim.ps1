param([int]$Port = 8848)
$ErrorActionPreference = 'Stop'
$Project = Split-Path -Parent $PSScriptRoot
$SDK = if ($env:ASTER_SDK) {$env:ASTER_SDK} else {Join-Path (Split-Path -Parent (Split-Path -Parent $Project)) 'aster-sdk'}
$Node = if ($env:OPENOMF_NODE) {$env:OPENOMF_NODE} else {'node'}
$Package = Join-Path $Project 'dist/xtensa/aster.openomf.ast'
if (!(Test-Path $Package)) {throw 'Run tools/build.ps1 first'}
& $Node "$SDK/simulator/src/cli.js" $Package --resolution 480x320 --state "$Project/sdcard/aster/apps" --port $Port @args
