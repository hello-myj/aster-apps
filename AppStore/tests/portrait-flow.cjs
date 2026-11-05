 'use strict';
const fs=require('node:fs'),path=require('node:path'),http=require('node:http'),assert=require('node:assert/strict');
const {Host}=require('../../../aster-sim/src/host'),{loadPackage}=require('../../../aster-sim/src/package'),{MemoryFS}=require('../../../aster-sim/src/memory-fs');
const pkg=loadPackage(path.resolve(process.argv[2]||'AppStore/build/sim/dist/riscv/aster.store.ast'));
const large=loadPackage(path.resolve(process.argv[3]||'AppStore/build/sim/riscv/dist/riscv/aster.store.ast'));let total=17;const seen=[];const server=http.createServer((req,res)=>{const u=new URL(req.url,'http://localhost'),p=Number(u.searchParams.get('page')||0);if(u.pathname!='/v1/catalog'){res.writeHead(404);res.end();return;}seen.push(p);const items=Array.from({length:total},(_,i)=>({id:'test.page'+i,name:'应用 '+(i+1),version:'1.0.0',release:String(i+1).padStart(64,'0'),bytes:1234,file_count:1,description:'分页测试',permissions:[]}));res.setHeader('Content-Type','application/json');res.end(JSON.stringify({schema:1,total,items:items.slice(p*8,p*8+8),next:(p+1)*8<total?p+1:-1}));});
const wait=()=>new Promise(r=>setTimeout(r,10));async function frames(h,n=30){for(let i=0;i<n;i++){h.tick();await wait();assert.equal(h.diagnostics().trap,null);}}
function tap(h,x,y){h.input({type:'touch',action:1,x,y});h.input({type:'touch',action:3,x,y});}
function page(h,current,total){const log=h.logs.filter(x=>x.message.includes('STORE page current=')).at(-1);assert(log&&log.message.includes(`current=${current} total=${total}`),JSON.stringify(log));}
const out=path.resolve(__dirname,'../artifacts/pages');fs.mkdirSync(out,{recursive:true});
(async()=>{await new Promise(r=>server.listen(0,'127.0.0.1',r));for(const [width,height] of [[240,320],[320,240],[480,480]]){
 total=17;const base=width>=400?large:pkg;const selected={...base,manifest:{...base.manifest,orientation:width<height?"portrait":"landscape"}};const h=new Host(selected,{width,height,network:true,noAudio:true,memoryStorage:new MemoryFS()});h.storageFs.writeFileSync(path.join(h.root,'kv',Buffer.from('store.url').toString('hex')),Buffer.from(`http://127.0.0.1:${server.address().port}\0`));h.start();const bottom=height-((height<300||width<height)?44:56),count=width<height?7:height<300?9:5;
 try{await frames(h,60);page(h,1,count);fs.writeFileSync(path.join(out,width+'-first.png'),h.screen.toBuffer('image/png'));
 if(width<height){
 const before=h.screen.toBuffer('image/png');tap(h,25,94);await frames(h);fs.writeFileSync(path.join(out,'240-detail.png'),h.screen.toBuffer('image/png'));assert.notDeepEqual(h.screen.toBuffer('image/png'),before);tap(h,35,bottom+18);await frames(h);assert.deepEqual(h.screen.toBuffer('image/png'),before);
 tap(h,35,bottom+18);await frames(h);fs.writeFileSync(path.join(out,'240-settings.png'),h.screen.toBuffer('image/png'));assert.notDeepEqual(h.screen.toBuffer('image/png'),before);tap(h,35,bottom+18);await frames(h);assert.deepEqual(h.screen.toBuffer('image/png'),before);
 }
 for(let i=2;i<=count;i++){tap(h,width-30,bottom+18);await frames(h);page(h,i,count);}fs.writeFileSync(path.join(out,width+'-last.png'),h.screen.toBuffer('image/png'));
 tap(h,width-30,bottom+18);await frames(h);page(h,count,count);
 for(let i=count-1;i>=1;i--){tap(h,width<height?145:190,bottom+18);await frames(h);page(h,i,count);}tap(h,width<height?145:190,bottom+18);await frames(h);page(h,1,count);
 total=0;tap(h,width<height?90:115,bottom+18);await frames(h);page(h,0,0);fs.writeFileSync(path.join(out,width+'-empty.png'),h.screen.toBuffer('image/png'));
 total=1;tap(h,width<height?90:115,bottom+18);await frames(h);page(h,1,1);tap(h,width-30,bottom+18);await frames(h);page(h,1,1);
 console.log('page count, server boundary back navigation, empty and refresh passed '+width+'x'+height);
 }finally{h.stop();}
}assert(seen.includes(2));})().catch(e=>{console.error(e.stack);process.exitCode=1;}).finally(()=>{server.closeAllConnections();server.close();});
