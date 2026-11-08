param([string]$Port='COM10',[switch]$OnlyGameResources)
$ErrorActionPreference='Stop'
$taskWorkspace=Split-Path $PSScriptRoot -Parent
. (Join-Path $taskWorkspace 'env.ps1')
$taskAster=Join-Path $env:ASTER_SDK 'aster.exe'
$taskBuild=Join-Path $PSScriptRoot 'build'
$taskProgress=Join-Path $taskBuild 'device-transfer-progress.json'
$taskJournal=Join-Path $taskBuild 'device-transfer-journal.json'
$taskLog=Join-Path $PSScriptRoot 'device-transfer.log'
$taskQueue=@()
$taskPartManifest=Get-Content (Join-Path $taskBuild 'device-parts\parts.json') -Raw | ConvertFrom-Json
foreach($taskPart in $taskPartManifest.parts){
    $taskQueue+= [pscustomobject]@{local=(Join-Path $taskBuild ('device-parts\'+$taskPart.name));remote=('/SD:/music/pal/resources/'+$taskPart.name);size=[long]$taskPart.size;sha256=$taskPart.sha256;stage='game'}
}
$taskMusicRoot=Join-Path $PSScriptRoot 'sim-state\shared\music\pal'
if(-not $OnlyGameResources){
foreach($taskFile in (Get-ChildItem -LiteralPath $taskMusicRoot -File -Recurse | Sort-Object FullName)){
    $taskRelative=$taskFile.FullName.Substring($taskMusicRoot.Length+1).Replace('\','/')
    $taskQueue+=[pscustomobject]@{local=$taskFile.FullName;remote=('/SD:/music/pal/'+$taskRelative);size=[long]$taskFile.Length;sha256=(Get-FileHash -LiteralPath $taskFile.FullName -Algorithm SHA256).Hash.ToLowerInvariant();stage='audio'}
}
}
$taskTotalBytes=[long](($taskQueue | Measure-Object size -Sum).Sum)
$taskDone=@()
if(Test-Path -LiteralPath $taskJournal){$taskDone=@(Get-Content -LiteralPath $taskJournal -Raw | ConvertFrom-Json)}
$taskCompletedBytes=[long]0
$taskCompletedFiles=0
$taskWatch=[Diagnostics.Stopwatch]::StartNew()
function Write-TransferState($status,$file,$current,$message){
    $taskSent=$taskCompletedBytes+[long]$current
    $taskState=[ordered]@{status=$status;port=$Port;completedFiles=$taskCompletedFiles;totalFiles=$taskQueue.Count;completedBytes=$taskCompletedBytes;currentBytes=[long]$current;totalBytes=$taskTotalBytes;percent=[math]::Round(100.0*$taskSent/$taskTotalBytes,2);currentFile=$file;elapsedSeconds=[math]::Round($taskWatch.Elapsed.TotalSeconds,1);message=$message}
    $taskState | ConvertTo-Json | Set-Content -LiteralPath $taskProgress -Encoding utf8
}
foreach($taskDir in @('/SD:/music/pal','/SD:/music/pal/resources','/SD:/music/pal/sfx')){
    & $taskAster shell --console $Port aster fs mkdir $taskDir | Add-Content -LiteralPath $taskLog
}
try{
    foreach($taskItem in $taskQueue){
        $taskPrior=$taskDone | Where-Object { $_.remote -eq $taskItem.remote -and $_.sha256 -eq $taskItem.sha256 } | Select-Object -First 1
        if($taskPrior){$taskCompletedBytes+=$taskItem.size;$taskCompletedFiles++;continue}
        Write-TransferState 'transferring' $taskItem.remote 0 ''
        Write-Output ("START {0}/{1} {2}" -f ($taskCompletedFiles+1),$taskQueue.Count,$taskItem.remote)
        # SDK progress is emitted on stderr; PowerShell 5 wraps those lines as
        # NativeCommandError records. Only the actual process exit is failure.
        $ErrorActionPreference='Continue'
        & $taskAster push $taskItem.local $taskItem.remote --port $Port --target xtensa 2>&1 | ForEach-Object {
            $taskLine=$_.ToString()
            Add-Content -LiteralPath $taskLog -Value $taskLine
            if($taskLine -match '^\s*(\d+)/(\d+)'){
                Write-TransferState 'transferring' $taskItem.remote ([long]$Matches[1]) ''
            }
        }
        $ErrorActionPreference='Stop'
        if($LASTEXITCODE -ne 0){throw "Transfer failed: $($taskItem.remote), exit $LASTEXITCODE"}
        $taskDone+=[pscustomobject]@{remote=$taskItem.remote;sha256=$taskItem.sha256;size=$taskItem.size}
        ConvertTo-Json -InputObject @($taskDone) | Set-Content -LiteralPath $taskJournal -Encoding utf8
        $taskCompletedBytes+=$taskItem.size;$taskCompletedFiles++
        Write-TransferState 'transferring' $taskItem.remote 0 ''
        Write-Output ("DONE {0}/{1} {2:N2}% ({3:N1}/{4:N1} MiB)" -f $taskCompletedFiles,$taskQueue.Count,(100.0*$taskCompletedBytes/$taskTotalBytes),($taskCompletedBytes/1MB),($taskTotalBytes/1MB))
    }
    Write-TransferState 'complete' '' 0 'All files transferred and checked by SDK CRC.'
}catch{
    Write-TransferState 'failed' $taskItem.remote 0 $_.Exception.Message
    throw
}
