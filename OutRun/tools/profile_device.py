from pathlib import Path
import sys,time,re
p=Path(__file__).resolve().parents[1];sys.path.insert(0,str(p.parent.parent/'aster-sdk/tools'))
import aster_send,deploy
label=sys.argv[1] if len(sys.argv)>1 else 'profile';seconds=int(sys.argv[2]) if len(sys.argv)>2 else 35
out=p/'artifacts/fps'/label;out.mkdir(parents=True,exist_ok=True)
with aster_send.open_serial('/dev/ttyACM0',timeout=.2,write_timeout=5) as con,(out/'device.log').open('a',encoding='utf8') as log:
 def record(s):log.write(s);log.flush()
 s=deploy.shell_command('/dev/ttyACM0','aster app stats aster.outrun',15,connection=con);print(s,flush=True);record(s)
 end=time.monotonic()+seconds;buf=''
 while time.monotonic()<end:
  b=con.read(4096)
  if b:
   s=b.decode(errors='replace');record(s);buf+=s
   while '\n' in buf:
    line,buf=buf.split('\n',1)
    if any(x in line for x in ['PERF ','IO ms=','V2 frame=','AUDIO','COLLISION']):print(re.sub(r'\x1b\[[0-9;]*[A-Za-z]','',line),flush=True)
 s=deploy.shell_command('/dev/ttyACM0','aster app stats aster.outrun',15,connection=con);print(s,flush=True);record(s)
