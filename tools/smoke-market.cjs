'use strict';
// Startup/render/resource smoke check. Does not claim device gameplay coverage.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'..');
const {Host}=require('../../aster-sim/src/host');
const {loadPackage}=require('../../aster-sim/src/package');
const projects=process.argv.slice(2);
if(!projects.length)throw Error('Pass project names to smoke-test');
function assets(dir,prefix,map){for(const ent of fs.readdirSync(dir,{withFileTypes:true})){
  const name=prefix?prefix+'/'+ent.name:ent.name,full=path.join(dir,ent.name);
  if(ent.isDirectory())assets(full,name,map);else if(ent.isFile())map.set(name,fs.readFileSync(full));
}}
let failed=false;
for(const project of projects){let host;
 try{
  const dir=path.join(root,project),files=fs.readdirSync(path.join(dir,'build/sim/dist/riscv')).filter(x=>x.endsWith('.ast'));
  assert.equal(files.length,1);
  const pkg=loadPackage(path.join(dir,'build/sim/dist/riscv',files[0]));
  assets(path.join(dir,project==='GoldMiner'?'dist/riscv/resources':'dist/resources'),'',pkg.assets);
  let clock=0;host=new Host(pkg,{width:480,height:480,clock:()=>clock});host.start();
  for(let frame=0;frame<240;frame++){clock+=33;host.tick();if(host.exitCode!==null)break;}
  const diag=host.diagnostics();assert.equal(diag.trap,null,JSON.stringify(host.logs));
  assert.equal(host.exitCode,null,JSON.stringify(host.logs));
  assert.ok(diag.presents>0,'No frame presented');
  const bad=host.logs.filter(x=>/missing|resource error|load failed|fatal|corrupt/i.test(x.message)&&
    !(project==='OpenTyrian'&&/^'tyrian\.(cfg|sav)' is invalid or missing\.$/.test(x.message)));
  assert.equal(bad.length,0,JSON.stringify(bad));
  const out=path.join(dir,'artifacts/market');fs.mkdirSync(out,{recursive:true});
  fs.writeFileSync(path.join(out,'startup-480.png'),host.screen.toBuffer('image/png'));
  fs.writeFileSync(path.join(out,'smoke.json'),JSON.stringify({diagnostics:diag,logs:host.logs},null,2));
  console.log(JSON.stringify({project,ok:true,presents:diag.presents,resourceFiles:pkg.assets.size}));
 }catch(error){failed=true;console.error(project+': '+error.stack);if(host)console.error(JSON.stringify(host.logs.slice(-20)));}
 finally{if(host)host.stop();}
}
process.exitCode=failed?1:0;
