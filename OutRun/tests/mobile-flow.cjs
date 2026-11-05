'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {Host}=require('../../../aster-sim/src/host');
const {loadPackage}=require('../../../aster-sim/src/package');
const root=path.resolve(__dirname,'..');
function assets(dir,prefix,map){for(const e of fs.readdirSync(dir,{withFileTypes:true})){const n=prefix?prefix+'/'+e.name:e.name,p=path.join(dir,e.name);if(e.isDirectory())assets(p,n,map);else map.set(n,fs.readFileSync(p));}}
function run(w,h,resourceDir,shots){
 const pkg=loadPackage(path.join(root,'build/sim/dist/riscv/aster.outrun.ast'));assets(path.join(root,resourceDir),'',pkg.assets);
 let time=0;const host=new Host(pkg,{width:w,height:h,clock:()=>time});
 const frames=[];const tick=(n=1)=>{for(let i=0;i<n;i++){time+=33;host.tick();assert.equal(host.exitCode,null,JSON.stringify(host.logs.slice(-8)));}};
 const touch=(slot,action,x,y)=>host.input({type:'touch',slot,action,x,y});
 const tap=(x,y)=>{touch(0,1,x,y);touch(0,3,x,y);tick();};
 const shot=name=>{const b=host.screen.toBuffer('image/png');frames.push(b);if(shots){const out=path.join(root,'artifacts/mobile');fs.mkdirSync(out,{recursive:true});fs.writeFileSync(path.join(out,`${w}-${name}.png`),b);}};
 try{
  host.start();tick(2);shot('start');
  // Exercise every course load and return to circuit 1 before driving.
  for(let i=0;i<15;i++)tap(w/2+90,(h>=400?h-120:h)/2-16);
  tap(w/2,(h>=400?h-120:h)/2+45);tick(120);
  if(h<400){
   touch(1,1,w-30,h-25);
   host.input({type:'sensor',accel:[0,-3,9.3]});tick(12);shot('tilt-left');
   assert.match(JSON.stringify(host.logs),/INPUT steer=-1/);
   host.input({type:'sensor',accel:[0,3,9.3]});tick(12);shot('tilt-right');
   assert.match(JSON.stringify(host.logs),/INPUT steer=1/);
   touch(0,1,70,120);tick(2);shot('half-screen-brake');
   assert.match(JSON.stringify(host.logs.slice(-5)),/brake=1/,'Left half brakes while right half stays held');
   touch(0,3,70,120);tick(2);assert.match(JSON.stringify(host.logs.slice(-5)),/brake=0/);
   touch(1,2,70,120);tick(2);assert.match(JSON.stringify(host.logs.slice(-5)),/brake=1/,'Crossing center switches the driving zone');
   touch(1,2,w-30,h-25);tick(2);assert.match(JSON.stringify(host.logs.slice(-5)),/brake=0/);
   tap(36,56);tick(3); // Recenter while tilted, without releasing the gas finger.
   shot('tilt-centered');touch(1,3,w-30,h-25);
   host.input({type:'sensor',accel:[0,0,9.80665]});tap(152,20); // Touch fallback.
  }else assert.equal(host.metrics.abiCalls.aster_accel_read||0,0,'P4 never polls the IMU');
  const x=h>=400?76:38,y=h>=400?h-60:h-36;
  touch(0,1,x,y);touch(0,2,x-20,y);touch(1,1,w-30,h-25);tick(3);shot('steer-gas');
  touch(1,3,w-30,h-35);tick(2);shot('steer-only');touch(0,3,x-30,y);tick(2);
  touch(1,1,w-30,h-35);tick(130);shot('drive');touch(1,3,w-30,h-35);
  tap(w-40,20);shot('paused');tap(w/2,h/2-28);tick(3);shot('resumed');
  host.setPaused(true);host.setPaused(false);tick(3);
  assert.equal(host.diagnostics().trap,null);
  const logs=JSON.stringify(host.logs);assert.match(logs,/state=2/);assert.match(logs,/speed=[1-9]/);assert.doesNotMatch(logs,/errors=[1-9]|assets=[1-9]/);
  return frames;
 }finally{host.stop();}
}
for(const [w,h] of [[320,240],[480,480]]){
 const packed=run(w,h,'dist/store-resources',true),loose=run(w,h,'dist/resources',false);
 assert.deepEqual(packed,loose,'Packed resources must render exactly like the original loose resources');
 console.log(`${w}x${h}: start, manual acceleration, wheel + gas, menu, resume, packed/loose equivalence passed`);
}
