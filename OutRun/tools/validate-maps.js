'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
const sdk='E:/个人项目资料/aster/aster-sdk/simulator';
const {Host}=require(path.join(sdk,'src/host'));
const {loadPackage}=require(path.join(sdk,'src/package'));
const project=path.resolve(__dirname,'..'),out=path.join(project,'artifacts/v2/maps');fs.mkdirSync(out,{recursive:true});
const pkg=loadPackage(path.join(project,'dist/xtensa/aster.outrun.ast'),{});let ms=1,logs=[];
const host=new Host(pkg,{width:480,height:320,state:process.env.OUTRUN_SIM_STATE||path.join(project,'sdcard/aster/apps'),clock:()=>ms,send:m=>{if(m.type==='log')logs.push(m);}});
try{
 host.start();for(let id=1;id<=15;id++){
  for(let f=0;f<3;f++){ms+=34;host.tick();}
  fs.writeFileSync(path.join(out,'map-'+String(id).padStart(2,'0')+'.png'),host.screen.toBuffer('image/png'));
  assert(logs.some(x=>x.message.startsWith('MAP '+id+' ')),'map not loaded '+id);
  if(id<15){host.input({type:'key',code:106,pressed:true});host.input({type:'key',code:106,pressed:false});}
 }
 const d=host.diagnostics();assert(!d.trap);fs.writeFileSync(path.join(out,'diagnostics.json'),JSON.stringify(d,null,2));
 console.log('ALL 15 MAPS LOADED / RENDERED',JSON.stringify(d));
}finally{host.stop();fs.writeFileSync(path.join(out,'events.json'),JSON.stringify(logs,null,2));}

