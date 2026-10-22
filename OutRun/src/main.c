/* GPL-3.0-or-later. Aster lifecycle adapter; game and rendering live in port/. */
#include "port/engine.h"
#include "port/ui.h"
FrameProfile profile;static uint32_t profile_start,profile_frames;
static aster_handle_t surface;static uint32_t last_time;static float accumulator;
static bool left_key,right_key,gas_key,brake_key;
static bool touch_blocked,ui_dirty=true,lifecycle_paused;
int aster_app_init(void){
 aster_surface_desc_t d={.size=sizeof(d),.version=1,.format=ASTER_PIXEL_RGB565};
 int r=aster_surface_create(&d,sizeof(d),&surface);if(r<0)return r;
 aster_surface_buffer_t b={.size=sizeof(b),.version=1};r=aster_surface_acquire(surface,&b,sizeof(b));if(r<0)return r;
 if(!b.ptr)return ASTER_ENOTSUP;pixels=b.ptr;screen_w=b.width;screen_h=b.height;stride=b.pitch/2;
 r=load_font();if(r<0){aster_logf(ASTER_LOG_ERROR,"Required resources missing. Copy sdcard/aster/apps/aster.outrun/data/resources to SD root.");return r;}
 game_reset();aster_app_set_framerate(60);
 aster_logf(ASTER_LOG_INFO,"OUTRUN V2 %ux%u original projection/height/object placement; state=%d",screen_w,screen_h,game.state);draw_frame();memset(&profile,0,sizeof(profile));return aster_surface_present(surface);
}
static void refresh(void){int before=game.steer;bool oldbrake=game.brake;game.steer=(left_key?-1:0)+(right_key?1:0)+game.touch_steer;game.steer=CLAMP(game.steer,-1,1);game.brake=brake_key||game.touch_brake;game.gas=gas_key||game.touch_gas;if(before!=game.steer||oldbrake!=game.brake)aster_logf(ASTER_LOG_INFO,"INPUT steer=%d brake=%u speed=%u",game.steer,game.brake,(uint32_t)game.speed);}
static void reset_clock(void){last_time=0;accumulator=0;profile_start=profile_frames=0;memset(&profile,0,sizeof(profile));}
static void release_controls(void){left_key=right_key=gas_key=brake_key=false;clear_input();ui_dirty=true;}
static void set_menu(bool open){
 game.menu_open=open;game.paused=open||lifecycle_paused;release_controls();reset_clock();
 if(open)aster_audio_pause();else if(!lifecycle_paused)aster_audio_resume();
}
static void start_or_retry(void){
 if(game.state==3||game.state==4)game_reset_map(game.map_id?game.map_id:1);
 if(game.state==0){release_controls();reset_clock();game.state=1;game.countdown=3;}
}
static void menu_action(int choice){
 uint32_t id=game.map_id?game.map_id:1;
 if(choice==0)set_menu(false);
 else {game_reset_map(id);release_controls();reset_clock();if(choice==1)start_or_retry();}
}
void aster_app_event(const aster_event_t *e){
 if(e->type==ASTER_EVENT_TOUCH){
  int x=e->u.touch.x,y=e->u.touch.y,action=e->u.touch.action;
  if(touch_blocked){if(action==ASTER_TOUCH_UP)touch_blocked=false;return;}
  if(game.menu_open){
   if(action==ASTER_TOUCH_DOWN){touch_blocked=true;
    if(ui_hit(UI_RESUME,x,y))menu_action(0);
    else if(ui_hit(UI_RESTART,x,y))menu_action(1);
    else if(ui_hit(UI_MAPS,x,y))menu_action(2);
   }return;
  }
  if(game.state==0){
   if(action==ASTER_TOUCH_DOWN){touch_blocked=true;
    if(ui_hit(UI_PREV,x,y))game_select(-1);
    else if(ui_hit(UI_NEXT,x,y))game_select(1);
    else if(ui_hit(UI_START,x,y))start_or_retry();
    ui_dirty=true;
   }return;
  }
  if(action==ASTER_TOUCH_DOWN&&ui_hit(UI_MENU,x,y)){set_menu(true);touch_blocked=true;return;}
  if(game.state==3||game.state==4){if(action==ASTER_TOUCH_DOWN&&ui_hit(UI_START,x,y)){start_or_retry();touch_blocked=true;}return;}
  if(action==ASTER_TOUCH_DOWN&&ui_hit(UI_AUTO,x,y)){game.auto_gas=!game.auto_gas;release_controls();touch_blocked=true;return;}
  game.touch_steer=0;game.touch_brake=game.touch_gas=false;
  if(action!=ASTER_TOUCH_UP){
   if(ui_hit(UI_LEFT,x,y))game.touch_steer=-1;
   else if(ui_hit(UI_RIGHT,x,y))game.touch_steer=1;
   else if(ui_hit(UI_BRAKE,x,y))game.touch_brake=true;
   else if(!game.auto_gas&&ui_hit(UI_GAS,x,y))game.touch_gas=true;
  }refresh();return;
 }
 if(e->type==ASTER_EVENT_KEY){
  uint32_t c=e->u.key.code;bool pressed=e->u.key.pressed;
  if(game.menu_open){
   if(pressed){if(c==1)set_menu(false);else if(c==103)game.menu_choice=(game.menu_choice+2)%3;
    else if(c==108)game.menu_choice=(game.menu_choice+1)%3;else if(c==28)menu_action(game.menu_choice);ui_dirty=true;}
   return;
  }
  if(game.state==0){if(pressed){if(c==105||c==106)game_select(c==105?-1:1);else if(c==28)start_or_retry();}return;}
  if(pressed&&c==1){set_menu(true);return;}
  if(pressed&&c==28&&(game.state==3||game.state==4)){start_or_retry();return;}
  if(c==105)left_key=pressed;if(c==106)right_key=pressed;if(c==103||c==29)gas_key=pressed;if(c==108||c==56)brake_key=pressed;
  refresh();return;
 }
 if(e->type!=ASTER_EVENT_FRAME)return;
 if(game.menu_open&&!ui_dirty){last_time=(uint32_t)e->timestamp_ms;return;}game.frames++;
 uint32_t now=(uint32_t)e->timestamp_ms;
 if(!last_time)last_time=now;uint32_t elapsed=now-last_time;last_time=now;
 uint32_t measured=aster_time_now_ms();if(!profile_start)profile_start=measured;
 accumulator+=MIN(elapsed,150);int steps=0;while(accumulator>=1000.f/60.f&&steps<9){game_update(1.f/60.f);accumulator-=1000.f/60.f;steps++;}
 audio_tick();
 profile.update+=(uint32_t)(aster_time_now_ms()-measured);measured=aster_time_now_ms();
 aster_surface_buffer_t b={.size=sizeof(b),.version=1};
 if(aster_surface_acquire(surface,&b,sizeof(b))<0){game.errors++;return;}pixels=b.ptr;stride=b.pitch/2;profile.acquire+=(uint32_t)(aster_time_now_ms()-measured);draw_frame();measured=aster_time_now_ms();
 int r=aster_surface_present(surface);if(r<0)game.errors++;ui_dirty=false;profile.present+=(uint32_t)(aster_time_now_ms()-measured);
 if(++profile_frames==60){uint32_t duration=(uint32_t)(aster_time_now_ms()-profile_start);
 aster_logf(ASTER_LOG_INFO,"PERF n=60 span=%u update=%u acquire=%u road=%u bg=%u sprite=%u hud=%u present=%u",duration,profile.update,profile.acquire,profile.road,profile.bg,profile.sprites,profile.hud,profile.present);
 aster_logf(ASTER_LOG_INFO,"IO ms=%u bytes=%u hits=%u misses=%u",profile.io,profile.bytes,profile.hits,profile.misses);memset(&profile,0,sizeof(profile));profile_start=aster_time_now_ms();profile_frames=0;}
 if(game.frames%150==0)aster_logf(ASTER_LOG_INFO,"V2 frame=%u map=%u line=%u speed=%u x=%d state=%d cache=%u/%u errors=%u assets=%u",game.frames,game.map_id,(uint32_t)(game.position/SEGMENT),(uint32_t)game.speed,(int)(game.x*100),game.state,cache_bytes,cache_peak,game.errors,asset_errors);
}
void aster_app_pause(void){lifecycle_paused=true;game.paused=true;release_controls();touch_blocked=false;aster_audio_pause();}
void aster_app_resume(void){lifecycle_paused=false;game.paused=game.menu_open;release_controls();touch_blocked=false;reset_clock();if(!game.menu_open)aster_audio_resume();}
void aster_app_stop(void){audio_end();aster_logf(ASTER_LOG_INFO,"V2 END frames=%u errors=%u collisions=%u cachePeak=%u assetErrors=%u",game.frames,game.errors,game.collisions,cache_peak,asset_errors);}
void aster_app_destroy(void){audio_end();free_map();free_images();if(surface)aster_surface_destroy(surface);surface=0;pixels=0;}



