$ErrorActionPreference='Stop'
$Workspace=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
if(-not $env:ASTER_SDK){$env:ASTER_SDK=Join-Path $Workspace 'aster-sdk'}
if(-not $env:ASTER_SIM){$env:ASTER_SIM=Join-Path $Workspace 'aster-sim'}
if(-not(Test-Path (Join-Path $env:ASTER_SDK 'include/aster_imports.h'))){throw 'Current Aster SDK not found; set ASTER_SDK'}
