'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
const sdk='E:/个人项目资料/aster/aster-sdk/simulator';
const {Host}=require(path.join(sdk,'src/host'));
const {loadPackage}=require(path.join(sdk,'src/package'));
const project=path.resolve(__dirname,'..'),out=path.join(project,'artifacts/v2');fs.mkdirSync(out,{recursive:true});
let ms=1,logs=[],over=false,starting=false;
const pkg=loadPackage(path.join(project,'dist/xtensa/aster.outrun.ast'),{});
const host=new Host(pkg,{width:480,height:320,state:path.join(project,'sdcard/aster/apps'),clock:()=>ms,send:m=>{if(m.type==='log'){logs.push(m);if(/state=3 /.test(m.message))over=true;}}});
function enter(){host.input({type:'key',code:28,pressed:true});host.input({type:'key',code:28,pressed:false});}
try{
 host.start();enter();
 let i;for(i=0;i<4000&&!over;i++){ms+=150;host.tick();}
 assert(over,'Timeout state was not reached');
 fs.writeFileSync(path.join(out,'timeout.png'),host.screen.toBuffer('image/png'));
 enter();ms+=150;host.tick();
 fs.writeFileSync(path.join(out,'retry.png'),host.screen.toBuffer('image/png'));
 const d=host.diagnostics();assert(!d.trap);fs.writeFileSync(path.join(out,'lifecycle-diagnostics.json'),JSON.stringify(d,null,2));
 console.log('TIMEOUT AND RETRY OK frames='+i,JSON.stringify(d));
}finally{host.stop();fs.writeFileSync(path.join(out,'lifecycle-events.json'),JSON.stringify(logs,null,2));}
