#!/usr/bin/env python3
"""SDK compilation -> internal Asyncify lowering -> normal SDK pack/verify."""
import argparse,os,pathlib,re,shutil,subprocess,sys
ROOT=pathlib.Path(__file__).resolve().parents[1]
SDK=ROOT.parent.parent/'aster-sdk'
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--target',choices=['xtensa','riscv'],default='xtensa')
p.add_argument('--aot',action='store_true')
args=p.parse_args()
workspace=pathlib.Path(os.environ.get('ASTER_WORKSPACE',ROOT))
env=dict(os.environ,PYTHONUTF8='1',ASTER_WORKSPACE=str(workspace))
base=workspace/'build'/args.target/'aster.opentyrian'
base.mkdir(parents=True,exist_ok=True)
log=ROOT/'artifacts/build.log'
with log.open('w',encoding='utf-8') as f:
    # Preserve coroutine boundaries until Asyncify has lowered the controls.
    # Generic Binaryen inlining can merge the engine into the uninstrumented
    # lifecycle callback, losing suspended locals and corrupting open FILEs.
    compiler="import sys;sys.path.insert(0,sys.argv[1]);import build;build.opt_wasm=lambda _:None;raise SystemExit(build.main(sys.argv[2:]))"
    result=subprocess.run([sys.executable,'-c',compiler,str(SDK/'tools'),str(ROOT),'--target',args.target,'--no-aot'],cwd=ROOT,env=env,stdout=f,stderr=subprocess.STDOUT)
if result.returncode:
    text=log.read_text(encoding='utf-8',errors='replace')
    if 'imports not provided by firmware' not in text or not (base/'app.wasm').exists():
        print(text[-12000:]);raise SystemExit(result.returncode)
wasm=base/'app.async.wasm'
BIN=ROOT/'artifacts/deps/binaryen-version_133/bin'
def binary(name):
    if os.environ.get('BINARYEN_BIN'):return str(pathlib.Path(os.environ['BINARYEN_BIN'])/(name+('.exe' if os.name=='nt' else '')))
    bundled=BIN/(name+('.exe' if os.name=='nt' else ''))
    return str(bundled) if bundled.is_file() else shutil.which(name) or name
wat=base/'app.internal.wat'
subprocess.run([binary('wasm-dis'),str(base/'app.wasm'),'-o',str(wat)],check=True)
text=wat.read_text(encoding='utf-8')
stack=re.search(r'\(export "__stack_pointer" \(global (\$[^\s()]+)\)\)',text)
if not stack:raise RuntimeError('LLVM stack global export missing')
global_name=stack[1]
helpers=[]
for name,body,signature in [
 ('stack_get',f'(global.get {global_name})',r'\(result i32\)'),
 ('stack_set',f'(global.set {global_name} (local.get 0))',r'\(param i32\)')]:
    pattern=r'\(import "tyrian" "'+name+r'" \(func (\$[^\s()]+) ('+signature+r')\)\)'
    def lower(m):
        helpers.append(f'(func {m[1]} {m[2]} {body})')
        return ''
    text,n=re.subn(pattern,lower,text)
    if n!=1:raise RuntimeError(f'Expected exactly one internal helper: {name}, got {n}')
text=text.rstrip()[:-1]+'\n'+'\n'.join(helpers)+'\n)'
wat.write_text(text,encoding='utf-8')
internal=base/'app.internal.wasm'
subprocess.run([binary('wasm-as'),str(wat),'--enable-bulk-memory','-o',str(internal)],check=True)
subprocess.run([binary('wasm-opt'),str(internal),'--enable-bulk-memory','--asyncify','--pass-arg=asyncify-ignore-imports','-o',str(wasm)],check=True)
package=workspace/'dist'/args.target/'aster.opentyrian.ast'
command=[sys.executable,str(SDK/'tools/pack_ast.py'),'--manifest',str(base/'manifest.json'),'--wasm',str(wasm),'--icon',str(ROOT/'assets/icon.bin'),'-o',str(package)]
if args.aot:
    sys.path.insert(0,str(SDK/'tools'))
    import build,aster_board
    app=build.App(ROOT);app.target=aster_board.get(args.target)
    aot=base/'app.aot'
    build.build_aot(app,wasm,aot,app.target['aot_target'])
    command.extend(['--aot',str(aot),'--aot-target',aster_board.pack_key_for(app.target['aot_target'])])
subprocess.run(command,check=True,env=env)
subprocess.run([sys.executable,str(SDK/'tools/verify_ast.py'),'--target',args.target,str(package)],check=True,env=env)
print(package)


