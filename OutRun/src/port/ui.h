/* Shared drawing and touch bounds, scaled from the 480x320 layout. */
#ifndef OUTRUN_UI_H
#define OUTRUN_UI_H
typedef struct {int x,y,w,h;} UiRect;
enum {UI_LEFT,UI_RIGHT,UI_BRAKE,UI_GAS,UI_MENU,UI_AUTO,UI_PREV,UI_NEXT,UI_START,UI_RESUME,UI_RESTART,UI_MAPS};
static inline UiRect ui_rect(int id){
 int w=(int)screen_w,h=(int)screen_h;UiRect r={0};
 switch(id){
 case UI_LEFT:r=(UiRect){8,248,84,64};break;
 case UI_RIGHT:r=(UiRect){100,248,84,64};break;
 case UI_BRAKE:r=(UiRect){376,248,96,64};break;
 case UI_GAS:r=(UiRect){272,248,96,64};break;
 case UI_MENU:r=(UiRect){388,34,84,44};break;
 case UI_AUTO:r=(UiRect){280,34,100,44};break;
 case UI_PREV:r=(UiRect){80,108,64,56};break;
 case UI_NEXT:r=(UiRect){336,108,64,56};break;
 case UI_START:r=(UiRect){108,180,264,56};break;
 case UI_RESUME:r=(UiRect){108,100,264,48};break;
 case UI_RESTART:r=(UiRect){108,156,264,48};break;
 case UI_MAPS:r=(UiRect){108,212,264,48};break;
 }
 r.x=r.x*w/480;r.w=r.w*w/480;r.y=r.y*h/320;r.h=r.h*h/320;return r;
}
static inline bool ui_hit(int id,int x,int y){UiRect r=ui_rect(id);return x>=r.x&&x<r.x+r.w&&y>=r.y&&y<r.y+r.h;}
#endif
