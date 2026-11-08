'use strict';
// Project-local resource mount for the official simulator and its worker.
// Device packages still use the normal SDK limits and external Store bundles.
const fs=require('node:fs'),path=require('node:path');
const root=path.resolve(__dirname,'..');
const sim=process.env.ASTER_SIM||path.resolve(root,'../../aster-sim');
const packages=require(path.join(sim,'src/package'));
const load=packages.loadPackage;
packages.loadPackage=function(file,options){
 const pkg=load(file,options);
 if(pkg.manifest.id==='aster.pal'){
  function add(dir,prefix=''){
   for(const e of fs.readdirSync(dir,{withFileTypes:true})){
    const p=path.join(dir,e.name),name=prefix?prefix+'/'+e.name:e.name;
    if(e.isDirectory())add(p,name);else pkg.assets.set(name,fs.readFileSync(p));
   }
  }
  add(path.join(root,'dist/resources'));
 }
 return pkg;
};
