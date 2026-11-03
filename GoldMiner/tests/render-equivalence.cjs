'use strict';
// Historical 0.1.5 -> 0.1.6 optimization check, not the 0.2 touch UI.
// Current layout tests: touch-flow.cjs.
// Pass the previous release AST and the optimized AST; both use the same assets.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {Host}=require('../../../aster-sim/src/host');
const {loadPackage}=require('../../../aster-sim/src/package');
const resource=path.resolve(__dirname,'../dist/resources');
function add(dir,rel,map){for(const e of fs.readdirSync(dir,{withFileTypes:true})){
 const name=rel?rel+'/'+e.name:e.name,full=path.join(dir,e.name);
 if(e.isDirectory())add(full,name,map);else map.set(name,fs.readFileSync(full));
}}
function run(file){const pkg=loadPackage(file);add(resource,'',pkg.assets);let now=0;
 const h=new Host(pkg,{width:480,height:480,clock:()=>now}).start(),pictures=[];
 const frames=n=>{for(let i=0;i<n;i++){now+=33;h.tick();assert.equal(h.exitCode,null);}};
 const snap=()=>pictures.push(Buffer.from(h.screen.getContext('2d').getImageData(0,0,480,480).data));
 const tap=(x,y)=>{h.input({type:'touch',action:1,x,y});h.input({type:'touch',action:3,x,y});};
 try{frames(2);snap();tap(400,415);frames(1);snap();frames(140);snap();tap(150,415);frames(30);snap();tap(35,415);frames(1);snap();return {pictures,diagnostics:h.diagnostics(),logs:h.logs};}
 finally{h.stop();}
}
assert.equal(process.argv.length,4,'Pass baseline.ast optimized.ast');
const a=run(process.argv[2]),b=run(process.argv[3]);
for(let i=0;i<a.pictures.length;i++)assert.ok(a.pictures[i].equals(b.pictures[i]),`Frame ${i} differs`);
assert.ok(b.logs.some(x=>x.message.includes('scene=GAME')),'Gameplay was reached');
console.log(JSON.stringify({ok:true,identicalFrames:a.pictures.length,
 baselineMeanMs:a.diagnostics.frameTiming.meanMs,optimizedMeanMs:b.diagnostics.frameTiming.meanMs,
 baselineReads:a.diagnostics.abiCalls.aster_fs_read,optimizedReads:b.diagnostics.abiCalls.aster_fs_read}));
