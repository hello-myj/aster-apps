"""Exercise real game touch rules, including purchases, bombs and pause routes."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

PROJECT=Path(__file__).resolve().parents[1]

class TouchRules(unittest.TestCase):
    @unittest.skipUnless(shutil.which('cc'), 'native C compiler required')
    def test_direct_touch_actions(self):
        with tempfile.TemporaryDirectory() as name:
            root=Path(name)
            (root/'aster_imports.h').write_text('#include <stdint.h>\nint aster_printf(const char*,...);\nuint32_t aster_time_now_ms(void);\n')
            (root/'test.c').write_text(r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
'''+'#include "'+(PROJECT/'port/game.c').as_posix()+'"\n'+r'''
int aster_printf(const char *s,...){(void)s;return 0;}
uint32_t aster_time_now_ms(void){return 0;}
int gm_width(const char *s){(void)s;return 32;}int gm_height(const char *s){(void)s;return 32;}
void gm_blit(const char *s,int f,float x,float y,float a,float sx,float sy,float ox,float oy){(void)s;(void)f;(void)x;(void)y;(void)a;(void)sx;(void)sy;(void)ox;(void)oy;}
void gm_text(int f,const char *s,float x,float y,uint16_t c){(void)f;(void)s;(void)x;(void)y;(void)c;}
void gm_rect(int x,int y,int w,int h,uint16_t c,unsigned a){(void)x;(void)y;(void)w;(void)h;(void)c;(void)a;}
void gm_line(int x,int y,int x2,int y2,uint16_t c,unsigned a){(void)x;(void)y;(void)x2;(void)y2;(void)c;(void)a;}
void gm_audio_play(int id){(void)id;}void gm_audio_stop(int id){(void)id;}
int gm_audio_playing(int id){(void)id;return 0;}void gm_audio_pause(int p){(void)p;}
void gm_save(int s,int l){(void)s;(void)l;}void gm_load(int *s,int *l){*s=0;*l=1;}
int main(void){
 gm_init();gm_touch(4,80);assert(gm_state==S_MENU);gm_touch(80,180);assert(gm_state==S_HIGH);
 gm_touch(80,200);assert(gm_state==S_MENU);gm_touch(80,155);assert(gm_state==S_GOAL);
 gm_touch(160,120);assert(gm_state==S_GAME);gm_touch(286,14);assert(gm_paused);
 gm_touch(2,90);assert(gm_paused);gm_touch(100,90);assert(!gm_paused);
 gm_touch(160,150);assert(h.firing);
 p.bombs=1;h.grabbed=0;h.stopped=0;entities[0].active=1;
 gm_touch(120,20);assert(p.bombs==0&&h.grabbed==-1&&!entities[0].active);
 p.money=p.goal;gm_touch(60,18);assert(gm_state==S_MADE_GOAL);
 gm_touch(160,120);assert(gm_state==S_SHOP);
 itemCount=1;items[0]=0;prices[0]=1;p.money=100;p.bombs=0;finishShopping=0;
 gm_touch(30,150);assert(p.bombs==1&&p.money==99&&itemCount==0);
 gm_touch(100,250);assert(!finishShopping);gm_touch(250,140);assert(finishShopping);
 gm_touch(286,14);assert(gm_paused);gm_touch(100,123);assert(gm_state==S_MENU&&!gm_paused);
 gm_touch(286,14);gm_touch(100,160);assert(gm_exit);return 0;
}
''')
            subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Wno-misleading-indentation','-I',str(root),str(root/'test.c'),'-lm','-o',str(root/'test')],check=True,capture_output=True,text=True)
            subprocess.run([str(root/'test')],check=True,capture_output=True,text=True)

if __name__=='__main__':unittest.main()
