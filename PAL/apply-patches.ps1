$ErrorActionPreference='Stop'
$taskEngine=Join-Path $PSScriptRoot 'sdlpal-aster'
if(-not(Test-Path -LiteralPath (Join-Path $taskEngine '.git'))){throw 'Initialize submodules: git submodule update --init --recursive'}
foreach($taskPatch in (Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'patches') -Filter '*.patch' | Sort-Object Name)){
    $ErrorActionPreference='Continue'
    & git -C $taskEngine apply --reverse --check $taskPatch.FullName 2>$null
    $ErrorActionPreference='Stop'
    if($LASTEXITCODE -eq 0){continue}
    & git -C $taskEngine apply --check $taskPatch.FullName
    if($LASTEXITCODE){throw "Patch conflicts with local code: $($taskPatch.Name). No files overwritten."}
    & git -C $taskEngine apply $taskPatch.FullName
    if($LASTEXITCODE){throw "Patch failed: $($taskPatch.Name)"}
}
