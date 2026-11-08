import binaryen from 'binaryen';
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
const here=path.dirname(fileURLToPath(import.meta.url));
const requested=process.argv[2]||process.env.PAL_BUILD_DIR;
const build=requested?path.resolve(here,'..',requested):path.join(here,'build');
const input=fs.readFileSync(path.join(build,'raw.wasm'));
const mod=binaryen.readBinary(input);
mod.setFeatures(binaryen.Features.All);
const controls=['start_unwind','start_rewind','stop_unwind','stop_rewind','get_state'];
const yieldInfo=binaryen.getFunctionInfo(mod.getFunction('yield'));
mod.removeFunction('yield');
mod.addFunctionImport('yield','env','pal_yield',yieldInfo.params,yieldInfo.results);
binaryen.setPassArgument('asyncify-imports','env.pal_yield');
binaryen.setPassArgument('asyncify-removelist', ['aster_app_init','aster_app_event','aster_app_stop','aster_app_destroy','aster_app_pause','aster_app_resume','pal_stack_get','pal_stack_set',...controls.map(x=>'pal_async_'+x)].join(','));
mod.runPasses(['asyncify']);
mod.removeFunction('yield');
mod.addFunction('yield',yieldInfo.params,yieldInfo.results,yieldInfo.vars,yieldInfo.body);
for(const name of controls) {
  const old='pal_async_'+name;
  const info=binaryen.getFunctionInfo(mod.getFunction(old));
  const args=name.startsWith('start_')?[mod.local.get(0,binaryen.i32)]:[];
  mod.removeFunction(old);
  mod.addFunction(old,info.params,info.results,[],mod.call('asyncify_'+name,args,info.results));
}
mod.removeFunction('pal_stack_get');
mod.addFunction('pal_stack_get',binaryen.none,binaryen.i32,[],mod.global.get('__stack_pointer',binaryen.i32));
mod.removeFunction('pal_stack_set');
mod.addFunction('pal_stack_set',binaryen.i32,binaryen.none,[],mod.global.set('__stack_pointer',mod.local.get(0,binaryen.i32)));
/* Keep exported entry points at low function indices for SDK symbol scanners.
 * Calls and table elements remain name-based and are reindexed by Binaryen. */
const exported=new Set();
for(let i=0;i<mod.getNumExports();i++){
 const entry=binaryen.getExportInfo(mod.getExportByIndex(i));
 if(entry.kind===binaryen.ExternalFunction)exported.add(entry.value);
}
const definitions=[];
for(let i=0;i<mod.getNumFunctions();i++){
 const info=binaryen.getFunctionInfo(mod.getFunctionByIndex(i));
 if(!info.module)definitions.push(info);
}
for(const info of definitions)mod.removeFunction(info.name);
for(const info of [...definitions.filter(x=>exported.has(x.name)),...definitions.filter(x=>!exported.has(x.name))])mod.addFunction(info.name,info.params,info.results,info.vars,info.body);
if(!mod.validate())throw Error('Asyncify validation failed');
const wasm=mod.emitBinary();
fs.writeFileSync(path.join(build,'transformed.wat'),mod.emitText());
fs.writeFileSync(path.join(build,'app.wasm'),wasm);
console.log('Aster PAL module: '+wasm.length+' bytes');
console.log(WebAssembly.Module.imports(new WebAssembly.Module(wasm)));
