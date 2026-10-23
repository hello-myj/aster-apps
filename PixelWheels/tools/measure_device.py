from pathlib import Path
import sys,time,argparse
p=Path(__file__).resolve().parents[1];sys.path.insert(0,str(p.parent.parent/'aster-sdk/tools'))
import aster_send
parser=argparse.ArgumentParser();parser.add_argument('--seconds',type=int,default=45);parser.add_argument('--label',default='perf');args=parser.parse_args()
path=p/'artifacts/device'/(args.label+'.log');path.parent.mkdir(parents=True,exist_ok=True)
with path.open('ab') as out,aster_send.open_serial('/dev/ttyACM0',timeout=.5,write_timeout=10) as con:
    end=time.monotonic()+args.seconds
    while time.monotonic()<end:
        data=con.read(8192)
        if data:
            out.write(data);out.flush();print(data.decode(errors='replace'),end='',flush=True)
