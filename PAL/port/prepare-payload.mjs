import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import {fileURLToPath} from 'node:url';
const here=path.dirname(fileURLToPath(import.meta.url));
const data=fs.readFileSync(path.join(here,'resources/pal_full.pak'));
const output=path.join(here,'build/device-parts');fs.mkdirSync(output,{recursive:true});
const parts=[];
for(let offset=0,index=0;offset<data.length;offset+=1048576,index++){
 const name='pal_full.'+String(index).padStart(2,'0');
 const part=data.subarray(offset,Math.min(data.length,offset+1048576));
 fs.writeFileSync(path.join(output,name),part);
 parts.push({name,size:part.length,sha256:crypto.createHash('sha256').update(part).digest('hex')});
}
fs.writeFileSync(path.join(output,'parts.json'),JSON.stringify({bytes:data.length,parts},null,2));
console.log(`Prepared ${parts.length} resource segments, ${data.length} bytes`);
