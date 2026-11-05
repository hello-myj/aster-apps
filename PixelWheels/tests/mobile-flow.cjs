'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {Host}=require('../../../aster-sim/src/host'),{loadPackage}=require('../../../aster-sim/src/package');
const root=path.resolve(__dirname,'..');
function add(dir,prefix,map){for(const e of fs.readdirSync(dir,{withFileTypes:true})){const n=prefix?prefix+'/'+e.name:e.name,p=path.join(dir,e.name);if(e.isDirectory())add(p,n,map);else map.set(n,fs.readFileSync(p));}}
for(const [w,h] of [[320,240],[480,480]]){
 const pkg=loadPackage(path.join(root,'build/sim/dist/riscv/aster.pixelwheels.ast'));add(path.join(root,'dist/resources'),'',pkg.assets);
 let time=0;const host=new Host(pkg,{width:w,height:h,clock:()=>time});
 const tick=(n=1)=>{for(let i=0;i<n;i++){time+=33;host.tick();assert.equal(host.exitCode,null,JSON.stringify(host.logs.slice(-5)));assert.equal(host.diagnostics().trap,null);}};
 const touch=(slot,action,x,y)=>host.input({type:'touch',slot,action,x,y});
 const tap=(x,y,slot=0)=>{touch(slot,1,x,y);touch(slot,3,x,y);tick(2);};
 const out=path.join(root,'artifacts/mobile');fs.mkdirSync(out,{recursive:true});
 const shot=name=>fs.writeFileSync(path.join(out,`${w}-${name}.png`),host.screen.toBuffer('image/png'));
 const input=()=>host.logs.filter(x=>x.message.startsWith('PW INPUT')).at(-1)?.message||'';
 const left=w===480?[51,424]:[31,209],gas=w===480?[432,420]:[289,205],turbo=w===480?[232,430]:[289,150];
 const held=()=>{const p=host.screen.getContext('2d').getImageData(w===480?24:18,h-(w===480?88:46),1,1).data;return p[0]>30&&p[0]<80&&p[1]>120&&p[1]<180&&p[2]>240;};
 try{
  host.start();tick(40);shot('select');
  const surface=[...host.handles.values()].find(x=>x.kind==='surface');assert.equal(surface.w,w);assert.equal(surface.h,h);assert.equal(surface.flags&2,0);
  for(let i=0;i<9;i++){tap(w===480?246:w-166,w===480?351:154);tick(40);}
  assert.equal(new Set(host.logs.filter(x=>/^map .* loaded /.test(x.message)).map(x=>x.message.split(' ')[1])).size,9);
  for(let i=0;i<15;i++)tap(w===480?444:w-24,w===480?351:154);
  shot('all-cars');tap(w/2,h-(w===480?48:38));tick(110);
  if(w===320){
   // Thumb zones include screen edges and space above the compact artwork.
   touch(0,1,0,145);tick(2);assert.match(input(),/left=1 right=0/);
   touch(0,2,80,145);tick(2);assert.match(input(),/left=0 right=1/);
   touch(0,3,80,145);tick(2);
   touch(0,1,319,239);tick(2);assert.match(input(),/brake=1 gas=0/);
   touch(0,3,319,239);tick(2);
  }
  touch(0,1,...left);tick(2);assert.match(input(),/left=1 right=0/);assert(held());shot('left');
  if(w===320){tap(40,56,1);assert(held(),'AUTO toggle preserves a held steering finger');}
  touch(1,1,...gas);tick(35);assert.match(input(),/left=1.*gas=1/);shot('steer-gas');
  touch(1,3,...gas);tick(2);assert.match(input(),/left=1.*gas=0/);assert(held(),'Gas UP preserves steering');
  touch(1,1,...turbo);tick(30);touch(1,2,w/2,110);tick(3);touch(1,2,...turbo);tick(30);
  assert.equal(host.logs.filter(x=>x.message.startsWith('PW TURBO')).length,1,'One turbo per held contact, including leaving and reentering');
  touch(1,3,...turbo);touch(0,3,...left);tick(2);assert(!held());shot('drive');
  tap(w-40,16);shot('paused');const paused=host.screen.toBuffer('image/png');tick(50);assert.deepEqual(host.screen.toBuffer('image/png'),paused);
  tap(w/2,h/2+12);touch(0,2,...left);tick(2);assert(!held(),'Resume ignores orphan MOVE events');
  touch(0,1,...left);tick(2);host.setPaused(true);host.setPaused(false);tick(2);assert(!held(),'System pause clears controls');
  touch(0,3,...left);tap(w/2,h/2+12);tick(40);shot('resumed');
  assert.equal(host.metrics.abiCalls.aster_accel_read||0,0);assert.equal(host.metrics.abiCalls.aster_gyro_read||0,0);
  assert(!host.logs.some(x=>x.level>=3&&x.message.startsWith('PixelWheels')),JSON.stringify(host.logs.slice(-10)));
  console.log(`${w}x${h}: native canvas, 9 maps/15 cars, independent steering/gas, turbo latch and pause/resume passed`);
 }finally{host.stop();}
}
