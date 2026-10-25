/* SDL services used by the unchanged upstream engine, backed by Aster events/libc. */
#include <aster_imports.h>
#include <aster_gfx_engine.h>
#include <SDL.h>
#include "game/utils/settings.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
static uint8_t state[SDL_NUM_SCANCODES],physical[256];
static int inArena,pcTouch=1,textActive=1;
static SDL_Event events[64];static unsigned eventRead,eventWrite;
typedef struct {uint32_t slot;int active,bits;} Contact;static Contact contacts[10];
static int direction_bits(int x,int y){int b=0;if(x>=8&&x<174&&y>=220&&y<310){if(x<63)b|=1;if(x>117)b|=2;if(y<250)b|=4;if(y>=280)b|=8;}if(x>=300&&x<373&&y>=254&&y<310)b|=16;if(x>=390&&x<472&&y>=254&&y<310)b|=32;if(x>=390&&x<472&&y>=204&&y<246)b|=48;if(x>=409&&x<472&&y>=42&&y<67)b|=64;return b;}
static SDL_Scancode scan(int c){switch(c){case 1:return SDL_SCANCODE_ESCAPE;case 14:return SDL_SCANCODE_BACKSPACE;case 15:return SDL_SCANCODE_TAB;case 41:return SDL_SCANCODE_GRAVE;case 28:return SDL_SCANCODE_RETURN;case 29:return SDL_SCANCODE_LCTRL;case 42:return SDL_SCANCODE_LSHIFT;case 54:return SDL_SCANCODE_RSHIFT;case 56:return SDL_SCANCODE_LALT;case 57:return SDL_SCANCODE_SPACE;case 97:return SDL_SCANCODE_RCTRL;case 100:return SDL_SCANCODE_RALT;case 103:return SDL_SCANCODE_UP;case 105:return SDL_SCANCODE_LEFT;case 106:return SDL_SCANCODE_RIGHT;case 108:return SDL_SCANCODE_DOWN;case 102:return SDL_SCANCODE_HOME;case 107:return SDL_SCANCODE_END;case 104:return SDL_SCANCODE_PAGEUP;case 109:return SDL_SCANCODE_PAGEDOWN;case 111:return SDL_SCANCODE_DELETE;}
 if(c>=59&&c<=68)return SDL_SCANCODE_F1+c-59;
 static const int ev[]={30,48,46,32,18,33,34,35,23,36,37,38,50,49,24,25,16,19,31,20,22,47,17,45,21,44};for(int i=0;i<26;i++)if(c==ev[i])return SDL_SCANCODE_A+i;if(c>=2&&c<=10)return SDL_SCANCODE_1+c-2;if(c==11)return SDL_SCANCODE_0;return SDL_SCANCODE_UNKNOWN;}
