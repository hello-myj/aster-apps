'use strict';
// Run with a portable Store AST and a reachable StoreServer. Exercises real
// guest hit testing on both screen sizes, including compact keyboard controls.
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const {Host}=require('../../../aster-sim/src/host');
const {loadPackage}=require('../../../aster-sim/src/package');
const small=loadPackage(path.resolve(process.argv[2]||'AppStore/artifacts/lckfb/store-portable.ast'));
const large=loadPackage(path.resolve(process.argv[3]||'AppStore/dist/riscv/aster.store.ast'));
const out=path.resolve(__dirname,'../artifacts/layout');fs.mkdirSync(out,{recursive:true});
const delay=()=>new Promise(r=>setTimeout(r,10));
async function frames(h,n=20){for(let i=0;i<n;i++){h.tick();await delay();}}
function tap(h,x,y){h.input({type:'touch',action:1,x,y});h.input({type:'touch',action:3,x,y});}
function shot(h,name){fs.writeFileSync(path.join(out,name+'.png'),h.screen.toBuffer('image/png'));}
(async()=>{
 for(const [width,height] of [[320,240],[480,480]]){
  const h=new Host(height<300?small:large,{width,height,network:true,noAudio:true}).start();
  try{
   await frames(h,60);assert.ok(h.logs.some(x=>x.message.includes('STORE ready')),JSON.stringify(h.logs));shot(h,`${width}-catalog`);
   const catalogImage=h.screen.toBuffer('image/png');
   const bottom=height-(height<300?44:56);
   tap(h,35,bottom+18);await frames(h);shot(h,`${width}-settings`);
   assert.notDeepEqual(h.screen.toBuffer('image/png'),catalogImage,'Settings must open after the catalog request finishes');
   if(height<300){
    // Clear, type, delete, type: saving an invalid URL must stay on settings.
    tap(h,260,171);await frames(h);const empty=h.screen.toBuffer('image/png');
    tap(h,18,93);await frames(h);assert.notDeepEqual(h.screen.toBuffer('image/png'),empty,'Typing edits the URL');
    tap(h,150,171);await frames(h);assert.deepEqual(h.screen.toBuffer('image/png'),empty,'Delete removes the typed character');
    tap(h,18,93);await frames(h);const invalid=h.screen.toBuffer('image/png');
    tap(h,210,bottom+18);await frames(h);shot(h,'320-keyboard');
    assert.deepEqual(h.screen.toBuffer('image/png'),invalid,'Invalid URL cannot be saved');
    assert.ok(!h.pendingInstall);
   }
   tap(h,40,bottom+18);await frames(h);shot(h,`${width}-returned`);
   assert.ok(!h.logs.some(x=>/STORE draw error|trap|exception/i.test(x.message)),JSON.stringify(h.logs));
   console.log(`layout passed ${width}x${height}`);
  }finally{h.stop();}
 }
})().catch(e=>{console.error(e.stack);process.exitCode=1;});
