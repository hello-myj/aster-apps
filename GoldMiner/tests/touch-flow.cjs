'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {Host}=require('../../../aster-sim/src/host');
const {loadPackage}=require('../../../aster-sim/src/package');
const project=path.resolve(__dirname,'..');
function resources(dir,rel,map){for(const e of fs.readdirSync(dir,{withFileTypes:true})){
 const name=rel?rel+'/'+e.name:e.name,full=path.join(dir,e.name);
 if(e.isDirectory())resources(full,name,map);else map.set(name,fs.readFileSync(full));
}}
for(const [target,width,height] of [['xtensa',320,240],['riscv',480,480]]){
 const pkg=loadPackage(path.join(project,`dist/${target}/aster.goldminer.ast`));
 resources(path.join(project,`dist/${target}/resources`),'',pkg.assets);
 let clock=0;const h=new Host(pkg,{width,height,clock:()=>clock}).start();
 const scale=Math.min(width/320,height/240),top=(height-240*scale)/2;
 const point=(x,y)=>({x:Math.floor(x*scale),y:Math.floor(top+y*scale)});
 const input=(action,x,y,slot=0)=>h.input({type:'touch',action,slot,...point(x,y)});
 const tick=(n=1)=>{for(let i=0;i<n;i++){clock+=33;h.tick();assert.equal(h.exitCode,null);}};
 const tap=(x,y)=>{input(1,x,y);input(3,x,y);tick();};
 const scene=()=>h.logs.filter(x=>x.message.startsWith('GoldMiner scene=')).at(-1)?.message;
 const out=path.join(project,'artifacts/touch',target);fs.mkdirSync(out,{recursive:true});
 const shot=name=>fs.writeFileSync(path.join(out,name+'.png'),h.screen.toBuffer('image/png'));
 try{
  tick();shot('menu');assert.equal(scene(),'GoldMiner scene=MENU');
  input(1,70,155);assert.equal(scene(),'GoldMiner scene=MENU','Down alone must not activate');
  input(2,130,155);input(3,130,155);tick();assert.equal(scene(),'GoldMiner scene=MENU','Drag cancelled');
  input(1,70,155);input(1,100,155,1);input(3,70,155);input(3,100,155,1);tick();assert.equal(scene(),'GoldMiner scene=MENU','Multi-touch cancelled');
  tap(70,180);assert.equal(scene(),'GoldMiner scene=HIGH SCORE');tap(160,210);assert.equal(scene(),'GoldMiner scene=MENU');
  const original=h.screen.toBuffer('image/png');tap(286,14);shot('pause');assert.ok(!h.screen.toBuffer('image/png').equals(original));
  tap(100,90);assert.ok(h.screen.toBuffer('image/png').equals(original),'Resume restores menu');
  if(height===480){h.input({type:'touch',action:1,x:100,y:20});h.input({type:'touch',action:3,x:100,y:20});tick();assert.equal(scene(),'GoldMiner scene=MENU','Letterbox ignored');}
  tap(70,155);assert.equal(scene(),'GoldMiner scene=GOAL');shot('goal');
  tap(160,120);assert.equal(scene(),'GoldMiner scene=GAME');tick(20);shot('game');
  const released=h.logs.filter(x=>x.message.startsWith('GoldMiner release')).length;
  tap(160,150);assert.equal(h.logs.filter(x=>x.message.startsWith('GoldMiner release')).length,released+1);
  tick(120);shot('playing');
  tap(286,14);shot('game-pause');tap(100,123);assert.equal(scene(),'GoldMiner scene=MENU');
  if(height===480){const p=h.screen.getContext('2d').getImageData(100,479,1,1).data;assert.deepEqual(Array.from(p.slice(0,3)),[0,0,0]);}
  fs.writeFileSync(path.join(out,'report.json'),JSON.stringify({diagnostics:h.diagnostics(),logs:h.logs},null,2));
  console.log(JSON.stringify({ok:true,target,width,height,resourceScale:target==='riscv'?1.5:1,gameplay:true}));
 }finally{h.stop();}
}
