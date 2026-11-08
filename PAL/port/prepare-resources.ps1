param([string]$DataDirectory)
$ErrorActionPreference='Stop'
$taskRepo=Join-Path (Split-Path $PSScriptRoot -Parent) 'sdlpal-aster'
if(-not $DataDirectory){$DataDirectory=Join-Path (Split-Path $taskRepo -Parent) 'pal-resources\pal'}
. (Join-Path (Split-Path $taskRepo -Parent) 'env.ps1')
$taskPython=Join-Path $env:ASTER_SDK 'runtime\python.exe'
if(-not(Test-Path $taskPython)){throw 'Python 3.10+ is required; update the taskPython path.'}
& $taskPython -B (Join-Path $taskRepo 'tools\pal_chapter_pack_build.py') $DataDirectory --out-dir (Join-Path $PSScriptRoot 'resources') --font10-archive (Join-Path $taskRepo 'third_party\fusion-pixel-font\fusion-pixel-font-10px-monospaced-bdf-v2026.07.20.zip')
if($LASTEXITCODE){exit $LASTEXITCODE}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'resources\pal_full.pak') -Destination (Join-Path $PSScriptRoot 'resources\pal_full.source.pak') -Force
& $taskPython (Join-Path $PSScriptRoot 'localize-resources.py')
exit $LASTEXITCODE
