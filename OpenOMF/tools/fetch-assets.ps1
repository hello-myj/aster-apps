$ErrorActionPreference = 'Stop'
$Project = Split-Path -Parent $PSScriptRoot
$Raw = Join-Path $Project 'vendor/game-data/OMF2097/FIGHTR0.AF'
if (Test-Path $Raw) {Write-Host 'Original OMF data already present'; exit 0}
$Downloads = Join-Path $Project 'artifacts/downloads'
New-Item -ItemType Directory -Path $Downloads -Force | Out-Null
$Zip = Join-Path $Downloads 'openomf-assets.zip'
Invoke-WebRequest -Uri 'https://www.omf2097.com/pub/files/omf/openomf-assets.zip' -OutFile $Zip
Expand-Archive -LiteralPath $Zip -DestinationPath (Join-Path $Project 'vendor/game-data') -Force
if (!(Test-Path $Raw)) {throw 'Original data archive layout changed'}