static SDL_Keycode sym(SDL_Scancode s){if(s>=SDL_SCANCODE_A&&s<=SDL_SCANCODE_Z)return 'a'+s-SDL_SCANCODE_A;if(s>=SDL_SCANCODE_1&&s<=SDL_SCANCODE_9)return '1'+s-SDL_SCANCODE_1;if(s==SDL_SCANCODE_0)return '0';if(s==SDL_SCANCODE_RETURN)return SDLK_RETURN;if(s==SDL_SCANCODE_ESCAPE)return SDLK_ESCAPE;if(s==SDL_SCANCODE_SPACE)return SDLK_SPACE;if(s==SDL_SCANCODE_BACKSPACE)return SDLK_BACKSPACE;if(s==SDL_SCANCODE_TAB)return SDLK_TAB;if(s==SDL_SCANCODE_GRAVE)return SDLK_BACKQUOTE;return SDL_SCANCODE_TO_KEYCODE(s);}
static void push(SDL_Scancode s,int on){if(s==SDL_SCANCODE_UNKNOWN||state[s]==on)return;state[s]=on;SDL_Event e={0};e.type=on?SDL_KEYDOWN:SDL_KEYUP;e.key.state=on?SDL_PRESSED:SDL_RELEASED;e.key.keysym.scancode=s;e.key.keysym.sym=sym(s);if(state[SDL_SCANCODE_LSHIFT])e.key.keysym.mod|=KMOD_LSHIFT;if(state[SDL_SCANCODE_RSHIFT])e.key.keysym.mod|=KMOD_RSHIFT;if(state[SDL_SCANCODE_LCTRL])e.key.keysym.mod|=KMOD_LCTRL;if(state[SDL_SCANCODE_RCTRL])e.key.keysym.mod|=KMOD_RCTRL;if(eventWrite-eventRead<64)events[eventWrite++%64]=e;if(on&&textActive&&e.key.keysym.sym>=32&&e.key.keysym.sym<=126&&!(e.key.keysym.mod&(KMOD_CTRL|KMOD_ALT))){SDL_Event t={0};t.type=SDL_TEXTINPUT;char c=e.key.keysym.sym;if(e.key.keysym.mod&KMOD_SHIFT){if(c>='a'&&c<='z')c-=32;else{const char *plain="1234567890-=[];'/.,\\`",*shift="!@#$%^&*()_+{}:\"?><|~";const char *at=strchr(plain,c);if(at)c=shift[at-plain];}}t.text.text[0]=c;if(eventWrite-eventRead<64)events[eventWrite++%64]=t;}}
static int pc_bits(void){int b=0;if(!pcTouch||!inArena)return 0;if(physical[30])b|=1;if(physical[32])b|=2;if(physical[17])b|=4;if(physical[31])b|=8;if(physical[44])b|=16;if(physical[45])b|=32;if(physical[46])b|=48;return b;}
static void controls(SDL_Scancode map[7]){
 const SDL_Scancode menu[]={SDL_SCANCODE_LEFT,SDL_SCANCODE_RIGHT,SDL_SCANCODE_UP,SDL_SCANCODE_DOWN,SDL_SCANCODE_RETURN,SDL_SCANCODE_RSHIFT,SDL_SCANCODE_ESCAPE};memcpy(map,menu,sizeof(menu));
 if(inArena){settings_keyboard *k=&settings_get()->keys;const char *n[]={k->key1_walk_back,k->key1_walk_right,k->key1_jump_up,k->key1_duck,k->key1_punch,k->key1_kick};for(int i=0;i<6;i++)map[i]=SDL_GetScancodeFromName(n[i]);}
}
static int is_alias(int c){return c==30||c==32||c==17||c==31||c==44||c==45||c==46;}
static void rebuild(void){uint8_t next[SDL_NUM_SCANCODES]={0};for(int c=0;c<256;c++)if(physical[c]&&!(pcTouch&&inArena&&is_alias(c)))next[scan(c)]=1;int b=pc_bits();for(int i=0;i<10;i++)if(inArena&&contacts[i].active)b|=contacts[i].bits;SDL_Scancode map[7];controls(map);for(int i=0;i<7;i++)if(b&(1<<i))next[map[i]]=1;for(int s=1;s<SDL_NUM_SCANCODES;s++)push(s,next[s]);}
void omf_input_mode(int arena){if(inArena!=arena){inArena=arena;rebuild();}}
void omf_input_cancel(void){memset(physical,0,sizeof(physical));memset(contacts,0,sizeof(contacts));rebuild();}
void omf_input_event(const aster_event_t *e){if(e->type==ASTER_EVENT_KEY){if(e->u.key.code==66&&e->u.key.pressed&&!physical[66])pcTouch=!pcTouch;if(e->u.key.code<256)physical[e->u.key.code]=e->u.key.pressed!=0;}else if(e->type==ASTER_EVENT_TOUCH){int at=-1;for(int i=0;i<10;i++)if(contacts[i].active&&contacts[i].slot==e->u.touch.slot)at=i;if(at<0&&e->u.touch.action==ASTER_TOUCH_DOWN)for(int i=0;i<10;i++)if(!contacts[i].active){at=i;contacts[i].slot=e->u.touch.slot;break;}if(at>=0){contacts[at].active=e->u.touch.action!=ASTER_TOUCH_UP;contacts[at].bits=direction_bits(e->u.touch.x,e->u.touch.y);}}rebuild();}
int SDL_PollEvent(SDL_Event *e){if(eventRead==eventWrite)return 0;*e=events[eventRead++%64];return 1;}
const Uint8 *SDL_GetKeyboardState(int *n){if(n)*n=SDL_NUM_SCANCODES;return state;}
Uint64 SDL_GetTicks64(void){return aster_time_now_ms();}Uint32 SDL_GetTicks(void){return aster_time_now_ms();}
const char *SDL_GetError(void){return "Aster platform operation unavailable";}
void SDL_free(void *p){free(p);}char *SDL_GetClipboardText(void){return strdup("");}SDL_bool SDL_HasClipboardText(void){return SDL_FALSE;}
static const struct {SDL_Scancode s;const char *n;} names[]={{SDL_SCANCODE_RETURN,"Return"},{SDL_SCANCODE_ESCAPE,"Escape"},{SDL_SCANCODE_UP,"Up"},{SDL_SCANCODE_DOWN,"Down"},{SDL_SCANCODE_LEFT,"Left"},{SDL_SCANCODE_RIGHT,"Right"},{SDL_SCANCODE_RSHIFT,"Right Shift"},{SDL_SCANCODE_LSHIFT,"Left Shift"},{SDL_SCANCODE_LCTRL,"Left Ctrl"},{SDL_SCANCODE_RCTRL,"Right Ctrl"},{SDL_SCANCODE_HOME,"Home"},{SDL_SCANCODE_END,"End"},{SDL_SCANCODE_PAGEUP,"PageUp"},{SDL_SCANCODE_PAGEDOWN,"PageDown"},{SDL_SCANCODE_SPACE,"Space"},{SDL_SCANCODE_TAB,"Tab"},{SDL_SCANCODE_BACKSPACE,"Backspace"}};
SDL_Scancode SDL_GetScancodeFromName(const char *n){if(!n||!n[0])return SDL_SCANCODE_UNKNOWN;for(unsigned i=0;i<sizeof(names)/sizeof(*names);i++)if(!strcasecmp(n,names[i].n))return names[i].s;if(!n[1]){int c=tolower((unsigned char)n[0]);if(c>='a'&&c<='z')return SDL_SCANCODE_A+c-'a';if(c>='1'&&c<='9')return SDL_SCANCODE_1+c-'1';if(c=='0')return SDL_SCANCODE_0;}return SDL_SCANCODE_UNKNOWN;}
const char *SDL_GetScancodeName(SDL_Scancode s){for(unsigned i=0;i<sizeof(names)/sizeof(*names);i++)if(s==names[i].s)return names[i].n;static const char *letters[]={"A","B","C","D","E","F","G","H","I","J","K","L","M","N","O","P","Q","R","S","T","U","V","W","X","Y","Z"};if(s>=SDL_SCANCODE_A&&s<=SDL_SCANCODE_Z)return letters[s-SDL_SCANCODE_A];return "";}
static Sint64 rwsize(SDL_RWops *r){FILE *f=r->hidden.stdio.fp;long p=ftell(f);fseek(f,0,SEEK_END);long n=ftell(f);fseek(f,p,SEEK_SET);return n;}
static Sint64 rwseek(SDL_RWops *r,Sint64 n,int w){if(n>0x7fffffff||n<-0x7fffffff)return -1;return fseek(r->hidden.stdio.fp,n,w)?-1:ftell(r->hidden.stdio.fp);}
static size_t rwread(SDL_RWops *r,void *p,size_t s,size_t n){return fread(p,s,n,r->hidden.stdio.fp);}static size_t rwwrite(SDL_RWops *r,const void *p,size_t s,size_t n){return fwrite(p,s,n,r->hidden.stdio.fp);}static int rwclose(SDL_RWops *r){int e=fclose(r->hidden.stdio.fp);free(r);return e;}
SDL_RWops *SDL_RWFromFile(const char *n,const char *mode){FILE *f=fopen(n,mode);if(!f)return NULL;SDL_RWops *r=calloc(1,sizeof(*r));if(!r){fclose(f);return NULL;}r->size=rwsize;r->seek=rwseek;r->read=rwread;r->write=rwwrite;r->close=rwclose;r->type=SDL_RWOPS_STDFILE;r->hidden.stdio.fp=f;return r;}
size_t SDL_RWwrite(SDL_RWops *r,const void *p,size_t s,size_t n){return r->write(r,p,s,n);}size_t SDL_RWread(SDL_RWops *r,void *p,size_t s,size_t n){return r->read(r,p,s,n);}int SDL_RWclose(SDL_RWops *r){return r->close(r);}
/* No gamepad is exposed by the current Aster ABI. Keyboard/touch remain active. */
int SDL_NumJoysticks(void){return 0;}SDL_Joystick *SDL_JoystickOpen(int n){return NULL;}SDL_bool SDL_JoystickGetAttached(SDL_Joystick *j){return SDL_FALSE;}void SDL_JoystickClose(SDL_Joystick *j){}const char *SDL_JoystickNameForIndex(int n){return NULL;}
SDL_GameController *SDL_GameControllerOpen(int n){return NULL;}SDL_bool SDL_GameControllerGetAttached(SDL_GameController *j){return SDL_FALSE;}void SDL_GameControllerClose(SDL_GameController *j){}const char *SDL_GameControllerNameForIndex(int n){return NULL;}SDL_Joystick *SDL_GameControllerGetJoystick(SDL_GameController *j){return NULL;}Sint16 SDL_GameControllerGetAxis(SDL_GameController *j,SDL_GameControllerAxis a){return 0;}Uint8 SDL_GameControllerGetButton(SDL_GameController *j,SDL_GameControllerButton b){return 0;}
SDL_Haptic *SDL_HapticOpenFromJoystick(SDL_Joystick *j){return NULL;}void SDL_HapticClose(SDL_Haptic *j){}int SDL_HapticRumbleSupported(SDL_Haptic *j){return 0;}int SDL_HapticRumbleInit(SDL_Haptic *j){return -1;}int SDL_HapticRumblePlay(SDL_Haptic *j,float strength,Uint32 duration){return -1;}

