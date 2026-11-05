"""Compile the real touch adapter; exercise both hardware geometries."""
from pathlib import Path
import subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1];SDK=ROOT.parent.parent/'aster-sdk'
class Controls(unittest.TestCase):
 def test_ownership_geometry_and_release(self):
  code=r'''
#include <assert.h>
#include "engine.h"
#include "ui.h"
Game game;uint32_t screen_w,screen_h; bool outrun_motion_enabled(void){return false;} bool outrun_motion_available(void){return false;}
static int touch(unsigned slot,unsigned action,int x,int y){aster_event_t e={.type=ASTER_EVENT_TOUCH};e.u.touch.slot=slot;e.u.touch.action=action;e.u.touch.x=x;e.u.touch.y=y;return outrun_controls_touch(&e);}
int main(void){
 for(int size=0;size<2;size++){
  screen_w=size?480:320;screen_h=size?480:240;outrun_controls_clear();game=(Game){.state=2};
  const int ids[]={UI_WHEEL,UI_BRAKE,UI_GAS,UI_MENU,UI_AUTO,UI_PREV,UI_NEXT,UI_START,UI_RESUME,UI_RESTART,UI_MAPS};
  for(unsigned i=0;i<sizeof(ids)/sizeof(*ids);i++){UiRect r=ui_rect(ids[i]);assert(r.x>=0&&r.y>=0&&r.w>0&&r.h>0&&r.x+r.w<=(int)screen_w&&r.y+r.h<=(int)screen_h);}
  assert(ui_play_height()==(size?360:240));
  UiRect gas=ui_rect(UI_GAS),brake=ui_rect(UI_BRAKE);int wx=ui_wheel_x(),wy=ui_wheel_y();
  touch(0,ASTER_TOUCH_DOWN,wx,wy);touch(0,ASTER_TOUCH_MOVE,wx-35,wy);assert(game.touch_steer==-1);
  touch(1,ASTER_TOUCH_DOWN,gas.x+gas.w/2,gas.y+gas.h/2);assert(game.touch_gas&&game.touch_steer==-1);
  touch(2,ASTER_TOUCH_DOWN,brake.x+brake.w/2,brake.y+brake.h/2);assert(game.touch_brake&&game.touch_gas);
  touch(1,ASTER_TOUCH_UP,gas.x+5,gas.y+5);assert(!game.touch_gas&&game.touch_brake&&game.touch_steer==-1);
  touch(2,ASTER_TOUCH_UP,brake.x+5,brake.y+5);touch(3,ASTER_TOUCH_DOWN,wx+20,wy);assert(game.touch_steer==-1);
  touch(3,ASTER_TOUCH_MOVE,wx+100,wy);assert(game.touch_steer==-1);touch(3,ASTER_TOUCH_UP,wx+100,wy);
  touch(0,ASTER_TOUCH_MOVE,wx+150,wy);assert(game.touch_steer==1&&outrun_wheel_offset()>0);
  touch(0,ASTER_TOUCH_UP,wx+150,wy);assert(!game.touch_steer&&!outrun_wheel_offset());
  touch(0,ASTER_TOUCH_DOWN,10,3);touch(0,ASTER_TOUCH_MOVE,gas.x+10,gas.y+10);assert(!game.touch_gas);touch(0,ASTER_TOUCH_UP,gas.x+10,gas.y+10);
  touch(0,ASTER_TOUCH_DOWN,gas.x+10,gas.y+10);touch(0,ASTER_TOUCH_MOVE,brake.x+10,brake.y+10);assert(!game.touch_gas);assert(game.touch_brake==!size);
  outrun_controls_clear();touch(0,ASTER_TOUCH_MOVE,gas.x+10,gas.y+10);assert(!game.touch_gas);
  game.state=0;UiRect start=ui_rect(UI_START);assert(touch(0,ASTER_TOUCH_DOWN,start.x+10,start.y+10)==UI_NONE);assert(touch(0,ASTER_TOUCH_UP,start.x+10,start.y+10)==UI_START);
  game.menu_open=true;UiRect menu=ui_rect(UI_RESUME);touch(0,ASTER_TOUCH_DOWN,menu.x+10,menu.y+10);assert(touch(0,ASTER_TOUCH_UP,menu.x+10,menu.y+10)==UI_RESUME);
 }
 return 0;
}
'''
  with tempfile.TemporaryDirectory() as d:
   source=Path(d)/'controls.c';source.write_text(code);binary=Path(d)/'controls'
   includes=[ROOT/'src/port',SDK/'include',SDK/'lib/aster']
   subprocess.run(['cc','-std=c11','-fno-builtin','-Wno-attributes',*[f'-I{x}' for x in includes],str(source),str(ROOT/'src/port/controls.c'),'-o',str(binary)],check=True,capture_output=True,text=True)
   subprocess.run([str(binary)],check=True)
if __name__=='__main__':unittest.main()
