/* GPL-3.0-or-later. Responsive geometry shared by drawing and touch. */
#ifndef OUTRUN_UI_H
#define OUTRUN_UI_H
typedef struct {int x,y,w,h;} UiRect;
bool outrun_motion_enabled(void);
bool outrun_motion_available(void);
void outrun_motion_init(void);
void outrun_motion_center(void);
void outrun_motion_toggle(void);
void outrun_motion_tick(uint32_t now);
float outrun_motion_steer(void);
enum {UI_BRAKE,UI_GAS,UI_MENU,UI_AUTO,UI_PREV,UI_NEXT,UI_START,UI_RESUME,UI_RESTART,UI_MAPS,UI_MODE,UI_CENTER,UI_WHEEL,UI_NONE=-1};
static inline bool ui_large(void){return screen_h>=400;}
static inline int ui_play_height(void){return ui_large()?(int)screen_h-120:(int)screen_h;}
static inline int ui_wheel_x(void){return ui_large()?76:38;}
static inline int ui_wheel_y(void){return ui_large()?(int)screen_h-60:(int)screen_h-36;}
static inline int ui_wheel_radius(void){return ui_large()?48:27;}
static inline UiRect ui_rect(int id){
 int w=screen_w,h=screen_h,cx=w/2,cy=ui_play_height()/2;bool big=ui_large();UiRect r={0};
 switch(id){
 case UI_WHEEL:r=(UiRect){ui_wheel_x()-ui_wheel_radius()-6,ui_wheel_y()-ui_wheel_radius()-6,2*(ui_wheel_radius()+6),2*(ui_wheel_radius()+6)};break;
 case UI_BRAKE:r=big?(UiRect){w-180,h-96,64,88}:(UiRect){0,39,cx,h-39};break;
 case UI_GAS:r=big?(UiRect){w-98,h-112,80,104}:(UiRect){cx,39,w-cx,h-39};break;
 case UI_MENU:r=(UiRect){w-(big?76:66),8,big?68:58,big?28:26};break;
 case UI_AUTO:r=(UiRect){w-(big?156:132),8,big?72:60,big?28:26};break;
 case UI_MODE:r=(UiRect){122,8,60,26};break;
 case UI_CENTER:r=(UiRect){8,44,56,24};break;
 case UI_PREV:r=(UiRect){cx-(big?132:110),cy-38,big?52:40,44};break;
 case UI_NEXT:r=(UiRect){cx+(big?80:70),cy-38,big?52:40,44};break;
 case UI_START:r=(UiRect){cx-(big?132:110),cy+28,big?264:220,big?48:40};break;
 case UI_RESUME:case UI_RESTART:case UI_MAPS:r=(UiRect){cx-(big?132:110),h/2-48+(id-UI_RESUME)*48,big?264:220,40};break;
 default:break;
 }return r;
}
static inline bool ui_hit(int id,int x,int y){UiRect r=ui_rect(id);if(id==UI_WHEEL){int dx=x-ui_wheel_x(),dy=y-ui_wheel_y(),radius=ui_wheel_radius()+6;return dx*dx+dy*dy<=radius*radius;}return x>=r.x&&x<r.x+r.w&&y>=r.y&&y<r.y+r.h;}
void outrun_controls_clear(void);
int outrun_controls_touch(const aster_event_t *event);
int outrun_wheel_offset(void);
bool outrun_control_held(int id);
#endif
