param([int]$Port = 8846)
$ErrorActionPreference = 'Stop'
$Project = Split-Path -Parent $PSScriptRoot
$SDK = if ($env:ASTER_SDK) {$env:ASTER_SDK} else {Join-Path (Split-Path -Parent (Split-Path -Parent $Project)) 'aster-sdk'}
$Package = Join-Path $Project 'dist\xtensa\aster.pixelwheels.ast'
if (!(Test-Path $Package)) {throw 'Run tools/build.ps1 first'}
$Node = if ($env:PIXELWHEELS_NODE) {$env:PIXELWHEELS_NODE} else {'node'}
& $Node "$SDK\simulator\src\cli.js" $Package --resolution 480x320 --state "$Project\sdcard\aster\apps" --port $Port @args