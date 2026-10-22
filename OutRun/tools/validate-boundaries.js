'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
const sdk='E:/个人项目资料/aster/aster-sdk/simulator';
const {Host}=require(path.join(sdk,'src/host'));const {loadPackage}=require(path.join(sdk,'src/package'));
const p=path.resolve(__dirname,'..'),out=path.join(p,'artifacts/v2');let ms=1,log=[];
const pkg=loadPackage(path.join(p,'artifacts/boundary-check/dist/xtensa/aster.outrun.ast'),{});
const host=new Host(pkg,{width:480,height:320,state:path.join(p,'sdcard/aster/apps'),clock:()=>ms,send:m=>{if(m.type==='log')log.push(m);}});
try{
 host.start();
 for(let i=0;i<600&&host.exitCode===null;i++){ms+=33;host.tick();if(i===100)fs.writeFileSync(path.join(out,'goal-approach.png'),host.screen.toBuffer('image/png'));}
 assert.equal(host.exitCode,0);assert(log.filter(m=>m.message.startsWith('BOUNDARY OK')).length===5);assert(log.some(m=>m.message.startsWith('GOAL OK')));
 fs.writeFileSync(path.join(out,'goal.png'),host.screen.toBuffer('image/png'));
 fs.writeFileSync(path.join(out,'boundary-diagnostics.json'),JSON.stringify(host.diagnostics(),null,2));
 console.log('BOUNDARIES / GOAL PASSED',JSON.stringify(host.diagnostics()));
}finally{host.stop();fs.writeFileSync(path.join(out,'boundary-events.json'),JSON.stringify(log,null,2));}



