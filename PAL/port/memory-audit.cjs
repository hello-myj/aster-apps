const fs=require('node:fs');
const path=require('node:path');
const {loadPackage}=require(require('node:path').join(process.env.ASTER_SIM || require('node:path').resolve(__dirname,'../../../aster-sim'),'src/package'));
const {Host}=require(require('node:path').join(process.env.ASTER_SIM || require('node:path').resolve(__dirname,'../../../aster-sim'),'src/host'));
const pkg=loadPackage(path.join(__dirname,'build/pal-research.ast'));
function readUleb(b,start){let v=0,shift=0,p=start;do{const c=b[p++];v|=(c&127)<<shift;shift+=7;if(!(c&128))break;}while(true);return {value:v>>>0,end:p};}
function uleb(value){const out=[];do{let c=value&127;value>>>=7;out.push(c|(value?128:0));}while(value);return Buffer.from(out);}
function memoryVariant(pages){
 const b=pkg.wasm;let p=8;
 while(p<b.length){const id=b[p++],size=readUleb(b,p),start=size.end,end=start+size.value;
  if(id===5){const payload=Buffer.concat([uleb(1),uleb(1),uleb(pages),uleb(pages)]);return Buffer.concat([b.subarray(0,p),uleb(payload.length),payload,b.subarray(end)]);}
  p=end;
 }throw Error('No defined memory section');
}
const stubImports={env:{}};
for(const i of WebAssembly.Module.imports(new WebAssembly.Module(pkg.wasm)))stubImports.env[i.name]=()=>0;
const initialInstance=new WebAssembly.Instance(new WebAssembly.Module(pkg.wasm),stubImports);
const heapBase=Number(initialInstance.exports.__heap_base.value);
const moduleFloorPages=Math.ceil(heapBase/65536);
const profile=JSON.parse(fs.readFileSync(path.join(__dirname,'memory-profile.json'),'utf8'));
const results=[];
for(const pages of [...new Set([moduleFloorPages-1,moduleFloorPages,moduleFloorPages+1,profile.linearBytes/65536])]){
 try{
  const bytes=pages*65536,wasm=memoryVariant(pages);
  const variant={...pkg,wasm,manifest:{...pkg.manifest,memory:{...pkg.manifest.memory,initial:bytes,max:bytes}}};
  const h=new Host(variant,{width:320,height:480,board:'waveshare-35b',state:path.join(__dirname,'sim-state'),send:()=>{}});
  h.start();let peak=h.heap;
  for(let i=0;i<100&&h.exitCode===null;i++){h.tick();peak=Math.max(peak,h.heap);}
  results.push({pages,bytes,MiB:bytes/1048576,status:h.exitCode===null?'startup-pass':'app-exit',frames:h.frame,presents:h.presentCount,hostAllocationEnd:peak});h.stop();
 }catch(e){results.push({pages,bytes:pages*65536,MiB:pages/16,status:'fail',error:e.message});}
}
const map=fs.readFileSync(path.join(__dirname,'build/app.map'),'utf8');
const symbols=[];
for(const line of map.split(/\r?\n/)){
 const m=/^\s*([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([A-Za-z_]\w*)\s*$/.exec(line);
 if(m&&parseInt(m[3],16)>=32768)symbols.push({name:m[4],bytes:parseInt(m[3],16),KiB:parseInt(m[3],16)/1024});
}
symbols.sort((a,b)=>b.bytes-a.bytes);
const report={heapBase,moduleFloorPages,moduleFloorBytes:moduleFloorPages*65536,configuredBytes:initialInstance.exports.memory.buffer.byteLength,results,largestFixedOwners:symbols};
fs.writeFileSync(path.join(__dirname,'build/memory-audit.json'),JSON.stringify(report,null,2));
console.log(JSON.stringify(report,null,2));
if(process.argv.includes('--write-minimum')){
 const selected=results.find(x=>x.status==='startup-pass');if(!selected)throw Error('No passing variant');
 fs.writeFileSync(path.join(__dirname,'build/minimum.wasm'),memoryVariant(selected.pages));
 const manifest={...pkg.manifest,memory:{...pkg.manifest.memory,initial:selected.bytes,max:selected.bytes}};
 fs.writeFileSync(path.join(__dirname,'build/minimum-manifest.json'),JSON.stringify(manifest,null,2));
}
