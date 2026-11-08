const fs=require('node:fs');
const assert=require('node:assert/strict');
const path=require('node:path');
const {Host}=require(require('node:path').join(process.env.ASTER_SIM || require('node:path').resolve(__dirname,'../../../aster-sim'),'src/host'));
const {loadPackage}=require(require('node:path').join(process.env.ASTER_SIM || require('node:path').resolve(__dirname,'../../../aster-sim'),'src/package'));
const base=__dirname;
const pkg=loadPackage(path.join(base,'build/pal-research.ast'));
if(process.env.PAL_TEST_SEGMENTS==='1'){
 const data=pkg.assets.get('resources/pal_full.pak');pkg.assets.delete('resources/pal_full.pak');
 for(let offset=0,index=0;offset<data.length;offset+=1048576,index++)pkg.assets.set('resources/pal_full.'+String(index).padStart(2,'0'),data.subarray(offset,Math.min(data.length,offset+1048576)));
}
if(process.env.PAL_TEST_MINIMUM==='1'){
 pkg.wasm=fs.readFileSync(path.join(base,'build/minimum.wasm'));
 pkg.manifest=JSON.parse(fs.readFileSync(path.join(base,'build/minimum-manifest.json'),'utf8'));
}
const resolution=process.env.PAL_TEST_RESOLUTION||'320x480';
const [width,height]=resolution.split('x').map(Number);
assert.ok(width>0&&height>0,'PAL_TEST_RESOLUTION must be WIDTHxHEIGHT');
const ui=path.join(base,'../tmp_ui',resolution);
fs.mkdirSync(ui,{recursive:true});
const testState=path.join(base,'build/smoke-state-'+resolution);
fs.mkdirSync(path.join(testState,'shared/music'),{recursive:true});
fs.cpSync(path.join(base,'sim-state/shared/music/pal'),path.join(testState,'shared/music/pal'),{recursive:true});
if(process.env.PAL_TEST_SHARED_RESOURCES==='1'){
 const shared=path.join(testState,'shared/music/pal/resources');fs.mkdirSync(shared,{recursive:true});
 for(const [name,data] of pkg.assets){
  if(name.startsWith('resources/pal_full.')){
   fs.writeFileSync(path.join(shared,path.basename(name)),data);pkg.assets.delete(name);
  }
 }
}
const audioOps=[];
let clock=0;
const h=new Host(pkg,{width,height,state:testState,send:m=>{
  if(m.type==='log')console.log(m.message);
  if(m.type==='audio')audioOps.push({op:m.op,bytes:m.data?.length||0,handle:m.handle});
}});
h.now=()=>clock;
if(process.env.PAL_TEST_ORDINARY_SURFACE==='1'){
 const getImports=h.imports.bind(h);
 h.imports=()=>{
  const imports=getImports(),create=imports.aster_surface_create;
  imports.aster_surface_create=(p,n,out)=>{
   const flags=h.u32(p+20);h.u32(p+20,flags&~1);
   try{return create(p,n,out);}finally{h.u32(p+20,flags);}
  };
  return imports;
 };
}
h.start();
assert.equal(h.exitCode,null,'Landscape app must initialize');
assert.ok(h.width>h.height,'Manifest must select landscape before init');
const joyR=h.width>=480?48:39,joyX=joyR+10,joyY=h.height-joyR-14;
const actionR=h.width>=480?28:22;
const confirm={x:h.width-actionR-12,y:h.height-actionR-14-actionR*2-14};
const key=(code,pressed)=>h.input({type:'key',code,pressed});
for(let i=0;i<1600&&h.exitCode===null;i++){
  clock+=17;
  if([120,360,600,900].includes(i))key(28,1);
  if([124,364,604,904].includes(i))key(28,0);
  h.tick();
  if([350,590,890,1500].includes(i))fs.writeFileSync(path.join(ui,`aster-${i}.png`),h.screen.toBuffer('image/png'));
}
fs.writeFileSync(path.join(ui,'aster-gameplay.png'),h.screen.toBuffer('image/png'));
for(let i=0;i<4000&&h.exitCode===null;i++){
  clock+=17;
  if(i%30===0)h.input({type:'touch',action:1,...confirm});
  if(i%30===4)h.input({type:'touch',action:3,...confirm});
  h.tick();
}
fs.writeFileSync(path.join(ui,'aster-after-dialogue.png'),h.screen.toBuffer('image/png'));
key(106,1);for(let i=0;i<100;i++){clock+=17;h.tick();}key(106,0);
fs.writeFileSync(path.join(ui,'aster-walk.png'),h.screen.toBuffer('image/png'));
key(1,1);for(let i=0;i<4;i++){clock+=17;h.tick();}key(1,0);for(let i=0;i<40;i++){clock+=17;h.tick();}
fs.writeFileSync(path.join(ui,'aster-menu.png'),h.screen.toBuffer('image/png'));
const advance=(n)=>{for(let i=0;i<n&&h.exitCode===null;i++){clock+=17;h.tick();}};
const quickTap=(code)=>{key(code,1);key(code,0);advance(20);};
const beforeQuickTap=h.instance.exports.pal_debug_main_menu();
quickTap(108);
assert.notEqual(h.instance.exports.pal_debug_main_menu(),beforeQuickTap,'A down/up pair queued before a frame must move the menu');
quickTap(103);
assert.equal(h.instance.exports.pal_debug_main_menu(),beforeQuickTap,'Quick up must restore menu selection');
console.log('PASS quick tap down/up before frame');

