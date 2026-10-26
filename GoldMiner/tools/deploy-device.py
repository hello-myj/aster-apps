"""Install and observe the actual GoldMiner APP through the SDK serial tools."""
from pathlib import Path
import argparse,sys,time,contextlib
P=Path(__file__).resolve().parents[1]
SDK=P.parents[1]/'aster-sdk'
sys.path.insert(0,str(SDK/'tools'))
import aster_send,aster_board,deploy
parser=argparse.ArgumentParser();parser.add_argument('--port',default='/dev/ttyACM0');parser.add_argument('--observe',type=int,default=180);a=parser.parse_args()
out=P/'artifacts/device';out.mkdir(parents=True,exist_ok=True)
log=(out/'deploy.log').open('a',encoding='utf-8',buffering=1)
class Tee:
 def write(self,s):sys.__stdout__.write(s);sys.__stdout__.flush();log.write(s)
 def flush(self):sys.__stdout__.flush();log.flush()
sys.stdout=Tee()
def shell(cmd,con=None,timeout=30):
 result=deploy.shell_command(a.port,cmd,timeout,connection=con);print(result,flush=True);return result
print('GoldMiner device deployment',time.strftime('%Y-%m-%d %H:%M:%S'))
info=shell('aster sys info')
if 'waveshare35b' not in info or not any('abi='+v in info for v in ('4.1.','4.2.')):raise SystemExit('Unexpected board/ABI')
shell('aster sys mem');shell('aster fs df')
listing=shell('aster app list')
for line in listing.splitlines():
 if ' RUNNING' in line or ' PAUSED' in line:
  app=line.split()[0]
  if app!='aster.goldminer':raise SystemExit('Stop the active APP first: '+app)
  shell('aster app stop '+app)
package=P/'dist/xtensa/aster.goldminer.ast'
with aster_send.open_serial(a.port,timeout=.5,write_timeout=10) as con:
 print('Uploading',package.stat().st_size,'bytes',flush=True)
 answer=aster_send.send_file(a.port,package,'/SD:/aster.goldminer.ast',target=aster_board.get('xtensa'),connection=con)
 if answer!='OK':raise RuntimeError('Upload failed: '+str(answer))
 print('Package CRC OK',flush=True)
with aster_send.open_serial(a.port,timeout=.2,write_timeout=10) as con:
 time.sleep(1)
 answer=shell('aster app install /SD:/aster.goldminer.ast',con,180)
 if 'installed OK' not in answer:raise RuntimeError('Install failed')
 answer=shell('aster app start aster.goldminer',con,60)
 if 'started aster.goldminer' not in answer:raise RuntimeError('Start failed')
 print('GOLDMINER STARTED - observing',a.observe,'seconds',flush=True)
 until=time.monotonic()+a.observe
 while time.monotonic()<until:
  data=con.read(8192)
  if data:print(data.decode(errors='replace'),end='',flush=True)
 state=shell('aster app state aster.goldminer',con)
 shell('aster app stats aster.goldminer',con);shell('aster sys mem',con)
 if 'RUNNING' not in state:raise RuntimeError('APP no longer running')
print('GOLDMINER DEVICE RUNNING',flush=True)
