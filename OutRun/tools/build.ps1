$ErrorActionPreference = 'Stop'
$env:PYTHONUTF8 = '1'
$Project = Split-Path -Parent $PSScriptRoot
$SDK = 'E:\个人项目资料\aster\aster-sdk'
python "$Project\tools\prepare_assets.py"
if ($LASTEXITCODE -ne 0) { throw "Asset conversion failed: $LASTEXITCODE" }
python "$Project\tools\prepare_audio.py"
if ($LASTEXITCODE -ne 0) { throw "Continuous audio preparation failed: $LASTEXITCODE" }
& "$SDK\aster.exe" build "$Project" --target xtensa --aot
if ($LASTEXITCODE -ne 0) { throw "Aster AOT build failed: $LASTEXITCODE" }
& "$SDK\aster.exe" verify "$Project\dist\xtensa\aster.outrun.ast" --target xtensa
if ($LASTEXITCODE -ne 0) { throw "Package verification failed: $LASTEXITCODE" }