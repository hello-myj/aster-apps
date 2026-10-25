# Official desktop reference only; this does not run an Aster APP.
$ErrorActionPreference = 'Stop'
$Project = Split-Path -Parent $PSScriptRoot
$Reference = Join-Path $Project 'artifacts/reference-0.8.6/openomf'
$Exe = Join-Path $Reference 'openomf.exe'
if (!(Test-Path -LiteralPath $Exe)) { & (Join-Path $PSScriptRoot 'prepare-reference.ps1') }
foreach ($Name in @('config','state')) { New-Item -ItemType Directory -Path (Join-Path $Reference $Name) -Force | Out-Null }
$Existing = Get-Process -Name openomf -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $Exe }
if ($Existing) { $Existing | Select-Object Id,ProcessName,MainWindowTitle; return }
$Previous = @{}
foreach ($Name in @('OPENOMF_CONFIG_PATH','OPENOMF_STATE_PATH','OPENOMF_RESOURCE_PATH')) { $Previous[$Name] = [Environment]::GetEnvironmentVariable($Name,'Process') }
try {
    $env:OPENOMF_CONFIG_PATH = './config'
    $env:OPENOMF_STATE_PATH = './state'
    $env:OPENOMF_RESOURCE_PATH = '.'
    $Game = Start-Process -FilePath $Exe -WorkingDirectory $Reference -WindowStyle Normal -PassThru
    $Game.Id | Set-Content -LiteralPath (Join-Path $Project 'artifacts/reference-process.txt')
    Write-Output "Official desktop reference started: PID $($Game.Id)"
} finally {
    foreach ($Name in $Previous.Keys) { [Environment]::SetEnvironmentVariable($Name,$Previous[$Name],'Process') }
}
