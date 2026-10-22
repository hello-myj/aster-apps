#!/usr/bin/env python3
"""Read real-device statistics and capture its displayed frame."""
from pathlib import Path
import sys,time,json
P=Path(__file__).resolve().parents[1]
SDK=P.parent.parent/'aster-sdk'
sys.path.insert(0,str(SDK/'tools'))
import aster_send,deploy,screenshot
port=sys.argv[1] if len(sys.argv)>1 else '/dev/ttyACM0'
out=P/'artifacts/device';out.mkdir(parents=True,exist_ok=True)
with (out/'runtime-verification.log').open('a',encoding='utf8',buffering=1) as log:
 def show(s):print(s,flush=True);log.write(s+'\n')
 with aster_send.open_serial(port,timeout=.2,write_timeout=5) as con:
  for cmd in ['aster app state aster.outrun','aster app stats aster.outrun','aster sys mem','aster ui']:
   show(deploy.shell_command(port,cmd,timeout=20,connection=con))
  end=time.monotonic()+15
  while time.monotonic()<end:
   b=con.read(4096)
   if b:show(b.decode(errors='replace'))
  show(deploy.shell_command(port,'aster app stats aster.outrun',timeout=20,connection=con))
 try:
  size=screenshot.save_png(port,out/'first-screen.png');show('CAPTURE '+str(size))
 except Exception as e:show('CAPTURE FAILED '+repr(e))
