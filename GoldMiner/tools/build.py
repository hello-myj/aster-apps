"""Build shared game sources with per-CPU memory and original-image resources."""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

P=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--target',choices=['riscv','xtensa'],required=True)
p.add_argument('--no-aot',action='store_true')
a=p.parse_args()
sdk=Path(os.environ.get('ASTER_SDK',P.parents[1]/'aster-sdk'))
scale=1.5 if a.target=='riscv' else 1.0
generated=P/'build/resources'/a.target/'assets'
subprocess.run([sys.executable,str(P/'tools/prepare.py'),'--scale',str(scale),'--output',str(generated)],check=True)
configured=P/'build/configured'/a.target
configured.mkdir(parents=True,exist_ok=True)
shutil.copytree(P/'port',configured/'port',dirs_exist_ok=True)
(configured/'assets').mkdir(exist_ok=True)
shutil.copyfile(generated/'icon.bin',configured/'assets/icon.bin')
manifest=(P/'app.toml').read_text(encoding='utf-8-sig')
manifest=manifest.replace('artifacts/deps',(P/'artifacts/deps').as_posix())
if a.target=='riscv':
    for key,value in [('initial_memory',3145728),('max_memory',4194304),('heap',786432)]:
        manifest=re.sub(r'^'+key+r' = \d+',key+' = '+str(value),manifest,flags=re.M)
    manifest=manifest.replace('OMF_HEAP_BYTES=1048576','OMF_HEAP_BYTES=2097152')
    manifest=manifest.replace('GOLDMINER_CACHE_BYTES=786432','GOLDMINER_CACHE_BYTES=1572864')
(configured/'app.toml').write_bytes(manifest.encode('utf-8'))
workspace=P/'build/sim' if a.no_aot else P
env=os.environ|{'ASTER_WORKSPACE':str(workspace)}
subprocess.run([sys.executable,str(sdk/'tools/build.py'),str(configured),'--target',a.target,
                '--no-aot' if a.no_aot else '--aot'],env=env,check=True)
# Each native release keeps its own resource tree. Building S3 must not mutate
# the resources paired with a previously built P4 program.
resources=P/'dist'/a.target/'resources/assets'
resources.mkdir(parents=True,exist_ok=True)
shutil.copyfile(generated/'game.gma',resources/'game.gma')
shutil.copytree(generated/'audio',resources/'audio',dirs_exist_ok=True)
print('Resource scale:',scale,'Resources:',resources)
if a.no_aot:
    standalone=workspace/'standalone'/a.target/'aster.goldminer.ast'
    standalone.parent.mkdir(parents=True,exist_ok=True)
    compiled=workspace/'build'/a.target/'aster.goldminer'
    command=[sys.executable,str(sdk/'tools/pack_ast.py'),'--manifest',str(compiled/'manifest.json'),
             '--wasm',str(compiled/'app.wasm'),'--icon',str(generated/'icon.bin'),'-o',str(standalone)]
    for file in sorted(resources.rglob('*')):
        if file.is_file():command.extend(['--asset',file.relative_to(resources.parent).as_posix()+'='+str(file)])
    subprocess.run(command,check=True)
    print('Standalone simulator package (not for Store publication):',standalone)
