'use strict';
// Real Store WASM against an immutable static source with opaque hosting ETags.
const fs=require('node:fs'),path=require('node:path'),http=require('node:http'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const {Host}=require('../../../aster-sim/src/host'),{loadPackage}=require('../../../aster-sim/src/package'),{MemoryFS}=require('../../../aster-sim/src/memory-fs');
const pkg=loadPackage(path.resolve(process.argv[2]||'AppStore/build/sim/riscv/dist/riscv/aster.store.ast'));
const programPath=path.resolve('TouchTest/dist/riscv/aster.touchtest.ast'),program=fs.readFileSync(programPath),app=loadPackage(programPath);
const resource=Buffer.alloc(65536,37),payloads=[program,resource],release='1'.repeat(64);
const digest=data=>crypto.createHash('sha256').update(data).digest('hex');
let base,mode,hits;
const metadata=()=>({id:app.manifest.id,version:app.manifest.version,release,boards:[mode==='wrong-board'?'wrong':'aster.simulator'],target:'wasm',abi_min:[4,1,0],requires:['display','touch'],screens:[[480,480]]});
const server=http.createServer((req,res)=>{
 const url=new URL(req.url,'http://localhost');res.setHeader('Content-Type','application/json');
 if(url.pathname==='/aster-source.json'){res.end(JSON.stringify({schema:1,kind:'aster-static-source',base_url:base+'/release-one'}));return;}
 if(url.pathname.endsWith('/catalog-aster.simulator-wasm-0.json')){res.end(JSON.stringify({schema:1,board:'aster.simulator',target:'wasm',total:1,next:-1,items:[{...metadata(),name:'静态资源测试',description:'独立资源',permissions:[],bytes:program.length+resource.length,file_count:2}]}));return;}
 if(url.pathname.endsWith(`/manifest-${release}-0.json`)){res.end(JSON.stringify({schema:1,...metadata(),total:2,next:-1,files:payloads.map((data,index)=>({index,role:index?'resource':'package',path:index?'pattern.bin':'package.ast',bytes:data.length,sha256:digest(data)}))}));return;}
 const match=url.pathname.match(/\/blob-([a-f0-9]{64})$/);
 if(match){hits++;const index=payloads.findIndex(data=>digest(data)===match[1]);assert(index>=0);const data=payloads[index],range=/^bytes=(\d+)-(\d+)$/.exec(req.headers.range||'');assert(range);assert.equal(req.headers['if-range'],undefined);const start=+range[1],end=+range[2];
  const body=Buffer.from(data.subarray(start,end+1));if(mode==='corrupt'&&index===1)body[0]^=1;
  res.writeHead(206,{'Content-Type':'application/octet-stream','Content-Length':body.length,'Content-Range':`bytes ${start}-${end}/${data.length}`,ETag:'"opaque-hosting-etag"'});res.end(body);return;
 }
 res.writeHead(404);res.end();
});
const delay=()=>new Promise(r=>setTimeout(r,5));
const tap=(h,x,y)=>{h.input({type:'touch',action:1,x,y});h.input({type:'touch',action:3,x,y});};
(async()=>{
 await new Promise(r=>server.listen(0,'127.0.0.1',r));base=`http://127.0.0.1:${server.address().port}`;
 for(mode of ['good','wrong-board','corrupt']){
  hits=0;const catalog=[pkg],memoryStorage=new MemoryFS();const h=new Host(pkg,{width:480,height:480,network:true,noAudio:true,catalog,memoryStorage});
  h.storageFs.writeFileSync(path.join(h.root,'kv',Buffer.from('store.url').toString('hex')),Buffer.from(base+'/aster-source.json\0'));
  try{
   h.start();for(let i=0;i<80;i++){h.tick();await delay();}tap(h,425,135);
   const end=Date.now()+15000;while(Date.now()<end&&!h.pendingInstall&&!h.logs.some(x=>x.message.includes('STORE FAIL'))){h.tick();await delay();}
   if(mode==='good'){assert(h.pendingInstall,JSON.stringify(h.logs));assert.equal(hits,2);h.stop();h.completeInstall();assert.deepEqual(catalog.find(x=>x.manifest.id===app.manifest.id).assets.get('pattern.bin'),resource);}
   else{assert(!h.pendingInstall);assert(h.logs.some(x=>x.message.includes('STORE FAIL')),JSON.stringify(h.logs));assert.equal(hits,mode==='wrong-board'?0:2);}
  }finally{h.stop();}
 }
 console.log('PASS static source: pinned release, opaque ETag, package/resource install, incompatible hardware and corrupt payload rejected');
})().catch(e=>{console.error(e);process.exitCode=1;}).finally(()=>server.close());
