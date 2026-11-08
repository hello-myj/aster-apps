# Keep Aster-owned files outside the upstream submodule.
function Resolve-PalPath([string]$Relative){
    if($Relative.StartsWith('aster-port/')){return Join-Path $PSScriptRoot $Relative.Substring(11)}
    return Join-Path $taskRepo $Relative
}
