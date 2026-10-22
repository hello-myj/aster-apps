$ErrorActionPreference = 'Stop'
$Project = Split-Path -Parent $PSScriptRoot
$SDK = 'E:\个人项目资料\aster\aster-sdk'
$Package = Join-Path $Project 'dist\xtensa\aster.outrun.ast'
$State = Join-Path $Project 'sdcard\aster\apps'
if (!(Test-Path $Package)) { throw "Build package first: $Package" }
if (!(Test-Path (Join-Path $State 'aster.outrun\data\resources\map01.bin'))) { throw 'Run tools/build.ps1 to prepare SD resources.' }
Push-Location (Join-Path $SDK 'simulator')
try {
 node src\cli.js $Package --resolution 480x320 --state $State @args
 if ($LASTEXITCODE -ne 0) { throw "Simulator failed: $LASTEXITCODE" }
} finally { Pop-Location }
