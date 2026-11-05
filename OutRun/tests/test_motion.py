"""Actual IMU adapter: tilt response, recenter, dead zone and sensor failure."""
from pathlib import Path
import subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1];SDK=ROOT.parent.parent/'aster-sdk'
class Motion(unittest.TestCase):
 def test_filter_and_fallback(self):
  code=r'''
#include <assert.h>
#include "engine.h"
#include "ui.h"
uint32_t screen_w=320,screen_h=240;
static float sample[3]={0,0,9.80665f};static int error,reads;
int32_t aster_accel_read(float *out,uint32_t n){assert(n==12);reads++;if(error)return error;memcpy(out,sample,n);return 0;}
void aster_logf(int level,const char *fmt,...){(void)level;(void)fmt;}
static void tick(uint32_t *now){*now+=33;outrun_motion_tick(*now);}
int main(void){uint32_t now=0;
 screen_w=screen_h=480;outrun_motion_init();assert(!outrun_motion_available()&&!reads);
 screen_w=320;screen_h=240;outrun_motion_init();assert(outrun_motion_enabled());
 sample[1]=.3f;for(int i=0;i<12;i++)tick(&now);assert(outrun_motion_steer()==0);
 sample[1]=-3;sample[2]=9.3f;for(int i=0;i<12;i++)tick(&now);float gentle=outrun_motion_steer();assert(gentle<-.7f&&gentle>-1.f);
 sample[1]=-5;sample[2]=8.5f;for(int i=0;i<12;i++)tick(&now);assert(outrun_motion_steer()<gentle);
 outrun_motion_center();for(int i=0;i<12;i++)tick(&now);assert(outrun_motion_steer()==0);
 sample[1]=3;sample[2]=9.3f;for(int i=0;i<12;i++)tick(&now);assert(outrun_motion_steer()>0);
 outrun_motion_toggle();assert(!outrun_motion_enabled()&&outrun_motion_steer()==0);
 outrun_motion_toggle();assert(outrun_motion_enabled());
 error=-1;tick(&now);assert(outrun_motion_steer()==0);tick(&now);tick(&now);assert(!outrun_motion_enabled());
 error=0;outrun_motion_toggle();assert(outrun_motion_enabled());
 sample[0]=100;tick(&now);assert(outrun_motion_steer()==0);
 return 0;}
'''
  with tempfile.TemporaryDirectory() as d:
   src=Path(d)/'motion.c';src.write_text(code);binary=Path(d)/'motion'
   subprocess.run(['cc','-std=c11','-fno-builtin','-Wno-attributes',*[f'-I{x}' for x in [ROOT/'src/port',SDK/'include',SDK/'lib/aster']],str(src),str(ROOT/'src/port/motion.c'),'-o',str(binary)],check=True)
   subprocess.run([str(binary)],check=True)
if __name__=='__main__':unittest.main()
