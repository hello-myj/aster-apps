/* SPDX-License-Identifier: GPL-3.0-or-later
 * A small SDL service adapter, not a second implementation of game rules. */
#include "tyrian_port.h"
#include <aster_gfx_engine.h>
#include "../../common/p4_gamepad.h"
#include <SDL.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "localization.h"

static SDL_Event queue[128];static unsigned front,back;
static uint8_t physical[256],state[SDL_NUM_SCANCODES];
static int text_active;
static struct {uint32_t slot;int active,bits,joystick,mask;} contacts[10];
static int joy_x,joy_y;
static uint32_t repeat_at;
static int joystick_bits(int x,int y){pad_stick_position(x,y,&joy_x,&joy_y);return pad_direction(x,y);}
static SDL_Scancode from_evdev(unsigned c){
 switch(c){case 1:return SDL_SCANCODE_ESCAPE;case 14:return SDL_SCANCODE_BACKSPACE;case 15:return SDL_SCANCODE_TAB;case 28:return SDL_SCANCODE_RETURN;case 29:return SDL_SCANCODE_LCTRL;case 42:return SDL_SCANCODE_LSHIFT;case 54:return SDL_SCANCODE_RSHIFT;case 56:return SDL_SCANCODE_LALT;case 57:return SDL_SCANCODE_SPACE;case 103:return SDL_SCANCODE_UP;case 105:return SDL_SCANCODE_LEFT;case 106:return SDL_SCANCODE_RIGHT;case 108:return SDL_SCANCODE_DOWN;case 111:return SDL_SCANCODE_DELETE;}
 static const int ev[]={30,48,46,32,18,33,34,35,23,36,37,38,50,49,24,25,16,19,31,20,22,47,17,45,21,44};
 for(int i=0;i<26;i++)if(c==(unsigned)ev[i])return SDL_SCANCODE_A+i;
 if(c>=2&&c<=10)return SDL_SCANCODE_1+c-2;if(c==11)return SDL_SCANCODE_0;
 if(c>=59&&c<=68)return SDL_SCANCODE_F1+c-59;return SDL_SCANCODE_UNKNOWN;
}
static SDL_Keycode key_sym(SDL_Scancode s){if(s>=SDL_SCANCODE_A&&s<=SDL_SCANCODE_Z)return 'a'+s-SDL_SCANCODE_A;if(s>=SDL_SCANCODE_1&&s<=SDL_SCANCODE_9)return '1'+s-SDL_SCANCODE_1;if(s==SDL_SCANCODE_0)return '0';switch(s){case SDL_SCANCODE_RETURN:return SDLK_RETURN;case SDL_SCANCODE_ESCAPE:return SDLK_ESCAPE;case SDL_SCANCODE_SPACE:return SDLK_SPACE;case SDL_SCANCODE_BACKSPACE:return SDLK_BACKSPACE;case SDL_SCANCODE_TAB:return SDLK_TAB;default:return SDL_SCANCODE_TO_KEYCODE(s);}}
int SDL_PushEvent(SDL_Event *e){if(back-front>=128)return -1;queue[back++%128]=*e;return 1;}
static void key_event(SDL_Scancode s,int on){
 if(s<=0||s>=SDL_NUM_SCANCODES||state[s]==on)return;
 state[s]=on;SDL_Event e={0};e.type=on?SDL_KEYDOWN:SDL_KEYUP;e.key.state=on?SDL_PRESSED:SDL_RELEASED;e.key.keysym.scancode=s;e.key.keysym.sym=key_sym(s);
 if(s>=SDL_SCANCODE_RIGHT&&s<=SDL_SCANCODE_UP)repeat_at=tyrian_ticks()+350;
 if(state[SDL_SCANCODE_LSHIFT]||state[SDL_SCANCODE_RSHIFT])e.key.keysym.mod|=KMOD_SHIFT;
 if(state[SDL_SCANCODE_LCTRL])e.key.keysym.mod|=KMOD_LCTRL;if(state[SDL_SCANCODE_LALT])e.key.keysym.mod|=KMOD_LALT;
 SDL_PushEvent(&e);
 if(on&&text_active&&e.key.keysym.sym>=32&&e.key.keysym.sym<=126){SDL_Event t={0};t.type=SDL_TEXTINPUT;t.text.text[0]=e.key.keysym.sym;if((e.key.keysym.mod&KMOD_SHIFT)&&t.text.text[0]>='a'&&t.text.text[0]<='z')t.text.text[0]-=32;SDL_PushEvent(&t);}
}
static int touch_bits(unsigned x,unsigned y){
 int b=0;
 if(pad_circle(x,y,404,422,43))b|=16;
 if(pad_box(x,y,194,350,112,44))b|=32;
 if(pad_circle(x,y,224,438,28))b|=64;
 if(pad_circle(x,y,290,438,28))b|=128;
 if(pad_box(x,y,388,6,84,28))b|=256;
 if(pad_box(x,y,302,6,78,28))b|=512;
 return b;
}
static void rebuild(void){
 uint8_t next[SDL_NUM_SCANCODES]={0};for(unsigned i=0;i<256;i++)if(physical[i])next[from_evdev(i)]=1;
 if(!text_active){
  const unsigned pc[]={30,32,17,31,44,45,46,47};const SDL_Scancode sc[]={SDL_SCANCODE_LEFT,SDL_SCANCODE_RIGHT,SDL_SCANCODE_UP,SDL_SCANCODE_DOWN,SDL_SCANCODE_SPACE,SDL_SCANCODE_RETURN,SDL_SCANCODE_LCTRL,SDL_SCANCODE_LALT};
  for(unsigned i=0;i<8;i++)if(physical[pc[i]]){next[from_evdev(pc[i])]=0;next[sc[i]]=1;}
 }
 int bits=0;for(unsigned i=0;i<10;i++)if(contacts[i].active)bits|=contacts[i].bits;
 const SDL_Scancode sc[]={SDL_SCANCODE_LEFT,SDL_SCANCODE_RIGHT,SDL_SCANCODE_UP,SDL_SCANCODE_DOWN,SDL_SCANCODE_SPACE,SDL_SCANCODE_RETURN,SDL_SCANCODE_LCTRL,SDL_SCANCODE_LALT,SDL_SCANCODE_ESCAPE,SDL_SCANCODE_P};
 for(unsigned i=0;i<10;i++)if(bits&(1<<i))next[sc[i]]=1;
 for(int s=1;s<SDL_NUM_SCANCODES;s++)key_event(s,next[s]);
}
void tyrian_input(const aster_event_t *ev){
 if(ev->type==ASTER_EVENT_KEY){if(ev->u.key.code<256)physical[ev->u.key.code]=ev->u.key.pressed!=0;}
 else if(ev->type==ASTER_EVENT_TOUCH){
  int slot=-1;for(unsigned i=0;i<10;i++)if(contacts[i].active&&contacts[i].slot==ev->u.touch.slot)slot=i;
  if(slot<0&&ev->u.touch.action==ASTER_TOUCH_DOWN)for(unsigned i=0;i<10;i++)if(!contacts[i].active){slot=i;contacts[i].slot=ev->u.touch.slot;break;}
  if(slot>=0){
   int x=ev->u.touch.x,y=ev->u.touch.y;
   if(ev->u.touch.action==ASTER_TOUCH_DOWN){
    int owned=0;for(unsigned j=0;j<10;j++)if(j!=(unsigned)slot&&contacts[j].active&&contacts[j].joystick)owned=1;
    contacts[slot].joystick=!owned&&pad_circle(x,y,PAD_X,PAD_Y,62);
    contacts[slot].mask=touch_bits(x,y);
   }
   contacts[slot].bits=contacts[slot].joystick?joystick_bits(x,y):(touch_bits(x,y)&contacts[slot].mask);
   contacts[slot].active=ev->u.touch.action!=ASTER_TOUCH_UP;
   if(!contacts[slot].active&&contacts[slot].joystick){joy_x=joy_y=0;contacts[slot].joystick=0;}
  }
 }
 rebuild();
 tyrian_video_invalidate();
}
void tyrian_input_clear(void){memset(physical,0,sizeof(physical));memset(contacts,0,sizeof(contacts));joy_x=joy_y=0;rebuild();tyrian_video_invalidate();}
int SDL_PollEvent(SDL_Event *e){
 if(front==back&&(int32_t)(tyrian_ticks()-repeat_at)>=0){repeat_at=tyrian_ticks()+140;for(int s=SDL_SCANCODE_RIGHT;s<=SDL_SCANCODE_UP;s++)if(state[s]){SDL_Event r={0};r.type=SDL_KEYDOWN;r.key.state=SDL_PRESSED;r.key.repeat=1;r.key.keysym.scancode=s;r.key.keysym.sym=key_sym(s);SDL_PushEvent(&r);}}
 if(front==back)return 0;*e=queue[front++%128];return 1;
}
Uint32 SDL_GetTicks(void){return tyrian_ticks();}
void SDL_Delay(Uint32 ms){tyrian_yield(ms?ms:1);}
int SDL_Init(Uint32 flags){(void)flags;SDL_Event e={0};e.type=SDL_WINDOWEVENT;e.window.event=SDL_WINDOWEVENT_FOCUS_GAINED;SDL_PushEvent(&e);return 0;}
int SDL_InitSubSystem(Uint32 flags){(void)flags;return 0;}
void SDL_QuitSubSystem(Uint32 flags){(void)flags;}
void SDL_Quit(void){}
SDL_bool SDL_SetHint(const char *name,const char *value){(void)name;(void)value;return SDL_TRUE;}
int SDL_ShowCursor(int toggle){(void)toggle;return 0;}
int SDL_SetRelativeMouseMode(SDL_bool enabled){(void)enabled;return 0;}
void SDL_StartTextInput(void){text_active=1;rebuild();}
void SDL_StopTextInput(void){text_active=0;rebuild();}
const Uint8 *SDL_GetKeyboardState(int *n){if(n)*n=SDL_NUM_SCANCODES;return state;}
const char *SDL_GetError(void){return "Aster platform adapter";}
char *SDL_GetBasePath(void){char *p=malloc(7);if(p)memcpy(p,"/data/",7);return p;}
int SDL_GetNumVideoDisplays(void){return 1;}
void *SDL_malloc(size_t n){return malloc(n);}void *SDL_calloc(size_t n,size_t s){return calloc(n,s);}void *SDL_realloc(void *p,size_t n){return realloc(p,n);}void SDL_free(void *p){free(p);}
size_t SDL_strlcpy(char *d,const char *s,size_t n){size_t z=strlen(s);if(n){size_t t=z<n-1?z:n-1;memcpy(d,s,t);d[t]=0;}return z;}
SDL_PixelFormat *SDL_AllocFormat(Uint32 format){SDL_PixelFormat *p=calloc(1,sizeof(*p));if(!p)return NULL;p->format=format;p->BitsPerPixel=format==SDL_PIXELFORMAT_INDEX8?8:16;p->BytesPerPixel=p->BitsPerPixel/8;p->Rmask=0xf800;p->Gmask=0x07e0;p->Bmask=0x001f;return p;}
void SDL_FreeFormat(SDL_PixelFormat *p){free(p);}
Uint32 SDL_MapRGB(const SDL_PixelFormat *f,Uint8 r,Uint8 g,Uint8 b){(void)f;return ((r>>3)<<11)|((g>>2)<<5)|(b>>3);}
SDL_Surface *SDL_CreateRGBSurface(Uint32 flags,int w,int h,int depth,Uint32 r,Uint32 g,Uint32 b,Uint32 a){
 (void)r;(void)g;(void)b;(void)a;if(w<=0||h<=0||depth!=8)return NULL;
 SDL_Surface *s=calloc(1,sizeof(*s));if(!s)return NULL;s->format=SDL_AllocFormat(SDL_PIXELFORMAT_INDEX8);s->pixels=calloc(w,h);if(!s->format||!s->pixels){SDL_FreeSurface(s);return NULL;}s->flags=flags;s->w=w;s->h=h;s->pitch=w;s->clip_rect=(SDL_Rect){0,0,w,h};s->refcount=1;return s;
}
void SDL_FreeSurface(SDL_Surface *s){if(s){SDL_FreeFormat(s->format);free(s->pixels);free(s);}}
int SDL_FillRect(SDL_Surface *s,const SDL_Rect *rect,Uint32 color){SDL_Rect r=rect?*rect:(SDL_Rect){0,0,s->w,s->h};int x0=r.x<0?0:r.x,y0=r.y<0?0:r.y,x1=r.x+r.w,y1=r.y+r.h;if(x1>s->w)x1=s->w;if(y1>s->h)y1=s->h;for(int y=y0;y<y1;y++)if(x1>x0)memset((uint8_t*)s->pixels+y*s->pitch+x0,color,x1-x0);return 0;}
int SDL_UpperBlit(SDL_Surface *src,const SDL_Rect *sr,SDL_Surface *dst,SDL_Rect *dr){SDL_Rect r=sr?*sr:(SDL_Rect){0,0,src->w,src->h};int dx=dr?dr->x:0,dy=dr?dr->y:0;for(int y=0;y<r.h;y++)for(int x=0;x<r.w;x++)if(r.x+x>=0&&r.x+x<src->w&&r.y+y>=0&&r.y+y<src->h&&dx+x>=0&&dx+x<dst->w&&dy+y>=0&&dy+y<dst->h)((uint8_t*)dst->pixels)[(dy+y)*dst->pitch+dx+x]=((uint8_t*)src->pixels)[(r.y+y)*src->pitch+r.x+x];return 0;}
static const struct{SDL_Scancode scan;const char *name;} names[]={{SDL_SCANCODE_UP,"Up"},{SDL_SCANCODE_DOWN,"Down"},{SDL_SCANCODE_LEFT,"Left"},{SDL_SCANCODE_RIGHT,"Right"},{SDL_SCANCODE_RETURN,"Return"},{SDL_SCANCODE_SPACE,"Space"},{SDL_SCANCODE_ESCAPE,"Escape"},{SDL_SCANCODE_LCTRL,"Left Ctrl"},{SDL_SCANCODE_LALT,"Left Alt"},{SDL_SCANCODE_LSHIFT,"Left Shift"},{SDL_SCANCODE_BACKSPACE,"Backspace"},{SDL_SCANCODE_TAB,"Tab"}};
const char *SDL_GetScancodeName(SDL_Scancode s){static char letters[26][2];for(unsigned i=0;i<sizeof(names)/sizeof(names[0]);i++)if(names[i].scan==s)return names[i].name;if(s>=SDL_SCANCODE_A&&s<=SDL_SCANCODE_Z){unsigned i=s-SDL_SCANCODE_A;letters[i][0]='A'+i;return letters[i];}return "";}
SDL_Scancode SDL_GetScancodeFromName(const char *s){for(unsigned i=0;i<sizeof(names)/sizeof(names[0]);i++)if(!strcmp(names[i].name,s))return names[i].scan;if(strlen(s)==1){char c=s[0];if(c>='a'&&c<='z')c-=32;if(c>='A'&&c<='Z')return SDL_SCANCODE_A+c-'A';}return SDL_SCANCODE_UNKNOWN;}
int SDL_NumJoysticks(void){return 0;}SDL_Joystick *SDL_JoystickOpen(int n){(void)n;return NULL;}void SDL_JoystickClose(SDL_Joystick *j){(void)j;}void SDL_JoystickUpdate(void){}
int SDL_JoystickEventState(int state_){(void)state_;return SDL_IGNORE;}const char *SDL_JoystickName(SDL_Joystick *j){(void)j;return "";}int SDL_JoystickNumAxes(SDL_Joystick *j){(void)j;return 0;}int SDL_JoystickNumButtons(SDL_Joystick *j){(void)j;return 0;}int SDL_JoystickNumHats(SDL_Joystick *j){(void)j;return 0;}Sint16 SDL_JoystickGetAxis(SDL_Joystick *j,int n){(void)j;(void)n;return 0;}Uint8 SDL_JoystickGetButton(SDL_Joystick *j,int n){(void)j;(void)n;return 0;}Uint8 SDL_JoystickGetHat(SDL_Joystick *j,int n){(void)j;(void)n;return 0;}
static void logv(int level,const char *fmt,va_list ap){char b[768];vsnprintf(b,sizeof(b),fmt,ap);aster_log(level,b,strlen(b));}
void SDL_Log(const char *fmt,...){va_list ap;va_start(ap,fmt);logv(ASTER_LOG_INFO,fmt,ap);va_end(ap);}
void SDL_LogDebug(int cat,const char *fmt,...){(void)cat;(void)fmt;}
void SDL_LogWarn(int cat,const char *fmt,...){(void)cat;va_list ap;va_start(ap,fmt);logv(ASTER_LOG_WARN,fmt,ap);va_end(ap);}
void SDL_LogError(int cat,const char *fmt,...){(void)cat;va_list ap;va_start(ap,fmt);logv(ASTER_LOG_ERROR,fmt,ap);va_end(ap);}
void SDL_LogSetPriority(int cat,SDL_LogPriority priority){(void)cat;(void)priority;}
void SDL_LogGetOutputFunction(SDL_LogOutputFunction *cb,void **userdata){if(cb)*cb=NULL;if(userdata)*userdata=NULL;}
int SDL_ShowSimpleMessageBox(Uint32 f,const char *title,const char *message,SDL_Window *win){(void)f;(void)win;aster_printf("%s: %s",title,message);return 0;}
void tyrian_overlay(uint16_t *pixels,unsigned pitch,unsigned w,unsigned h){
 if(w!=PAD_W||h!=PAD_H)return;aster_gfx_t g;aster_gfx_init(&g,pixels,w,h,pitch*2);
 int active=0;for(unsigned i=0;i<10;i++)if(contacts[i].active&&contacts[i].joystick)active=1;
 int dx=active?joy_x:(state[SDL_SCANCODE_RIGHT]-state[SDL_SCANCODE_LEFT])*22;
 int dy=active?joy_y:(state[SDL_SCANCODE_DOWN]-state[SDL_SCANCODE_UP])*22;
 pad_base(&g,"OPENTYRIAN",0x34fd);pad_stick(&g,dx,dy,active,0x34fd);
 pad_top_button(&g,302,78,"PAUSE",state[SDL_SCANCODE_P],0x34fd);
 pad_top_button(&g,388,84,"BACK",state[SDL_SCANCODE_ESCAPE],0xc32a);
 pad_action(&g,404,422,39,"FIRE",NULL,state[SDL_SCANCODE_SPACE],0xfd0a);
 pad_action(&g,224,438,24,"L",NULL,state[SDL_SCANCODE_LCTRL],0x3d5f);
 pad_action(&g,290,438,24,"R",NULL,state[SDL_SCANCODE_LALT],0x3d5f);
 pad_round(&g,194,350,112,44,10,state[SDL_SCANCODE_RETURN]?0x34fd:0x2948);
 tyrian_zh_rgb(pixels,pitch,234,364,"确认",0xffff);
 tyrian_zh_rgb(pixels,pitch,388,438,"射击",0xffff);
 pad_center(&g,257,402,"SIDEKICKS",1,0x9cd3);
}
