"""Observe a running APP without restarting the player's session."""
from pathlib import Path
import sys,time,argparse
P=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(P.parents[1]/'aster-sdk/tools'))
import aster_send,deploy
parser=argparse.ArgumentParser();parser.add_argument('--port',default='/dev/ttyACM0');parser.add_argument('--seconds',type=int,default=60);a=parser.parse_args()
log=P/'artifacts/device/gameplay.log';log.parent.mkdir(parents=True,exist_ok=True)
with log.open('w',encoding='utf-8',buffering=1) as out,aster_send.open_serial(a.port,timeout=.2,write_timeout=5) as con:
 until=time.monotonic()+a.seconds
 while time.monotonic()<until:
  data=con.read(8192)
  if data:
   s=data.decode(errors='replace');out.write(s)
 for cmd in ['aster app state aster.goldminer','aster app stats aster.goldminer','aster sys mem']:
  s=deploy.shell_command(a.port,cmd,30,connection=con);out.write(s);print(s,flush=True)
print('Saved',log,flush=True)
