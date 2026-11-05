'use strict';
// Explicit integration test: run StoreServer on port 9080 with the portable
// StoreDemo release, then build AppStore/build/sim as documented in README.
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const {Host}=require('../../../aster-sim/src/host');
const {loadPackage}=require('../../../aster-sim/src/package');
const {MemoryFS}=require('../../../aster-sim/src/memory-fs');
const out=path.resolve(__dirname,'../artifacts');fs.mkdirSync(out,{recursive:true});
const pkg=loadPackage(path.resolve(__dirname,'../build/sim/dist/riscv/aster.store.ast'));
const catalog=[pkg],memoryStorage=new MemoryFS();
const options={network:true,width:480,height:480,catalog,memoryStorage,noAudio:true};
const delay=()=>new Promise(r=>setTimeout(r,10));
async function frames(h,n){for(let i=0;i<n;i++){h.tick();await delay();}}
function tap(h,x,y){h.input({type:'touch',action:1,x,y});h.input({type:'touch',action:3,x,y});}
function screenshot(h,name){fs.writeFileSync(path.join(out,name+'.png'),h.screen.toBuffer('image/png'));}
(async()=>{
 let h=new Host(pkg,options).start();
 try {
   await frames(h,40);screenshot(h,'catalog');
   tap(h,180,128);await frames(h,12);screenshot(h,'details');
   tap(h,350,450);
   const deadline=Date.now()+30000;
   while(!h.pendingInstall && Date.now()<deadline){h.tick();await delay();}
   screenshot(h,'download');
   assert.ok(h.pendingInstall,JSON.stringify(h.logs));
   assert.equal(catalog.length,1,'No registry mutation while Store VM is alive');
   h.stop();h.completeInstall();
   const installed=catalog.find(x=>x.manifest.id==='aster.store.demo');assert.ok(installed);
   assert.equal(installed.assets.get('pattern.bin').length,1048576);
   assert.equal(installed.assets.get('message.txt').toString(),'ASTER_STORE_RESOURCE_V1\n');
   h=new Host(pkg,options).start();await frames(h,15);screenshot(h,'installed');
   assert.ok(!h.logs.some(x=>x.message.includes('STORE FAIL')),JSON.stringify(h.logs));
   h.stop();
   h=new Host(installed,options).start();screenshot(h,'resource-demo');
   assert.ok(h.logs.some(x=>x.message.includes('STOREDEMO PASS')),JSON.stringify(h.logs));
   // A real download/install upgrade must preserve data and replace, not duplicate, the app.
   const save=path.join(h.root,'data','saved-progress.bin');h.storageFs.writeFileSync(save,Buffer.from('keep my progress'));h.stop();
   installed.manifest.version='0.0.1';
   h=new Host(pkg,options).start();await frames(h,100);
   assert(h.logs.some(x=>x.message.includes('id=aster.store.demo ')&&x.message.includes('state=1')),JSON.stringify(h.logs));
   tap(h,425,135);const updateDeadline=Date.now()+30000;
   while(!h.pendingInstall&&Date.now()<updateDeadline){h.tick();await delay();}
   assert(h.pendingInstall,'List update button must download and install');h.stop();h.completeInstall();
   assert.equal(catalog.filter(x=>x.manifest.id==='aster.store.demo').length,1);
   assert.notEqual(catalog.find(x=>x.manifest.id==='aster.store.demo').manifest.version,'0.0.1');
   assert.equal(memoryStorage.readFileSync(save).toString(),'keep my progress');
   h=new Host(pkg,options).start();await frames(h,100);tap(h,40,450);await frames(h,60);
   assert(h.logs.some(x=>x.message.includes('id=aster.store.demo ')&&x.message.includes('state=2')),JSON.stringify(h.logs));
   console.log(JSON.stringify({ok:true,installed:installed.manifest.id,externalResources:installed.assets.size,patternBytes:installed.assets.get('pattern.bin').length}));
 } finally {h.stop();}
})().catch(e=>{console.error(e.stack);process.exitCode=1;});
