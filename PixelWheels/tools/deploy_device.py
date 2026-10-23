#!/usr/bin/env python3
"""Install Pixel Wheels AOT + CRC-verified device resources via public SDK tools."""
from pathlib import Path
import argparse,sys,json,time,hashlib,contextlib,re
p=Path(__file__).resolve().parents[1];sdk=p.parent.parent/'aster-sdk';sys.path.insert(0,str(sdk/'tools'))
import deploy,aster_send,aster_board
parser=argparse.ArgumentParser();parser.add_argument('--port',required=True);parser.add_argument('--resume',action='store_true');parser.add_argument('--observe',type=int,default=15);args=parser.parse_args()
port=args.port;artifact=p/'artifacts/device';artifact.mkdir(parents=True,exist_ok=True)
log=open(artifact/'deploy.log','a',encoding='utf8',buffering=1)
class Tee:
    def write(self,s):sys.__stdout__.write(s);sys.__stdout__.flush();log.write(s)
    def flush(self):sys.__stdout__.flush();log.flush()
sys.stdout=Tee()
statefile=artifact/'upload-status.json'
state=json.loads(statefile.read_text()) if args.resume and statefile.exists() else {'files':{}}
def status(**values):
    state.update(values);statefile.write_text(json.dumps(state,indent=2),encoding='utf8')
def shell(cmd,con=None,timeout=30):
    for attempt in range(3):
        try:
            result=deploy.shell_command(port,cmd,timeout,connection=con);print(result,flush=True);return result
        except RuntimeError:
            if attempt==2:raise
            print('Retry shell',cmd,flush=True)
class Progress:
    def __init__(self,name):self.name=name;self.buffer='';self.last=0;self.started=time.monotonic()
    def write(self,s):
        log.write(s);self.buffer+=s
        while '\n' in self.buffer:
            line,self.buffer=self.buffer.split('\n',1);m=re.fullmatch(r'\s*(\d+)/(\d+)\s*',line)
            if m:
                n,total=map(int,m.groups())
                if n-self.last>=524288:
                    self.last=n;status(file=self.name,sent=n,total=total)
                    print('TRANSFER',self.name,n,'/',total,flush=True)
    def flush(self):log.flush()
info=shell('aster sys info')
if 'board=aster.esp32s3.waveshare35b' not in info or 'abi=4.1.' not in info:raise SystemExit('Unexpected board or ABI')
shell('aster fs df');shell('aster sys mem');listing=shell('aster app list')
active=[line.split()[0] for line in listing.splitlines() if re.search(r'\b(RUNNING|PAUSED)\b',line)]
for app in active:
    if app!='aster.pixelwheels':raise SystemExit('Another APP is active: '+app)
    shell('aster app stop '+app)
resources=p/'dist/device-resources';metadata=resources/'device-resources.json'
if not metadata.exists():raise SystemExit('Run tools/prepare_device_assets.py first')
manifest=json.loads(metadata.read_text())
for entry in manifest:
    file=resources/entry['file']
    if file.stat().st_size!=entry['bytes'] or hashlib.sha256(file.read_bytes()).hexdigest()!=entry['sha256']:raise SystemExit('Stale device resource metadata: '+file.name)
root='/SD:/aster/apps/aster.pixelwheels/data/resources';target=aster_board.get('xtensa')
files=[(resources/entry['file'],root+'/'+entry['file']) for entry in manifest]
files.append((p/'dist/xtensa/aster.pixelwheels.ast','/SD:/aster.pixelwheels.ast'))
with aster_send.open_serial(port,timeout=.5,write_timeout=10) as con:
    for directory in ['/SD:/aster','/SD:/aster/apps','/SD:/aster/apps/aster.pixelwheels','/SD:/aster/apps/aster.pixelwheels/data',root]:shell('aster fs mkdir '+directory,con)
    for local,remote in files:
        digest=hashlib.sha256(local.read_bytes()).hexdigest()
        if args.resume and state['files'].get(remote)==digest:
            print('REUSE previously CRC-verified upload',local.name,flush=True);continue
        status(stage='upload',file=local.name,sent=0,total=local.stat().st_size)
        print('UPLOAD',local.name,local.stat().st_size,flush=True)
        with contextlib.redirect_stderr(Progress(local.name)):result=aster_send.send_file(port,local,remote,target=target,connection=con)
        if result!='OK':raise RuntimeError('Upload failed: '+local.name+' '+str(result))
        state['files'][remote]=digest;status(sent=local.stat().st_size);print('CRC OK',local.name,flush=True)
    response=shell('aster app install /SD:/aster.pixelwheels.ast',con)
    if 'installed OK' not in response:raise RuntimeError('Install failed')
    response=shell('aster app start aster.pixelwheels',con,45)
    if not re.search(r'(?m)^\s*started aster\.pixelwheels\s*$',response):raise RuntimeError('Start failed')
    status(stage='starting')
    until=time.monotonic()+args.observe
    while time.monotonic()<until:
        data=con.read(8192)
        if data:print(data.decode(errors='replace'),end='',flush=True)
    result=shell('aster app state aster.pixelwheels',con)
    shell('aster app stats aster.pixelwheels',con);shell('aster sys mem',con)
    if 'RUNNING' not in result:raise RuntimeError('APP is not running')
status(stage='running');print('DEVICE DEPLOY COMPLETE',flush=True)
