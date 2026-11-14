'use strict';
// Exercise the real Store WASM: editable source selection, units, live speed and cancel.
const fs=require('node:fs'),path=require('node:path'),http=require('node:http'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const {Host}=require('../../../aster-sim/src/host'),{loadPackage}=require('../../../aster-sim/src/package'),{MemoryFS}=require('../../../aster-sim/src/memory-fs');
const pkg=loadPackage(path.resolve(process.argv[2]||'AppStore/dist/riscv/aster.store.ast'));
const program=fs.readFileSync('TouchTest/dist/riscv/aster.touchtest.ast'),resource=Buffer.alloc(2621440,37),payloads=[program,resource],release='2'.repeat(64);
const sha=b=>crypto.createHash('sha256').update(b).digest('hex');let base;
const server=http.createServer((req,res)=>{
 const u=new URL(req.url,'http://localhost');u.pathname=u.pathname.replace(/^\/long-[a]+/,'');
 if(u.pathname==='/v1/catalog'){res.end(JSON.stringify({schema:1,total:1,next:-1,items:[{id:'aster.touchtest',name:'下载测试',version:'9.0.0',release,bytes:program.length+resource.length,file_count:2,description:'下载速度和大小单位验证',permissions:[]}]}));return;}
 if(u.pathname===`/v1/releases/${release}`){res.end(JSON.stringify({schema:1,release,id:'aster.touchtest',version:'9.0.0',target:'wasm',boards:['aster.simulator'],abi_min:[4,1,0],requires:['display','touch'],screens:[],permissions:[],total:2,next:-1,files:payloads.map((data,index)=>({index,role:index?'resource':'package',path:index?'resource.bin':'package.ast',bytes:data.length,sha256:sha(data)}))}));return;}
 const match=u.pathname.match(/\/files\/([01])$/);
 if(match){const data=payloads[+match[1]],range=/bytes=(\d+)-(\d+)/.exec(req.headers.range||'');assert(range);let at=+range[1];const end=+range[2];res.writeHead(206,{'Content-Length':end-at+1,'Content-Range':`bytes ${at}-${end}/${data.length}`,ETag:`"${sha(data)}"`});const timer=setInterval(()=>{if(at>end){clearInterval(timer);res.end();return;}let n=Math.min(end+1,at+8192);res.write(data.subarray(at,n));at=n;},30);res.on('close',()=>clearInterval(timer));return;}
 res.writeHead(404);res.end();
});
const delay=()=>new Promise(r=>setTimeout(r,5));
const tap=(h,x,y)=>{h.input({type:'touch',action:1,x,y});h.input({type:'touch',action:3,x,y});};
async function frames(h,n=20){for(let i=0;i<n;i++){h.tick();await delay();assert.equal(h.diagnostics().trap,null);}}
const output=path.resolve('AppStore/artifacts/download-ui');fs.mkdirSync(output,{recursive:true});
(async()=>{
 await new Promise(r=>server.listen(0,'127.0.0.1',r));base=`http://127.0.0.1:${server.address().port}`;
 for(const [width,height] of [[240,320],[320,240],[480,480]]){
  const selected={...pkg,manifest:{...pkg.manifest,orientation:width<height?'portrait':'landscape'}};
  const h=new Host(selected,{width,height,network:true,noAudio:true,memoryStorage:new MemoryFS()});
  const initialUrl=base+'/long-'+'a'.repeat(110);
  h.storageFs.writeFileSync(path.join(h.root,'kv',Buffer.from('store.url').toString('hex')),Buffer.from(initialUrl+'\0'));
  let texts=[];const imports=h.imports.bind(h);h.imports=()=>{const api=imports(),exec=api.aster_gfx_exec;api.aster_gfx_exec=(handle,p,n,arena,len,...rest)=>{if(n<4096)for(let i=0;i<n;i++)if((h.u32(p+i*24)&255)===7)texts.push(h.string(arena+h.u32(p+i*24+12),h.u32(p+i*24+16)&65535));return exec(handle,p,n,arena,len,...rest);};return api;};
  try{
   h.start();await frames(h,100);assert(h.logs.some(x=>x.message.includes('STORE version')));
   const listBottom=height-((width<height&&width<320)||height<300?44:56);
   tap(h,30,listBottom+12);await frames(h);assert(texts.includes('服务器设置'));
   assert(texts.some(t=>t.startsWith('http://127.0.0.1:')),'Settings must initially show the URL prefix');
   const tall=height>=400,small=height<300,sourceY=tall?54:small?26:42,keysY=tall?152:small?90:120,step=tall?46:small?19:24,actionY=tall?390:small?188:246,settingsBottom=height-(tall?48:small?28:40),aw=Math.floor((width-32)/5),kw=Math.floor((width-24)/10);
   const action=async i=>{tap(h,12+i*(aw+2)+aw/2,actionY+8);await frames(h,22);};
   const key=async c=>{const i='1234567890qwertyuiopasdfghjklzxcvbnm.:/-_'.indexOf(c);assert(i>=0);tap(h,12+(i%10)*kw+kw/2,keysY+Math.floor(i/10)*step+8);await frames(h,22);};
   let offset=0;const now=h.now.bind(h);h.now=()=>now()+offset;
   const hold=async(i,ms)=>{const x=12+i*(aw+2)+aw/2,y=actionY+8;h.input({type:'touch',action:1,x,y});await frames(h,1);for(let n=0;n<ms;n+=50){offset+=50;await frames(h,1);}h.input({type:'touch',action:3,x,y});await frames(h,22);};
   await action(0);await action(2);await key('z');assert(texts.some(t=>t.startsWith('hZttp://')),'A click must move exactly one character');
   await action(3);await action(1);
   let mark=texts.length;await hold(2,1600);await key('z');
   assert(texts.slice(mark).some(t=>t.length>3&&t.includes('Z')&&!t.startsWith('hZ')),'Holding right must repeatedly advance the caret');
   mark=texts.length;await hold(1,2500);await key('z');assert(texts.slice(mark).some(t=>t.startsWith('Zhttp://')),'Holding left must return to the beginning');
   // Dragging off an arrow stops repeat and must not activate a different key on release.
   const rx=12+2*(aw+2)+aw/2;h.input({type:'touch',action:1,x:rx,y:actionY+8});
   h.input({type:'touch',action:2,x:20,y:keysY+8});offset+=1000;await frames(h,22);
   h.input({type:'touch',action:3,x:20,y:keysY+8});await frames(h,22);
   await key('z');assert(texts.some(t=>t.startsWith('ZhZttp://')),'Dragging off must cancel repeat without typing on release');
   await action(0);
   await action(4);await action(0);await key('a');await action(1);await key('1');await action(3);await action(2);assert(texts.includes('A'));
   tap(h,35,sourceY+10);await frames(h);fs.writeFileSync(path.join(output,`${width}-default.png`),h.screen.toBuffer('image/png'));
   tap(h,width-40,sourceY+10);await frames(h);await action(4);await action(0);
   tap(h,12+5*kw,keysY+4*step+8);await frames(h,2);
   tap(h,width-50,settingsBottom+10);await frames(h,2);
   assert.equal(h.storageFs.readFileSync(path.join(h.root,'kv',Buffer.from('store.url').toString('hex'))).toString(),initialUrl+'\0','A protocol prefix without a host must not be saved');
   await action(4);tap(h,12+2*kw,keysY+4*step+8);await frames(h,2);
   for(const c of base.slice(7))await key(c);
   fs.writeFileSync(path.join(output,`${width}-settings.png`),h.screen.toBuffer('image/png'));
   tap(h,width-50,settingsBottom+10);await frames(h,100);
   const saved=h.storageFs.readFileSync(path.join(h.root,'kv',Buffer.from('store.url').toString('hex'))).toString();assert.equal(saved,base+'\0');
   // Row body opens details; installation is in the detail footer.
   tap(h,30,(width<height&&width<320)||height<300?90:130);await frames(h);assert(texts.some(t=>t.includes(' MB')));
   tap(h,width-35,listBottom+12);
   const end=Date.now()+10000;while(Date.now()<end&&!texts.some(t=>/速度 [1-9][0-9.]* KB\/s/.test(t))){await frames(h,3);}
   assert(texts.some(t=>/速度 [1-9][0-9.]* KB\/s/.test(t)),JSON.stringify(h.logs));
   fs.writeFileSync(path.join(output,`${width}-download.png`),h.screen.toBuffer('image/png'));
   tap(h,width/2,listBottom+12);await frames(h);assert(h.logs.some(x=>x.message.includes('下载已取消')));
   assert(!h.pendingInstall);console.log('PASS source editing, persistence, MB sizes, KB/s speed, cancel:',width,height);
  }finally{h.stop();}
 }
})().catch(e=>{console.error(e);process.exitCode=1;}).finally(()=>{server.closeAllConnections();server.close();});
