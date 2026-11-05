const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {Host}=require('../../../aster-sim/src/host');const {loadPackage}=require('../../../aster-sim/src/package');
const root=path.resolve(__dirname,'../..'),dir=path.join(root,'OpenOMF');
const p=loadPackage(path.join(dir,'build/sim/dist/riscv/aster.openomf.ast'));
function assets(d,pre=''){for(const e of fs.readdirSync(d,{withFileTypes:true})){let n=pre+e.name,x=path.join(d,e.name);if(e.isDirectory())assets(x,n+'/');else p.assets.set(n,fs.readFileSync(x));}}assets(path.join(dir,'dist/resources'));
let t=0;const h=new Host(p,{width:480,height:480,clock:()=>t});
function tick(n){for(let i=0;i<n;i++){t+=33;h.tick();assert.equal(h.exitCode,null,JSON.stringify(h.logs.slice(-10)));}}
function tap(){h.input({type:'touch',slot:0,action:1,x:326,y:428});tick(3);h.input({type:'touch',slot:0,action:3,x:326,y:428});tick(100);}
try{h.start();tick(200);for(let i=0;i<8;i++){tap();console.log('tap',i,h.logs.filter(x=>x.message.includes('OMF SCENE')).slice(-1).map(x=>x.message).join('\n'));if(h.logs.some(x=>/OMF SCENE.*SCENE_ARENA/.test(x.message)))break;}
assert(h.logs.some(x=>/OMF SCENE.*SCENE_ARENA/.test(x.message)),'Touch selection must reach a real fight');tick(300);
fs.mkdirSync(path.join(dir,'artifacts/selection-fix'),{recursive:true});fs.writeFileSync(path.join(dir,'artifacts/selection-fix/fight.png'),h.screen.toBuffer('image/png'));fs.writeFileSync(path.join(dir,'artifacts/selection-fix/sim.json'),JSON.stringify(h.logs,null,2));console.log('Touch-only pilot, robot, VS and arena passed');}finally{h.stop();}
