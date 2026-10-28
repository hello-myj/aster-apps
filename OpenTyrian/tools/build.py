#!/usr/bin/env python3
"""SDK compilation -> internal Asyncify lowering -> normal SDK pack/verify."""
import os,pathlib,re,subprocess,sys
ROOT=pathlib.Path(__file__).resolve().parents[1]
SDK=ROOT.parent.parent/'aster-sdk'
env=dict(os.environ,PYTHONUTF8='1')
base=ROOT/'build/xtensa/aster.opentyrian'
base.mkdir(parents=True,exist_ok=True)
log=ROOT/'artifacts/build.log'
with log.open('w',encoding='utf-8') as f:
    result=subprocess.run([str(SDK/'aster.exe'),'build',str(ROOT),'--target','xtensa','--no-aot'],cwd=ROOT,env=env,stdout=f,stderr=subprocess.STDOUT)
if result.returncode:
    text=log.read_text(encoding='utf-8',errors='replace')
    if 'imports not provided by firmware' not in text or not (base/'app.wasm').exists():
        print(text[-12000:]);raise SystemExit(result.returncode)
wasm=base/'app.async.wasm'
BIN=ROOT/'artifacts/deps/binaryen-version_133/bin'
wat=base/'app.internal.wat'
subprocess.run([str(BIN/'wasm-dis.exe'),str(base/'app.wasm'),'-o',str(wat)],check=True)
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
subprocess.run([str(BIN/'wasm-as.exe'),str(wat),'--enable-bulk-memory','-o',str(internal)],check=True)
subprocess.run([str(BIN/'wasm-opt.exe'),str(internal),'--enable-bulk-memory','--asyncify','--pass-arg=asyncify-ignore-imports','--pass-arg=asyncify-ignore-indirect','-o',str(wasm)],check=True)
package=ROOT/'dist/xtensa/aster.opentyrian.ast'
subprocess.run([sys.executable,str(SDK/'tools/pack_ast.py'),'--manifest',str(base/'manifest.json'),'--wasm',str(wasm),'--icon',str(ROOT/'assets/icon.bin'),'-o',str(package)],check=True,env=env)
subprocess.run([str(SDK/'aster.exe'),'verify',str(package)],check=True,env=env)
print(package)


