#!/usr/bin/env python3
"""Capture the real display and read current APP state through public SDK tools."""
from pathlib import Path
import sys,argparse
p=Path(__file__).resolve().parents[1];sys.path.insert(0,str(p.parent.parent/'aster-sdk/tools'))
import deploy,screenshot
parser=argparse.ArgumentParser();parser.add_argument('--port',required=True);parser.add_argument('--label',default='device-screen');args=parser.parse_args()
out=p/'artifacts/device';out.mkdir(parents=True,exist_ok=True)
with (out/'runtime.log').open('a',encoding='utf8') as log:
    for cmd in ['aster app state aster.pixelwheels','aster app stats aster.pixelwheels','aster sys mem']:
        s=deploy.shell_command(args.port,cmd,20);print(s,flush=True);log.write(s+'\n')
    path=out/(args.label+'.png');result=screenshot.save_png(args.port,path)
    print('CAPTURE',path,result,flush=True);log.write('CAPTURE '+str(path)+' '+str(result)+'\n')