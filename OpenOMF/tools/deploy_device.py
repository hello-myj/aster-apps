#!/usr/bin/env python3
"""Upload original OpenOMF resources and its AOT package through the SDK."""
from pathlib import Path
import argparse,contextlib,hashlib,json,os,re,sys,time
P=Path(__file__).resolve().parents[1]
SDK=Path(os.environ.get("ASTER_SDK",str(P.parent.parent/"aster-sdk")))
sys.path.insert(0,str(SDK/"tools"))
import aster_send,aster_board,deploy
parser=argparse.ArgumentParser();parser.add_argument("--port",default="/dev/ttyACM0");parser.add_argument("--resume",action="store_true");parser.add_argument("--demo",action="store_true");parser.add_argument("--demo-stress",action="store_true");parser.add_argument("--observe",type=int,default=20);a=parser.parse_args()
OUT=P/"artifacts/device";OUT.mkdir(parents=True,exist_ok=True)
statefile=OUT/"upload-status.json";state=json.loads(statefile.read_text()) if a.resume and statefile.exists() else {"files":{}}
log=(OUT/"deploy.log").open("a",encoding="utf-8",buffering=1)
class Tee:
 def write(self,s):sys.__stdout__.write(s);sys.__stdout__.flush();log.write(s)
 def flush(self):log.flush();sys.__stdout__.flush()
sys.stdout=Tee()
def status(**values):state.update(values);statefile.write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding="utf-8")
def shell(cmd,con=None,timeout=25):
 result=deploy.shell_command(a.port,cmd,timeout,connection=con);print(result,flush=True);return result
class Progress:
 def __init__(self,name):self.name=name;self.text="";self.last=0
 def write(self,s):
  log.write(s);self.text+=s
  while "\n" in self.text:
   line,self.text=self.text.split("\n",1);m=re.fullmatch(r"\s*(\d+)/(\d+)\s*",line)
   if m:
    n,total=map(int,m.groups())
    if n-self.last>=524288:self.last=n;status(stage="upload",file=self.name,sent=n,total=total);print("TRANSFER",self.name,n,"/",total,flush=True)
 def flush(self):log.flush()
print("OpenOMF device deploy",time.strftime("%Y-%m-%d %H:%M:%S"),flush=True)
info=shell("aster sys info")
if "board=aster.esp32s3.waveshare35b" not in info or "abi=4.2." not in info:raise SystemExit("Unexpected board or ABI")
shell("aster sys mem");shell("aster fs df")
listing=shell("aster app list")
for line in listing.splitlines():
 if re.search(r"\b(RUNNING|PAUSED)\b",line):
  app=line.split()[0]
  if app!="aster.openomf":raise SystemExit("Another APP is active: "+app)
  shell("aster app stop "+app)
source=P/"sdcard/aster/apps/aster.openomf/data/resources"
reference=P/"artifacts/reference-0.8.6/openomf/resources"
if not reference.exists():raise SystemExit("Prepare original resources first")
# Only original files plus full converted music; never upload old preview .bin assets.
names={f.name for f in reference.iterdir() if f.is_file()}
names.update(stem+".mp3" for stem in ["MENU","END"]+["ARENA"+str(i) for i in range(5)])
root="/SD:/aster/apps/aster.openomf/data/resources"
files=[(source/name,root+"/"+name) for name in sorted(names)]
if a.demo or a.demo_stress:
 marker=OUT/"device-demo.once";marker.write_text("2" if a.demo_stress else "1")
 files.append((marker,"/SD:/aster/apps/aster.openomf/data/state/device-demo.once"))
files.append((P/"dist/xtensa/aster.openomf.ast","/SD:/aster.openomf.ast"))
for local,remote in files:
 if not local.is_file():raise SystemExit("Missing resource: "+str(local))
print("UPLOAD TOTAL",sum(f.stat().st_size for f,_ in files),"bytes",len(files),"files",flush=True)
target=aster_board.get("xtensa")
with aster_send.open_serial(a.port,timeout=.5,write_timeout=10) as con:
 for directory in ["/SD:/aster","/SD:/aster/apps","/SD:/aster/apps/aster.openomf","/SD:/aster/apps/aster.openomf/data","/SD:/aster/apps/aster.openomf/data/state",root]:shell("aster fs mkdir "+directory,con)
 for local,remote in files:
  digest=hashlib.sha256(local.read_bytes()).hexdigest()
  if a.resume and not remote.endswith("device-demo.once") and state["files"].get(remote)==digest:print("REUSE CRC-VERIFIED",local.name,flush=True);continue
  print("UPLOAD",local.name,local.stat().st_size,flush=True);status(stage="upload",file=local.name,sent=0,total=local.stat().st_size)
  with contextlib.redirect_stderr(Progress(local.name)):answer=aster_send.send_file(a.port,local,remote,target=target,connection=con)
  if answer!="OK":raise RuntimeError("Upload failed: "+local.name+" "+str(answer))
  state["files"][remote]=digest;status(sent=local.stat().st_size);print("CRC OK",local.name,flush=True)
# Reopen the shared console after the last ASTERPUT transaction.
with aster_send.open_serial(a.port,timeout=.2,write_timeout=10) as con:
 time.sleep(1)
 response=shell("aster app install /SD:/aster.openomf.ast",con,180)
 if "installed OK" not in response:raise RuntimeError("Install failed")
 status(stage="starting")
 response=shell("aster app start aster.openomf",con,60)
 if not re.search(r"(?m)^\s*started aster\.openomf\s*$",response):
  shell("aster sys mem",con);raise RuntimeError("APP did not start; inspect loader log")
 until=time.monotonic()+a.observe
 while time.monotonic()<until:
  data=con.read(8192)
  if data:print(data.decode(errors="replace"),end="",flush=True)
 response=shell("aster app state aster.openomf",con)
 shell("aster app stats aster.openomf",con);shell("aster sys mem",con)
 if "RUNNING" not in response:raise RuntimeError("APP stopped after launch")
status(stage="running");print("OPENOMF DEVICE RUNNING",flush=True)
