'use strict';
// Real Store WASM + HTTP manifest, package and resource download from portrait.
const fs=require('node:fs'),path=require('node:path'),http=require('node:http'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const {Host}=require('../../../aster-sim/src/host'),{loadPackage}=require('../../../aster-sim/src/package'),{MemoryFS}=require('../../../aster-sim/src/memory-fs');
const pkg=loadPackage(path.resolve(process.argv[2]||'AppStore/build/sim/xtensa/dist/xtensa/aster.store.ast'));
const fixturePath=path.resolve(process.argv[3]||'StoreDemo/build/sim/dist/riscv/aster.store.demo.ast');
const fixture=loadPackage(fixturePath),program=fs.readFileSync(fixturePath);
let eofRaces=0;
const resource=Buffer.alloc(65536,37),payloads=[program,resource],release='1'.repeat(64),id=fixture.manifest.id,version=fixture.manifest.version;
let screens,board,target,hits;
const server=http.createServer((req,res)=>{
 const u=new URL(req.url,'http://localhost');res.setHeader('Content-Type','application/json');
 if(u.pathname==='/v1/catalog'){res.end(JSON.stringify({schema:1,total:1,next:-1,items:[{id,name:'横屏下载测试',version,release,bytes:program.length+resource.length,file_count:2,description:'屏幕方向兼容校验',permissions:[]}]}));return;}
 if(u.pathname.endsWith('/'+release)){res.end(JSON.stringify({schema:1,id,version,release,target,boards:[board],abi_min:[4,3,0],requires:[],screens,total:2,next:-1,files:payloads.map((data,index)=>({index,role:index?'resource':'package',path:index?'pattern.bin':'package.ast',bytes:data.length,sha256:crypto.createHash('sha256').update(data).digest('hex')}))}));return;}
 const match=u.pathname.match(/\/files\/(\d+)$/);if(match){hits++;const data=payloads[Number(match[1])],range=/bytes=(\d+)-(\d+)/.exec(req.headers.range||'');assert(range);const a=+range[1],b=+range[2];res.writeHead(206,{'Content-Length':b-a+1,'Content-Range':`bytes ${a}-${b}/${data.length}`,ETag:'"'+crypto.createHash('sha256').update(data).digest('hex')+'"'});res.end(data.subarray(a,b+1));return;}
 res.writeHead(404);res.end();
});
const delay=()=>new Promise(r=>setTimeout(r,5));
(async()=>{await new Promise(r=>server.listen(0,'127.0.0.1',r));
 for(const [shape,expected,other] of [[[[320,240]],true,{}],[[[240,320]],true,{}],[[[480,320]],false,{}],[[[320,240]],false,{target:'xtensa'}],[[[320,240]],false,{board:'other.board'}]]){
  screens=shape;board=other.board||'aster.simulator';target=other.target||'wasm';hits=0;const catalog=[pkg],storage=new MemoryFS();
  const h=new Host(pkg,{width:240,height:320,network:true,noAudio:true,catalog,memoryStorage:storage});
  // Force status lock contention immediately after successful EOF. read()==0
  // already establishes completion; a redundant status query must not fail it.
  const read=h.http.read.bind(h.http),status=h.http.status.bind(h.http);
  let afterEof=false;
  h.http.read=(...args)=>{const r=read(...args);afterEof=r===0;if(afterEof)eofRaces++;return r;};
  h.http.status=(...args)=>{if(afterEof){afterEof=false;return -15;}return status(...args);};
  h.storageFs.writeFileSync(path.join(h.root,'kv',Buffer.from('store.url').toString('hex')),Buffer.from(`http://127.0.0.1:${server.address().port}\0`));
  try{h.start();for(let i=0;i<60;i++){h.tick();await delay();}h.input({type:'touch',action:1,x:210,y:94});h.input({type:'touch',action:3,x:210,y:94});
   const end=Date.now()+10000;while(Date.now()<end&&!h.pendingInstall&&!h.logs.some(x=>x.message.includes('STORE FAIL'))){h.tick();await delay();}
   assert.equal(!!h.pendingInstall,expected,JSON.stringify(h.logs));
   if(expected){assert.equal(hits,2);h.stop();h.completeInstall();const app=catalog.find(x=>x.manifest.id===id);assert(app);assert.deepEqual(app.assets.get('pattern.bin'),resource);}
   else assert.equal(hits,0,'Rejected compatibility metadata must never start file downloads');
  }finally{h.stop();}
 }
 assert(eofRaces>0,'Exercise EOF/status contention');
 console.log('PASS EOF/status contention; portrait -> landscape download and install; wrong dimensions, board and architecture rejected');
})().catch(e=>{console.error(e.stack);process.exitCode=1;}).finally(()=>{server.closeAllConnections();server.close();});
