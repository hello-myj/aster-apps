/* SDK host-level verification. Real WASM, real SD data, fixed input clock. */
'use strict';
const fs=require('fs'),path=require('path');
const sdk='E:/个人项目资料/aster/aster-sdk/simulator';
const {Host}=require(path.join(sdk,'src/host'));
const {loadPackage}=require(path.join(sdk,'src/package'));
const project=path.resolve(__dirname,'..'),out=path.join(project,'artifacts/v2');
fs.mkdirSync(out,{recursive:true});
const pkg=loadPackage(path.join(project,'dist/xtensa/aster.outrun.ast'),{});
let ms=0,log=[],audio=[];
const host=new Host(pkg,{width:480,height:320,state:process.env.OUTRUN_SIM_STATE||path.join(project,'sdcard/aster/apps'),clock:()=>ms,send:m=>{if(m.type==='log')log.push(m);if(m.type.includes('audio'))audio.push({...m,data:undefined});}});
const emit=(code,pressed)=>host.input({type:'key',code,pressed});
try{
 host.start();host.tick();fs.writeFileSync(path.join(out,'start.png'),host.screen.toBuffer('image/png'));
 emit(28,true);emit(28,false);
 for(let f=0;f<2100;f++){
  ms=Math.floor(f*1000/30);
  if(f===320)emit(105,true);if(f===335)emit(105,false);
  if(f===350)emit(106,true);if(f===365)emit(106,false);
  if(f===390)emit(108,true);if(f===420)emit(108,false);
  host.tick();
  if([89,300,330,360,420,600,900,1500,2099].includes(f))fs.writeFileSync(path.join(out,'drive-'+f+'.png'),host.screen.toBuffer('image/png'));
 }
 fs.writeFileSync(path.join(out,'diagnostics.json'),JSON.stringify(host.diagnostics(),null,2));
 fs.writeFileSync(path.join(out,'audio-events.json'),JSON.stringify(audio,null,2));
 console.log('DIAGNOSTICS',JSON.stringify(host.diagnostics()));
}finally{host.stop();fs.writeFileSync(path.join(out,'events.json'),JSON.stringify(log,null,2));}



