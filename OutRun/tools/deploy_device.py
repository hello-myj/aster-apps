#!/usr/bin/env python3
"""Deploy current OutRun AOT + seekable SD resources using the SDK serial protocol."""
from pathlib import Path
import sys,os,re,json,time,contextlib,argparse,hashlib
P=Path(__file__).resolve().parents[1]
SDK=P.parent.parent/'aster-sdk'
sys.path.insert(0,str(SDK/'tools'))
import aster_send,aster_board,deploy
parser=argparse.ArgumentParser();parser.add_argument('--port',default='/dev/ttyACM0');parser.add_argument('--resume',action='store_true');parser.add_argument('--skip-install',action='store_true');parser.add_argument('--install-only',action='store_true')
args=parser.parse_args();port=args.port
OUT=P/'artifacts/device';OUT.mkdir(parents=True,exist_ok=True)
statefile=OUT/'upload-status.json';state=json.loads(statefile.read_text()) if args.resume and statefile.exists() else {'files':{}}
log=open(OUT/'deploy.log','a',encoding='utf8',buffering=1)
class Tee:
 def write(self,s):sys.__stdout__.write(s);sys.__stdout__.flush();log.write(s)
 def flush(self):sys.__stdout__.flush();log.flush()
sys.stdout=Tee()
def status(**kw):
 state.update(kw);statefile.write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf8')
def shell(cmd,connection=None,timeout=30):
 for attempt in range(3):
  try:
   result=deploy.shell_command(port,cmd,timeout,connection=connection);print(result,flush=True);return result
  except RuntimeError:
   if attempt==2:raise
   print('RETRY SHELL',cmd,flush=True);time.sleep(.5)
status(stage='connecting')
print('DEVICE',port,time.strftime('%Y-%m-%d %H:%M:%S'),flush=True)
info=shell('aster sys info')
if 'board=aster.esp32s3.waveshare35b' not in info or 'abi=4.1.' not in info:raise SystemExit('Unexpected board/ABI; not deploying')
shell('aster sys mem');shell('aster fs df')
pkg=P/'dist/xtensa/aster.outrun.ast'
if not args.skip_install:deploy.main([str(pkg),'--target','xtensa','--port',port])
if args.install_only:raise SystemExit(0)
root='/SD:/aster/apps/aster.outrun/data/resources'
files=[(f,root+'/'+f.name) for f in sorted((P/'dist').glob('resources*.pak'))]
audio=P/'sdcard/aster/apps/aster.outrun/data/resources/audio'
files.append((audio/'music.mp3',root+'/audio/music.mp3'))
files += [(audio/name,root+'/audio/'+name) for name in ['engine-loop.wav','brake-loop.wav','crash-once.wav','start-once.wav']]
target=aster_board.get('xtensa')
class Progress:
 def __init__(self,name,total):self.name=name;self.total=total;self.buffer='';self.last=0;self.started=time.monotonic()
 def write(self,s):
  log.write(s);self.buffer+=s
  while '\n' in self.buffer:
   line,self.buffer=self.buffer.split('\n',1);m=re.fullmatch(r'\s*(\d+)/(\d+)\s*',line)
   if m:
    n=int(m[1])
    if n-self.last>=1048576:
     self.last=n;elapsed=time.monotonic()-self.started
     status(file=self.name,sent=n,total=self.total,seconds=round(elapsed,1))
     print(f'TRANSFER {self.name}: {n/1048576:.1f}/{self.total/1048576:.1f} MiB, {n/max(elapsed,1)/1024:.0f} KiB/s',flush=True)
 def flush(self):log.flush()
with aster_send.open_serial(port,timeout=.5,write_timeout=10) as con:
 for path in ['/SD:/aster','/SD:/aster/apps','/SD:/aster/apps/aster.outrun','/SD:/aster/apps/aster.outrun/data',root,root+'/audio']:shell('aster fs mkdir '+path,con)
 for local,remote in files:
  digest=hashlib.sha256(local.read_bytes()).hexdigest();n=local.stat().st_size
  if args.resume and state['files'].get(remote)==digest:
   print('REUSE PREVIOUS CRC-VERIFIED UPLOAD',local.name,flush=True);continue
  status(stage='upload',file=local.name,sent=0,total=n)
  print('UPLOAD',local.name,n,'bytes',flush=True)
  with contextlib.redirect_stderr(Progress(local.name,n)):
   answer=aster_send.send_file(port,local,remote,target=target,connection=con)
  if answer!='OK':raise RuntimeError('Upload failed '+remote+': '+str(answer))
  state['files'][remote]=digest;status(sent=n);print('UPLOADED CRC OK',local.name,flush=True)
 shell('aster fs stat '+root+'/resources.pak',con)
 status(stage='starting')
 result=shell('aster app start aster.outrun',con,45)
 if not re.search(r'(?m)^\s*started aster\.outrun\s*$',result):raise RuntimeError('App start failed; see deploy.log')
 until=time.monotonic()+12
 while time.monotonic()<until:
  b=con.read(4096)
  if b:print(b.decode(errors='replace'),end='',flush=True)
 shell('aster app state aster.outrun',con)
 shell('aster app stats aster.outrun',con)
 shell('aster sys mem',con)
 status(stage='running')
print('DEPLOYMENT FINISHED',flush=True)
