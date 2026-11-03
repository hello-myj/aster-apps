'use strict';
// Real Store WASM, bounded HTTP fixture and actual simulator bitmap handles.
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),http=require('node:http'),crypto=require('node:crypto');
const {Host}=require('../../../aster-sim/src/host');
const {loadPackage}=require('../../../aster-sim/src/package');
const bmp=Buffer.alloc(54+48*48*3);bmp.write('BM');bmp.writeUInt32LE(bmp.length,2);bmp.writeUInt32LE(54,10);bmp.writeUInt32LE(40,14);bmp.writeInt32LE(48,18);bmp.writeInt32LE(48,22);bmp.writeUInt16LE(1,26);bmp.writeUInt16LE(24,28);
for(let i=54;i<bmp.length;i+=3){bmp[i]=30;bmp[i+1]=70;bmp[i+2]=230;}
const sha=crypto.createHash('sha256').update(bmp).digest('hex');
const release=i=>String(i).padStart(64,'0');
const items=[1,2].map(i=>({id:`test.icon${i}`,name:`Icon ${i}`,version:'1.0.0',release:release(i),bytes:1234,file_count:1,description:'Icon fixture',permissions:[],icon:{bytes:bmp.length,sha256:i===1?sha:'f'.repeat(64)}}));
let slow=false,timers=new Set();
const server=http.createServer((req,res)=>{
 const u=new URL(req.url,'http://localhost');
 if(u.pathname==='/v1/catalog'){res.setHeader('Content-Type','application/json');res.end(JSON.stringify({schema:1,items,total:2,next:-1}));return;}
 if(u.pathname.endsWith('/icon')){
  res.writeHead(200,{'Content-Length':bmp.length,'Content-Type':'image/bmp'});
  if(!slow)res.end(bmp);else {const t=setTimeout(()=>{timers.delete(t);if(!res.destroyed)res.end(bmp);},2000);timers.add(t);}
  return;
 }
 res.writeHead(404);res.end();
});
const wait=()=>new Promise(r=>setTimeout(r,10));
async function frames(h,n=20){for(let i=0;i<n;i++){h.tick();await wait();}}
function tap(h,x,y){h.input({type:'touch',action:1,x,y});h.input({type:'touch',action:3,x,y});}
const out=path.resolve(__dirname,'../artifacts/icons');fs.mkdirSync(out,{recursive:true});
(async()=>{
 await new Promise(r=>server.listen(0,'127.0.0.1',r));
 for(const [width,height,file] of [[320,240,process.argv[2]],[480,480,process.argv[3]]]){
  const h=new Host(loadPackage(path.resolve(file)),{width,height,network:true,noAudio:true});
  h.storageFs.writeFileSync(path.join(h.root,'kv',Buffer.from('store.url').toString('hex')),Buffer.from(`http://127.0.0.1:${server.address().port}\0`));
  h.start();
  try{
   await frames(h,100);
   assert.equal([...h.handles.values()].filter(x=>x.kind==='bitmap').length,1,'Good icon loads; invalid SHA uses placeholder');
   assert.ok(h.logs.some(x=>x.message.includes('STORE icon unavailable')));
   assert.ok(!h.logs.some(x=>x.message.includes('STORE FAIL')));
   fs.writeFileSync(path.join(out,`${width}-list.png`),h.screen.toBuffer('image/png'));
   tap(h,110,height<300?90:128);await frames(h);
   fs.writeFileSync(path.join(out,`${width}-detail.png`),h.screen.toBuffer('image/png'));
   const bottom=height-(height<300?44:56);tap(h,35,bottom+18);await frames(h);
   slow=true;tap(h,115,bottom+18);await frames(h,10);
   const before=h.screen.toBuffer('image/png');tap(h,35,bottom+18);await frames(h,20);
   assert.notDeepEqual(h.screen.toBuffer('image/png'),before,'Settings stays usable during background icon transfer');
   assert.ok(!h.logs.some(x=>x.message.includes('STORE FAIL')));
   console.log(`icon flow passed ${width}x${height}`);
  }finally{h.stop();slow=false;}
 }
})().catch(e=>{console.error(e.stack);process.exitCode=1;}).finally(()=>{for(const t of timers)clearTimeout(t);server.closeAllConnections();server.close();});
