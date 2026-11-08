// Run after current-flow.cjs at 480x480 and 320x240. Compare real gameplay
// screenshots at the same deterministic point, excluding S3 control overlays.
// Scene pixels use nearest-neighbour 3:2 scaling; native UI must differ from
// simply scaling the original low-resolution text.
const assert=require('node:assert/strict');
const fs=require('node:fs');
const path=require('node:path');
const {createRequire}=require('node:module');
const simRequire=createRequire(path.join(process.env.ASTER_SIM||path.resolve(__dirname,'../../../aster-sim'),'package.json'));
const {PNG}=simRequire('pngjs');
const ui=path.resolve(__dirname,'../tmp_ui');
const read=size=>PNG.sync.read(fs.readFileSync(path.join(ui,size,'aster-350.png')));
const p4=read('480x480'),s3=read('320x240');
assert.equal(p4.width,480);assert.equal(s3.width,320);
let nativeTextDifferences=0,scenePixels=0;
for(let y=0;y<300;y++){
 for(let x=0;x<480;x++){
  const sx=Math.floor(x*2/3),sy=Math.floor(y*2/3);
  if(sy>=110&&(sx<128||sx>=240))continue;
  const a=(y*480+x)*4,b=(sy*320+sx)*4;
  const actual=p4.data.subarray(a,a+4),scaled=s3.data.subarray(b,b+4);
  if(sx>=120&&sx<210&&sy>=70&&sy<130){
   if(!actual.equals(scaled))nativeTextDifferences++;
  }else{
   assert.deepEqual(actual,scaled,`Unchanged scene pixel ${sx},${sy}`);scenePixels++;
  }
 }
}
assert(nativeTextDifferences>100,'P4 text must use native glyphs, not a scaled 12px bitmap');
console.log(`PASS ${scenePixels} nearest-neighbour scene pixels and ${nativeTextDifferences} independently rendered UI pixels`);