const touchTap=(x,y)=>{
 const before=h.presentCount;
 h.input({type:'touch',action:1,x,y});
 assert.equal(h.presentCount,before+1,'Press must publish exactly one complete frame, including labels');
 h.input({type:'touch',action:3,x,y});
 assert.equal(h.presentCount,before+2,'Release must publish exactly one complete frame');
 advance(20);
};
touchTap(joyX,joyY+joyR*0.75);
assert.notEqual(h.instance.exports.pal_debug_main_menu(),beforeQuickTap,'Left-side touch down must move selection');
touchTap(joyX,joyY-joyR*0.75);
assert.equal(h.instance.exports.pal_debug_main_menu(),beforeQuickTap,'Left-side touch up must restore selection');
touchTap(h.width/2,h.height/2);
assert.equal(h.instance.exports.pal_debug_main_menu(),beforeQuickTap,'Game viewport must not act as a touch button');
touchTap(joyX,joyY);
assert.equal(h.instance.exports.pal_debug_main_menu(),beforeQuickTap,'D-pad center must not send a direction');
h.input({type:'touch',action:1,x:joyX,y:joyY+joyR*0.75});advance(20);
assert.notEqual(h.instance.exports.pal_debug_main_menu(),beforeQuickTap,'Held direction down must move selection');
h.input({type:'touch',action:2,x:joyX,y:joyY-joyR*0.75});advance(20);
assert.equal(h.instance.exports.pal_debug_main_menu(),beforeQuickTap,'Sliding to up must change direction');
h.input({type:'touch',action:3,x:joyX,y:joyY});advance(20);
fs.writeFileSync(path.join(ui,'aster-overlay-menu.png'),h.screen.toBuffer('image/png'));
console.log('PASS D-pad directions, slide, center, release, atomic frames and inert viewport');
const tap=(code)=>{key(code,1);advance(20);key(code,0);advance(20);console.log('tap',code,'main',h.instance.exports.pal_debug_main_menu(),'system',h.instance.exports.pal_debug_system_menu());};
tap(108);tap(108);tap(108);tap(28);advance(20);
fs.writeFileSync(path.join(ui,'aster-system.png'),h.screen.toBuffer('image/png'));
tap(28);advance(20);
fs.writeFileSync(path.join(ui,'aster-save-slots.png'),h.screen.toBuffer('image/png'));
tap(28);advance(30);
const savePath=path.join(h.root,'data/1.rpg');
assert.ok(h.storageFs.existsSync(savePath),'Normal save menu must create slot 1');
assert.ok(h.storageFs.readFileSync(savePath).length>1000,'Save must contain actual game state');
const savedViewport=h.instance.exports.pal_debug_viewport();
const savedScene=h.instance.exports.pal_debug_scene();
key(105,1);advance(100);key(105,0);advance(20);
const movedViewport=h.instance.exports.pal_debug_viewport();
assert.notEqual(movedViewport,savedViewport,'Normal movement must change the viewport');
tap(1);tap(28);tap(108);tap(28);tap(28);advance(100);
assert.equal(h.instance.exports.pal_debug_scene(),savedScene,'Load must restore scene');
assert.equal(h.instance.exports.pal_debug_viewport(),savedViewport,'Load must restore position');
fs.writeFileSync(path.join(ui,'aster-loaded.png'),h.screen.toBuffer('image/png'));
console.log('PASS normal save -> walk -> normal load restores position');
assert.ok(audioOps.some(m=>m.op==='start'&&m.bytes>1000),'Actual gameplay must start nonempty BGM');
assert.ok(audioOps.some(m=>m.op==='effect-load'&&m.bytes>44),'Actual gameplay must load PCM effects');
assert.ok(audioOps.some(m=>m.op==='effect'),'Actual gameplay must play an effect');
const oldSession=h.audio.session;
h.input({type:'audio-feedback',session:oldSession,event:'ended'});advance(2);
assert.notEqual(h.audio.session,oldSession,'Looped BGM must restart after browser completion');
h.input({type:'pause'});assert.equal(h.audio.state,2,'Pause must pause audio');
h.input({type:'pause'});assert.equal(h.audio.state,1,'Resume must resume audio');
console.log('PASS BGM, effect, BGM loop, pause/resume');
console.log('Audio messages:',audioOps.map(x=>({op:x.op,bytes:x.bytes})).filter(x=>['start','effect','effect-load'].includes(x.op)));
console.log({frames:h.frame,presents:h.presentCount,exit:h.exitCode});
console.log({linearBytes:h.memory.buffer.byteLength,hostAllocationEnd:h.heap,heapBase:Number(h.instance.exports.__heap_base.value)});
if(h.exitCode!==null&&h.exitCode!==0)process.exitCode=1;
h.stop();
