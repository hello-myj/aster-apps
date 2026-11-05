'use strict';
// Actual engines + new full-screen surfaces; verifies gesture feedback and
// pause cleanup independently of whatever animation is playing in the viewport.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {Host}=require('../../../aster-sim/src/host');
const {loadPackage}=require('../../../aster-sim/src/package');
const root=path.resolve(__dirname,'../..');
function assets(dir,prefix,map){for(const e of fs.readdirSync(dir,{withFileTypes:true})){const n=prefix?prefix+'/'+e.name:e.name,p=path.join(dir,e.name);if(e.isDirectory())assets(p,n,map);else map.set(n,fs.readFileSync(p));}}
for(const project of process.argv.slice(2)){
 const dir=path.join(root,project),file=fs.readdirSync(path.join(dir,'build/sim/dist/riscv')).find(x=>x.endsWith('.ast'));
 const pkg=loadPackage(path.join(dir,'build/sim/dist/riscv',file));assets(path.join(dir,'dist/resources'),'',pkg.assets);
 let time=0;const h=new Host(pkg,{width:480,height:480,clock:()=>time});
 const tick=(n=1)=>{for(let i=0;i<n;i++){time+=33;h.tick();assert.equal(h.exitCode,null,JSON.stringify(h.logs.slice(-8)));}};
 const touch=(slot,action,x,y)=>h.input({type:'touch',slot,action,x,y});
 const stick=()=>Buffer.from(h.screen.getContext('2d').getImageData(20,350,128,124).data);
 const out=path.join(dir,'artifacts/p4-layout');fs.mkdirSync(out,{recursive:true});
 const shot=name=>fs.writeFileSync(path.join(out,name+'.png'),h.screen.toBuffer('image/png'));
 try{
  h.start();tick(240);const surface=[...h.handles.values()].find(x=>x.kind==='surface');
  assert.equal(surface.w,480);assert.equal(surface.h,480);assert.equal(surface.flags&2,0,'No SCALE_FIT intermediate canvas');
  const centered=stick();shot('idle');
  touch(0,1,84,414);touch(0,2,40,380);tick(2);assert.notDeepEqual(stick(),centered);shot('stick');
  touch(1,1,project==='OpenOMF'?420:404,project==='OpenOMF'?414:422);tick(2);shot('multi-touch');
  touch(1,3,420,422);tick(2);assert.notDeepEqual(stick(),centered,'Lifting the action finger preserves the joystick');
  touch(0,3,40,380);tick(2);assert.deepEqual(stick(),centered,'Stick returns to center');
  touch(0,1,84,414);touch(0,2,40,380);tick(2);h.setPaused(true);h.setPaused(false);tick(2);
  assert.deepEqual(stick(),centered,'Pause/resume clears held joystick');shot('resumed');
  tick(240);shot('steady');
  assert.equal(h.diagnostics().trap,null);console.log(`${project}: native-size canvas, joystick, multitouch and resume passed`);
 }finally{h.stop();}
}
