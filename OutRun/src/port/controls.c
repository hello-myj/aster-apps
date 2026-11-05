/* GPL-3.0-or-later. Stable touch ownership: wheel and pedals are independent. */
#include "engine.h"
#include "ui.h"
typedef struct {uint32_t slot;int role,x,y;bool active;} Contact;
static Contact contacts[10];static int wheel_offset;
void outrun_controls_clear(void){memset(contacts,0,sizeof(contacts));wheel_offset=0;game.touch_steer=0;game.touch_gas=game.touch_brake=false;}
int outrun_wheel_offset(void){return wheel_offset;}
bool outrun_control_held(int id){for(unsigned i=0;i<10;i++)if(contacts[i].active&&contacts[i].role==id&&ui_hit(id,contacts[i].x,contacts[i].y))return true;return false;}
static int role_at(int x,int y){
 if(game.menu_open){const int ids[]={UI_RESUME,UI_RESTART,UI_MAPS};for(unsigned i=0;i<3;i++)if(ui_hit(ids[i],x,y))return ids[i];return UI_NONE;}
 if(game.state==0){const int ids[]={UI_PREV,UI_NEXT,UI_START};for(unsigned i=0;i<3;i++)if(ui_hit(ids[i],x,y))return ids[i];return UI_NONE;}
 if(ui_hit(UI_MENU,x,y))return UI_MENU;
 if(game.state==3||game.state==4)return ui_hit(UI_START,x,y)?UI_START:UI_NONE;
 if(game.state!=1&&game.state!=2)return UI_NONE;
 if(!ui_large()&&outrun_motion_available()&&ui_hit(UI_MODE,x,y))return UI_MODE;
 if(outrun_motion_enabled()&&ui_hit(UI_CENTER,x,y))return UI_CENTER;
 const int ids[]={UI_AUTO,UI_WHEEL,UI_BRAKE,UI_GAS};for(unsigned i=0;i<4;i++)if((ids[i]!=UI_WHEEL||!outrun_motion_enabled())&&ui_hit(ids[i],x,y))return ids[i];return UI_NONE;
}
int outrun_controls_touch(const aster_event_t *e){
 unsigned action=e->u.touch.action;int at=-1,result=UI_NONE;
 for(unsigned i=0;i<10;i++)if(contacts[i].active&&contacts[i].slot==e->u.touch.slot)at=(int)i;
 if(action==ASTER_TOUCH_DOWN){
  if(at<0)for(unsigned i=0;i<10;i++)if(!contacts[i].active){at=(int)i;break;}
  if(at>=0){int role=role_at(e->u.touch.x,e->u.touch.y);if(role==UI_WHEEL)for(unsigned i=0;i<10;i++)if((int)i!=at&&contacts[i].active&&contacts[i].role==UI_WHEEL)role=UI_NONE;
   contacts[at]=(Contact){.slot=e->u.touch.slot,.role=role,.x=e->u.touch.x,.y=e->u.touch.y,.active=true};}
 }
 if(at>=0){Contact *c=&contacts[at];c->x=e->u.touch.x;c->y=e->u.touch.y;
  /* Only a driving contact may cross between the invisible half-screen
   * zones. Dragging from a menu/control never becomes gas or brake. */
  if(!ui_large()&&(c->role==UI_GAS||c->role==UI_BRAKE)&&action==ASTER_TOUCH_MOVE){
   if(ui_hit(UI_BRAKE,c->x,c->y))c->role=UI_BRAKE;
   else if(ui_hit(UI_GAS,c->x,c->y))c->role=UI_GAS;
  }
  if(action==ASTER_TOUCH_UP){if(c->role!=UI_WHEEL&&c->role!=UI_BRAKE&&c->role!=UI_GAS&&ui_hit(c->role,c->x,c->y))result=c->role;c->active=false;}
 }
 game.touch_steer=0;game.touch_gas=game.touch_brake=false;wheel_offset=0;
 for(unsigned i=0;i<10;i++)if(contacts[i].active){Contact *c=&contacts[i];
  if(c->role==UI_WHEEL){int range=ui_wheel_radius()*3/4;wheel_offset=CLAMP(c->x-ui_wheel_x(),-range,range);if(wheel_offset>8)game.touch_steer=1;else if(wheel_offset<-8)game.touch_steer=-1;}
  else if(c->role==UI_BRAKE&&ui_hit(UI_BRAKE,c->x,c->y))game.touch_brake=true;
  else if(c->role==UI_GAS&&ui_hit(UI_GAS,c->x,c->y))game.touch_gas=true;
 }
 return result;
}
