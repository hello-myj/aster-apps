# Prepare the unmodified official desktop reference in this project.
$ErrorActionPreference = 'Stop'
$Project = Split-Path -Parent $PSScriptRoot
$Archive = Join-Path $Project 'artifacts/downloads/openomf_0.8.6_windows.zip'
$Reference = Join-Path $Project 'artifacts/reference-0.8.6'
$Expected = '16e737154b61683d9674de978f48f13e493c6b590ab7237269490d31b6a2fb13'
New-Item -ItemType Directory -Path (Split-Path -Parent $Archive) -Force | Out-Null
if (!(Test-Path -LiteralPath $Archive)) {
    Invoke-WebRequest -Uri 'https://github.com/omf2097/openomf/releases/download/0.8.6/openomf_0.8.6_windows.zip' -OutFile $Archive
}
if ((Get-FileHash -LiteralPath $Archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $Expected) {
    throw 'Official release SHA256 mismatch; reference will not be launched.'
}
New-Item -ItemType Directory -Path $Reference -Force | Out-Null
Expand-Archive -LiteralPath $Archive -DestinationPath $Reference -Force
Write-Output 'Official OpenOMF 0.8.6 desktop reference prepared and verified.'
