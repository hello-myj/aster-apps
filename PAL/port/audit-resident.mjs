import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
const here=path.dirname(fileURLToPath(import.meta.url));
const profile=JSON.parse(fs.readFileSync(path.join(here,'memory-profile.json'),'utf8'));
const data=fs.readFileSync(path.join(here,'resources/pal_full.pak'));
const module=new WebAssembly.Module(fs.readFileSync(path.join(here,'build/resource-audit.wasm')));
let memory;
const instance=new WebAssembly.Instance(module,{env:{pal_resource_read:(offset,dst,size)=>{
 if(offset>data.length||size>data.length-offset||dst+size>memory.buffer.byteLength)return 0;
 new Uint8Array(memory.buffer,dst,size).set(data.subarray(offset,offset+size));return 1;
}}});
memory=instance.exports.memory;
let mask=0;
for(const id of profile.residentArchiveIds){if(!Number.isInteger(id)||id<1||id>31)throw Error('Invalid resident archive ID');mask|=1<<id;}
const bytes=instance.exports.audit_resident(data.length,mask,profile.residentPackBytes);
if(!bytes)throw Error('Actual resident builder rejects selected data/capacity');
const report={sourcePackBytes:data.length,residentArchiveIds:profile.residentArchiveIds,residentBytes:bytes,capacityBytes:profile.residentPackBytes,spareBytes:profile.residentPackBytes-bytes};
fs.writeFileSync(path.join(here,'build/resident-audit.json'),JSON.stringify(report,null,2));
console.log(JSON.stringify(report,null,2));
