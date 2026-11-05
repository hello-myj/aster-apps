 'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),http=require('node:http');
const {Host}=require('../../../aster-sim/src/host');const {loadPackage}=require('../../../aster-sim/src/package');const {MemoryFS}=require('../../../aster-sim/src/memory-fs');
const pkg=loadPackage(path.resolve(process.argv[2]||'AppStore/build/sim/dist/riscv/aster.store.ast'));
const versions=[['upgrade','1.10.0','1.9.9',1],['fresh','1.0.0',null,0],['same','1.2','1.2.0',2],['newer','1.9.0','1.10.0',3]];
const items=versions.map(([id,version],i)=>({id:'test.'+id,name:['可更新应用','未安装应用','已是最新','本机版本较新'][i],version,release:String(i+1).padStart(64,'0'),bytes:1234,file_count:1,description:'更新与列表点击测试',permissions:[]}));
const requests=[];const server=http.createServer((req,res)=>{const u=new URL(req.url,'http://localhost');if(u.pathname==='/v1/catalog'){res.setHeader('Content-Type','application/json');res.end(JSON.stringify({schema:1,items,next:-1,total:4}));}else{requests.push(u.pathname);res.writeHead(404);res.end();}});
const delay=()=>new Promise(r=>setTimeout(r,10));async function frames(h,n=35){for(let i=0;i<n;i++){h.tick();await delay();assert.equal(h.diagnostics().trap,null);}}
function tap(h,x,y){h.input({type:'touch',action:1,x,y});h.input({type:'touch',action:3,x,y});}
const out=path.resolve(__dirname,'../artifacts/updates');fs.mkdirSync(out,{recursive:true});function shot(h,name){fs.writeFileSync(path.join(out,name+'.png'),h.screen.toBuffer('image/png'));}
(async()=>{await new Promise(r=>server.listen(0,'127.0.0.1',r));
for(const [width,height] of [[320,240],[480,480]]){
 const first=height<300?74:104,stride=height<300?50:72,bottom=height-(height<300?44:56);
 async function create(){const catalog=[pkg,...versions.filter(x=>x[2]).map(([id,_,local])=>({...pkg,manifest:{...pkg.manifest,id:'test.'+id,version:local}}))];const h=new Host(pkg,{width,height,catalog,memoryStorage:new MemoryFS(),network:true,noAudio:true});h.storageFs.writeFileSync(path.join(h.root,'kv',Buffer.from('store.url').toString('hex')),Buffer.from(`http://127.0.0.1:${server.address().port}\0`));h.start();await frames(h,65);for(const [id,,,state] of versions)assert(h.logs.some(x=>x.message.includes('id=test.'+id+' ')&&x.message.includes('state='+state)),JSON.stringify(h.logs));return h;}
 let h=await create();try{shot(h,width+'-list');const before=h.screen.toBuffer('image/png');tap(h,25,first+(height<300?46:66));await frames(h);assert.deepEqual(h.screen.toBuffer('image/png'),before,'Row gaps do not open details');const n=requests.length;tap(h,25,first+20);await frames(h);assert.notDeepEqual(h.screen.toBuffer('image/png'),before,'Entire row opens details');assert.equal(requests.length,n,'Row body does not start download');shot(h,width+'-update-detail');tap(h,width-30,bottom+18);await frames(h);assert(requests.slice(n).some(x=>x.endsWith(items[0].release)),'Detail update starts matching release');}finally{h.stop();}
 h=await create();try{const n=requests.length;tap(h,width-55,first+20);await frames(h);assert(requests.slice(n).some(x=>x.endsWith(items[0].release)),'List update acts directly');}finally{h.stop();}
 h=await create();try{const n=requests.length;tap(h,width-55,first+stride+20);await frames(h);assert(requests.slice(n).some(x=>x.endsWith(items[1].release)),'Missing app list button downloads');}finally{h.stop();}
 h=await create();try{if(height<300){tap(h,width-30,bottom+18);await frames(h);}const row=height<300?0:2,n=requests.length;tap(h,width-55,first+row*stride+20);assert.equal(h.switchTo,'test.same');assert.equal(requests.length,n,'Current version opens without downloading');}finally{h.stop();}
 h=await create();try{if(height<300){tap(h,width-30,bottom+18);await frames(h);}const row=height<300?1:3,n=requests.length;tap(h,25,first+row*stride+20);await frames(h);shot(h,width+'-newer-detail');tap(h,width-30,bottom+18);await frames(h);assert.equal(requests.length,n,'Local newer version cannot be downgraded');assert(!h.pendingInstall);}finally{h.stop();}
 h=await create();try{h.options.catalog.find(x=>x.manifest.id==='test.upgrade').manifest.version='1.10.0';tap(h,110,bottom+18);await frames(h,65);const logs=h.logs.filter(x=>x.message.includes('id=test.upgrade '));assert(logs.at(-1).message.includes('state=2'),'Refresh recalculates installed version');}finally{h.stop();}
 console.log('update states and row/action routing passed '+width+'x'+height);
}
})().catch(e=>{console.error(e.stack);process.exitCode=1;}).finally(()=>{server.closeAllConnections();server.close();});