static uint16_t shade(uint16_t a,uint16_t b,int v){return (((((a>>11)*(255-v)+(b>>11)*v)/255)&31)<<11)|(((((a>>5&63)*(255-v)+(b>>5&63)*v)/255)&63)<<5)|(((a&31)*(255-v)+(b&31)*v)/255);}
static void control_button(uint16_t *out,unsigned pitch,aster_gfx_t *g,int x,int y,int w,int h,const char *name,int down){for(int yy=y;yy<y+h;yy++)for(int xx=x;xx<x+w;xx++)out[yy*pitch+xx]=shade(out[yy*pitch+xx],down?0x19f4:0x0842,down?180:55);aster_gfx_frame_rect(g,x,y,w,h,down?0xbfff:0x7c96);aster_gfx_text(g,x+(w-(int)strlen(name)*6)/2,y+(h-7)/2,name,0xffff,0,0);}
void omf_controls_overlay(uint16_t *out,unsigned pitch){SDL_Scancode map[7];controls(map);int b=0;for(int j=0;j<10;j++)if(contacts[j].active)b|=contacts[j].bits;for(int i=0;i<7;i++)if(state[map[i]])b|=1<<i;aster_gfx_t g;aster_gfx_init(&g,out,480,320,pitch*2);
 control_button(out,pitch,&g,8,250,54,30,"<",b&1);control_button(out,pitch,&g,120,250,54,30,">",b&2);control_button(out,pitch,&g,64,220,54,30,"UP",b&4);control_button(out,pitch,&g,64,280,54,30,"DOWN",b&8);
 control_button(out,pitch,&g,8,220,54,30,"UP-L",(b&5)==5);control_button(out,pitch,&g,120,220,54,30,"UP-R",(b&6)==6);control_button(out,pitch,&g,8,280,54,30,"DN-L",(b&9)==9);control_button(out,pitch,&g,120,280,54,30,"DN-R",(b&10)==10);
 control_button(out,pitch,&g,300,254,73,56,"PUNCH",b&16);control_button(out,pitch,&g,390,254,82,56,"KICK",b&32);control_button(out,pitch,&g,390,204,82,42,"P+K",(b&48)==48);control_button(out,pitch,&g,409,42,63,25,"ESC",b&64);
}

SDL_bool SDL_IsTextInputActive(void){return textActive?SDL_TRUE:SDL_FALSE;}
void SDL_StartTextInput(void){textActive=1;}
void SDL_StopTextInput(void){textActive=0;}
int omf_menu_touch_bits(void){if(inArena)return 0;int b=0;for(int i=0;i<10;i++)if(contacts[i].active)b|=contacts[i].bits;return b;}
