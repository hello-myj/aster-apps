/* Boundary integration fixture: uses unmodified production game/render/assets. */
#include "port/engine.h"
static aster_handle_t surface;static int ticks;
int aster_app_init(void){
 aster_surface_desc_t d={.size=sizeof(d),.version=1,.format=ASTER_PIXEL_RGB565};int r=aster_surface_create(&d,sizeof(d),&surface);if(r<0)return r;
 aster_surface_buffer_t b={.size=sizeof(b),.version=1};r=aster_surface_acquire(surface,&b,sizeof(b));if(r<0)return r;
 pixels=b.ptr;screen_w=b.width;screen_h=b.height;stride=b.pitch/2;
 if(load_font()<0)return ASTER_EIO;game_reset();
 unsigned expected[]={3,5,9,13,0};
 for(int i=0;i<5;i++){
  game.state=2;game.speed=200;game.gear=1;game.remaining=200;game.position=(map.h.count-210)*SEGMENT-1;game.x=0;game.crash=0;game.route_choice=(i%2)?1:2;
  game_update(1.f/60.f);
  if(game.map_id!=expected[i]){aster_logf(ASTER_LOG_ERROR,"BOUNDARY mismatch %u != %u",game.map_id,expected[i]);return ASTER_EIO;}
  aster_logf(ASTER_LOG_INFO,"BOUNDARY OK step=%u map=%u stage=%u",i,game.map_id,game.stage);
 }
 aster_app_set_framerate(30);return 0;
}
void aster_app_event(const aster_event_t *e){
 if(e->type!=ASTER_EVENT_FRAME)return;
 game_update(1.f/60.f);game_update(1.f/60.f);draw_frame();int r=aster_surface_present(surface);
 if(r<0){aster_logf(ASTER_LOG_ERROR,"PRESENT %d",r);aster_app_exit(2);return;}
 ticks++;
 if(game.state==4){aster_logf(ASTER_LOG_INFO,"GOAL OK ticks=%u asset_errors=%u",ticks,asset_errors);aster_app_exit(asset_errors?3:0);}
 if(ticks>600)aster_app_exit(4);
}
void aster_app_stop(void){audio_end();}
void aster_app_destroy(void){free_map();free_images();if(surface)aster_surface_destroy(surface);}
