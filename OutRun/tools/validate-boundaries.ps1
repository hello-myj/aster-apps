$ErrorActionPreference='Stop'
$env:PYTHONUTF8='1'
$Project=Split-Path -Parent $PSScriptRoot
$SDK='E:\个人项目资料\aster\aster-sdk'
$Fixture=Join-Path $Project 'artifacts\boundary-check'
New-Item -ItemType Directory -Force "$Fixture\src\port" | Out-Null
Copy-Item "$Project\src\port\*" "$Fixture\src\port" -Force
Copy-Item "$Project\tools\fixtures\boundary-main.c" "$Fixture\src\main.c" -Force
Copy-Item "$Project\app.toml" "$Fixture\app.toml" -Force
& "$SDK\aster.exe" build $Fixture --target xtensa --no-aot
if($LASTEXITCODE -ne 0){throw 'Boundary fixture build failed'}
node "$Project\tools\validate-boundaries.js"
if($LASTEXITCODE -ne 0){throw 'Boundary verification failed'}
