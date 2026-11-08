$ErrorActionPreference='Stop'
. (Join-Path (Split-Path $PSScriptRoot -Parent) 'env.ps1')
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'compile-audio.ps1')
if($LASTEXITCODE){exit $LASTEXITCODE}
& node (Join-Path $PSScriptRoot 'prepare-audio.mjs')
exit $LASTEXITCODE
