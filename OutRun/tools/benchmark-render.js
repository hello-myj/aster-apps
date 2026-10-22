'use strict';
// Deterministic render workload and wall-clock benchmark; screenshots outside timing.
const fs=require('fs'),path=require('path'),{performance}=require('perf_hooks');
const sdk='E:/个人项目资料/aster/aster-sdk/simulator';
const {Host}=require(path.join(sdk,'src/host')),{loadPackage}=require(path.join(sdk,'src/package'));
const p=path.resolve(__dirname,'..'),label=process.argv[2]||'current',out=path.join(p,'artifacts/fps',label);
fs.mkdirSync(out,{recursive:true});let ms=1,logs=[];
const host=new Host(loadPackage(process.argv[3]||path.join(p,'dist/xtensa/aster.outrun.ast'),{}),{width:480,height:320,state:path.join(p,'artifacts/device-state'),clock:()=>ms,send:m=>{if(m.type==='log')logs.push(m);}});
const key=(code)=>{host.input({type:'key',code,pressed:true});host.input({type:'key',code,pressed:false});};
let results=[];
function run(name,n){let times=[];for(let i=0;i<n;i++){ms+=34;let t=performance.now();host.tick();times.push(performance.now()-t);}times.sort((a,b)=>a-b);results.push({name,frames:n,meanMs:times.reduce((a,b)=>a+b,0)/n,p95Ms:times[Math.floor(n*.95)]});fs.writeFileSync(path.join(out,name+'.png'),host.screen.toBuffer('image/png'));}
try{host.start();for(let map=1;map<=15;map++){run('map-'+String(map).padStart(2,'0'),20);if(map<15)key(106);}key(106);key(28);run('drive',400);console.log(JSON.stringify(results));fs.writeFileSync(path.join(out,'timing.json'),JSON.stringify({results,diagnostics:host.diagnostics()},null,2));}finally{host.stop();fs.writeFileSync(path.join(out,'events.json'),JSON.stringify(logs,null,2));}
