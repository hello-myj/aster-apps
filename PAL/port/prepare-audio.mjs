import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {createRequire} from 'node:module';
import {spawnSync} from 'node:child_process';
const here=path.dirname(fileURLToPath(import.meta.url));
const require=createRequire(import.meta.url);
const ffmpeg=require('@ffmpeg-installer/ffmpeg').path;
const pack=fs.readFileSync(path.join(here,'resources/pal_full.pak'));
const module=new WebAssembly.Module(fs.readFileSync(path.join(here,'build/audio/rix-render.wasm')));
const synth=new WebAssembly.Instance(module,{}).exports;
synth.__wasm_call_ctors();
const musicDir=path.join(here,'sim-state/shared/music/pal');
const sfxDir=path.join(musicDir,'sfx');
fs.mkdirSync(sfxDir,{recursive:true});
function archive(id){
 const count=pack.readUInt16LE(8),table=pack.readUInt32LE(12);
 for(let i=0;i<count;i++){
  const row=table+i*12;if(pack.readUInt16LE(row)!==id)continue;
  const chunks=pack.readUInt16LE(row+2),offset=pack.readUInt32LE(row+4);
  return Array.from({length:chunks},(_,j)=>{
   const p=offset+j*16,start=pack.readUInt32LE(p),n=pack.readUInt32LE(p+4);
   if(start+n>pack.length||pack.readUInt16LE(p+10))throw Error('Invalid native audio chunk');
   return pack.subarray(start,start+n);
  });
 }throw Error('Missing audio archive '+id);
}
const music=archive(11),effects=archive(19);
let songs=0,nonzeroMusic=0;
for(let id=0;id<music.length;id++){
 const rix=music[id];if(!rix.length)continue;
 if(rix.length>16384)throw Error('RIX owner capacity exceeded');
 new Uint8Array(synth.memory.buffer,synth.pal_rix_input(),rix.length).set(rix);
 if(!synth.pal_rix_load(rix.length))throw Error('Invalid RIX '+id);
 const pcm=[];let ticks=0,frames=0,nonzero=0;
 for(;ticks<42001;ticks++){
  const n=synth.pal_rix_tick();if(!n)break;
  const buffer=Buffer.from(new Uint8Array(synth.memory.buffer,synth.pal_rix_output(),n*2));
  for(let k=0;k<n;k++)if(buffer.readInt16LE(k*2))nonzero++;
  pcm.push(buffer);frames+=n;
 }
 if(!ticks||ticks>=42001||!nonzero)throw Error('Incomplete or silent RIX '+id);
 const output=path.join(musicDir,String(id).padStart(3,'0')+'.mp3');
 const result=spawnSync(ffmpeg,['-hide_banner','-loglevel','error','-y','-f','s16le','-ar','16384','-ac','1','-i','pipe:0','-codec:a','libmp3lame','-ar','22050','-b:a','48k','-threads','1',output],{input:Buffer.concat(pcm),windowsHide:true,maxBuffer:1024*1024});
 if(result.status!==0)throw Error('MP3 conversion failed: '+result.stderr);
 console.log(`RIX ${id}: ${(frames/16384).toFixed(1)} sec, ${fs.statSync(output).size} B`);
 songs++;nonzeroMusic+=nonzero;
}
function wav(pcm){
 const header=Buffer.alloc(44);header.write('RIFF');header.writeUInt32LE(36+pcm.length,4);header.write('WAVEfmt ',8);header.writeUInt32LE(16,16);header.writeUInt16LE(1,20);header.writeUInt16LE(2,22);header.writeUInt32LE(8192,24);header.writeUInt32LE(8192*4,28);header.writeUInt16LE(4,32);header.writeUInt16LE(16,34);header.write('data',36);header.writeUInt32LE(pcm.length,40);return Buffer.concat([header,pcm]);
}
const clipFrames=16300,parts=[],samples=[];let clips=0;
for(let id=0;id<effects.length;id++){
 const source=effects[id];samples.push(source.length);const count=Math.ceil(source.length/clipFrames);parts.push(count);
 for(let part=0;part<count;part++){
  const start=part*clipFrames,n=Math.min(clipFrames,source.length-start),pcm=Buffer.alloc(n*4);
  for(let i=0;i<n;i++){const v=source.readInt8(start+i)*256;pcm.writeInt16LE(v,i*4);pcm.writeInt16LE(v,i*4+2);}
  const file=wav(pcm);if(file.length>65536)throw Error('SFX exceeds SDK budget');
  fs.writeFileSync(path.join(sfxDir,String(id).padStart(3,'0')+'_'+part+'.wav'),file);clips++;
 }
}
const header=`#ifndef PAL_ASTER_AUDIO_CATALOG_H\n#define PAL_ASTER_AUDIO_CATALOG_H\n#define PAL_AUDIO_MUSIC_COUNT ${music.length}\n#define PAL_AUDIO_SFX_COUNT ${effects.length}\n#define PAL_AUDIO_CLIP_FRAMES ${clipFrames}u\nstatic const unsigned char pal_audio_sfx_parts[PAL_AUDIO_SFX_COUNT]={${parts.join(',')}};\nstatic const unsigned int pal_audio_sfx_samples[PAL_AUDIO_SFX_COUNT]={${samples.join(',')}};\n#endif\n`;
fs.writeFileSync(path.join(here,'include/audio_catalog.h'),header);
fs.writeFileSync(path.join(here,'build/audio/audio-manifest.json'),JSON.stringify({songs,clips,nonzeroMusic,musicRate:22050,sfxRate:8192,sfxChannels:2,sfxBits:16,clipFrames,sourcePackSize:pack.length},null,2));
console.log(`Audio ready: ${songs} songs, ${clips} bounded WAV clips`);
