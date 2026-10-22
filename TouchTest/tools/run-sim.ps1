$ErrorActionPreference = 'Stop'
$Project = Split-Path -Parent $PSScriptRoot
$SDK = Join-Path (Split-Path -Parent (Split-Path -Parent $Project)) 'aster-sdk'
Push-Location "$SDK\simulator"
try {
 node src\cli.js "$Project\dist\xtensa\aster.touchtest.ast" --resolution 480x320 @args
 if ($LASTEXITCODE -ne 0) { throw 'Simulator failed' }
} finally { Pop-Location }