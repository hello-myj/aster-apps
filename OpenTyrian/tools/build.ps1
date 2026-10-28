param([string]$Python='python')
$ErrorActionPreference='Stop'
$env:PYTHONUTF8='1'
& $Python "$PSScriptRoot/build.py"
if ($LASTEXITCODE -ne 0) { throw 'OpenTyrian build failed' }
