param([ValidateSet('riscv','xtensa')][string]$Target='riscv',[switch]$NoAot)
$ErrorActionPreference='Stop'
$Python=if($env:PAL_PYTHON){$env:PAL_PYTHON}else{'python'}
$BuildArgs=@((Join-Path $PSScriptRoot 'tools/build.py'),'--target',$Target)
if($NoAot){$BuildArgs+='--no-aot'}
& $Python @BuildArgs
if($LASTEXITCODE){exit $LASTEXITCODE}
& $Python (Join-Path $PSScriptRoot 'tools/prepare_store.py')
exit $LASTEXITCODE
