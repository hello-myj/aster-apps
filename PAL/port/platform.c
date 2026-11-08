#include "pal_libc.h"
#include "SDL.h"
#include <aster_imports.h>
#include <aster_gfx_engine.h>
#include "pal_pack.h"
#include "pal_level2_resident_pack.h"
#include "pal_memory_profile.h"
#include "pal_engine_pack_provider.h"
#include "sys/stat.h"
#include "pal_aster_ui.h"

/* Fixed owners, native 320x200 game viewport and touch controls. */
struct PAL_FILE { aster_handle_t handle; uint32_t pos,size; int used,eof,error; };
static FILE files[12];
FILE *stdin,*stdout,*stderr;
int errno;
uint8_t pal_sram_framebuffer[64000], pal_sram_aux_framebuffer[64000];
uint8_t pal_sram_fbp_scanline[320];
static aster_handle_t display;
#ifndef PAL_MAX_FRAME_PIXELS
#define PAL_MAX_FRAME_PIXELS (480*480)
#endif
static uint16_t rgb565[PAL_MAX_FRAME_PIXELS];
/* Keep a clean frame so translucent controls never accumulate or leave trails. */
static uint8_t game_pixels[320*200];
static int screen_w,screen_h,joy_x,joy_y,joy_r,view_x,view_h;
static int action_x,confirm_y,back_y,action_r,touch_mode;
static int control_border_alpha;
extern int PAL_IsInDialog(void);
static aster_surface_buffer_t surface_pixels;
typedef struct {int x,y,w,h,key;const char *label;} DirectionButton;
static DirectionButton directions[4];
static int draw_controls(void);
static uint16_t palette565[256];
static uint16_t source_x[480],source_y[300];
static SDL_Event events[32];
static unsigned event_head,event_tail;
static int poll_key_down;
static uint32_t wake_at;
static int started,suspended,ended,game_code;
static uint8_t continuation[131072] __attribute__((aligned(16)));
/* Asyncify stores Wasm locals, while C stack arrays also need preservation
 * between independent Aster event callbacks. Capacity follows the linker. */
static uint8_t saved_c_stack[PAL_ASTER_STACK_BYTES] __attribute__((aligned(16)));
extern unsigned char __heap_base;
static uint32_t async_data[2];
static aster_ge_t controls_ge;
static aster_gfx_cmd_t control_commands[48];
static uint8_t control_text[512];
static int touch_key;
typedef struct {uint32_t slot;int active,mode,key;} PalFinger;
static PalFinger fingers[10];
static int held_touch[6],controls_dirty=1;
static int feedback_key;
static uint32_t feedback_until;
static uint32_t presents,perf_since,callback_peak;
extern int PAL_EngineMain(int,char **);
extern void __wrap_PAL_Quit(int);
extern void PalAsterAudioTick(void);
extern void PalAsterAudioPause(void);
extern void PalAsterAudioResume(void);
extern void AUDIO_CloseDevice(void);
/* Rebound to Binaryen's generated control functions after Asyncify. */
__attribute__((noinline)) void pal_async_start_unwind(uint32_t p) { __asm__("" : : "r"(p)); }
__attribute__((noinline)) void pal_async_start_rewind(uint32_t p) { __asm__("" : : "r"(p)); }
__attribute__((noinline)) void pal_async_stop_unwind(void) { __asm__ volatile(""); }
__attribute__((noinline)) void pal_async_stop_rewind(void) { __asm__ volatile(""); }
static volatile int async_stub_state;
__attribute__((noinline)) int pal_async_get_state(void) { return async_stub_state; }
static volatile uint32_t stack_stub;
__attribute__((noinline)) uint32_t pal_stack_get(void){return stack_stub;}
__attribute__((noinline)) void pal_stack_set(uint32_t p){stack_stub=p;}
static uint32_t suspended_stack;
#ifdef PAL_ASTER_TRACE
static unsigned trace_yields,trace_frames,trace_renders;
#endif

static void yield(uint32_t ms)
{
#ifdef PAL_ASTER_TRACE
    if(trace_yields++<6)aster_printf("PAL trace yield ms=%u state=%d",ms,pal_async_get_state());
#endif
    if (pal_async_get_state()==2) { pal_async_stop_rewind(); return; }
    wake_at=aster_time_now_ms()+ms;
    pal_async_start_unwind((uint32_t)(uintptr_t)async_data);
#ifdef PAL_ASTER_TRACE
    if(trace_yields<=6)aster_printf("PAL trace yield unwinding state=%d",pal_async_get_state());
#endif
}
Uint32 PalEngineBridge_GetTicks(void) { return aster_time_now_ms(); }
void PalEngineBridge_Delay(Uint32 ms) { yield(ms?ms:1); }
void PalEngineBridge_LogRuntimeMemory(const char *stage) { aster_printf("PAL stage: %s",stage); }
int PalEngineBridge_PollEvent(SDL_Event *out)
{
    if(event_head==event_tail){poll_key_down=0;return 0;}
    /* PAL samples SDL's held-key array after draining events. Keep a quick
     * down/up pair across two drain cycles so the press is sampled once. */
    SDL_Event *next=&events[event_tail%32];
    if(next->type==SDL_KEYUP&&poll_key_down){poll_key_down=0;return 0;}
    *out=*next;event_tail++;
    if(out->type==SDL_KEYDOWN)poll_key_down=1;
    return 1;
}
void PalEngineBridge_NotifyPaletteChanged(void) {}
void PalEngineBridge_RenderPresent(const void *p,int pitch,int w,int h)
{ (void)p;(void)pitch;(void)w;(void)h; }
bool PalEngineBridge_RenderPresentIndexed(const void *pixels,int pitch,int w,int h,
 const void *palette,int rx,int ry,int rw,int rh)
{
    (void)rx;(void)ry;(void)rw;(void)rh;
    if(w!=320||h!=200||pitch<320||!palette||!pixels)return false;
    const SDL_Color *colors=palette;
    /* Rebuild only 256 colors so palette fades remain correct; pixels just
     * look up the converted color instead of repeating RGB arithmetic. */
    for(int i=0;i<256;i++)palette565[i]=ASTER_RGB565(colors[i].r,colors[i].g,colors[i].b);
    for(int y=0;y<200;y++)memcpy(game_pixels+y*320,(const uint8_t*)pixels+y*pitch,320);
    int rc=draw_controls();
    if(rc>=0){
        presents++;
        /* Slow panels may miss every engine wait deadline. Always return to
         * the event scheduler after a game frame so touch and FRAME events
         * remain deliverable even when rendering exceeds the frame budget. */
        yield(1);
    }
    return rc>=0;
}
static int path_map(const char *src,char *dst,size_t cap)
{
    if(!src)return 0;
    if(src[0]=='0'&&src[1]==':')src+=2;
    while(*src=='/'||*src=='.')src++;
    const char *base=strstr(src,".rpg")?"/data/":"/assets/resources/";
    if(strlen(base)+strlen(src)+1>cap)return 0;
    strcpy(dst,base);strcat(dst,src);return 1;
}
FILE *fopen(const char *path,const char *mode)
{
    char mapped[256]; if(!path_map(path,mapped,sizeof(mapped)))return NULL;
    for(int i=0;i<12;i++)if(!files[i].used){
        uint32_t flags=ASTER_FS_O_RDONLY;
        if(mode[0]=='w')flags=ASTER_FS_O_WRONLY|ASTER_FS_O_CREAT|ASTER_FS_O_TRUNC;
        if(mode[0]=='a')flags=ASTER_FS_O_WRONLY|ASTER_FS_O_CREAT|ASTER_FS_O_APPEND;
        aster_handle_t handle=0;
        if(aster_fs_open(mapped,(uint32_t)strlen(mapped),flags,&handle)<0){
            errno=ENOENT;return NULL;
        }
        files[i]=(FILE){.handle=handle,.used=1};
        aster_stat_t st={.size=sizeof(st),.version=1};
        if(aster_fs_stat(mapped,(uint32_t)strlen(mapped),&st,sizeof(st))==0)files[i].size=(uint32_t)st.filesize;
        return &files[i];
    }return NULL;
}
int fclose(FILE *f){if(!f||!f->used)return EOF;aster_fs_close(f->handle);f->used=0;return 0;}
int __real_fclose(FILE *f){return fclose(f);}
size_t fread(void *p,size_t s,size_t n,FILE *f)
{ if(!s||!n||!f||!f->used||n>UINT32_MAX/s)return 0;uint32_t want=(uint32_t)(s*n),got=0;
  while(got<want){int rc=aster_fs_read(f->handle,(uint8_t*)p+got,want-got);if(rc<0){f->error=1;break;}if(!rc){f->eof=1;break;}got+=rc;}f->pos+=got;return got/s; }
size_t fwrite(const void *p,size_t s,size_t n,FILE *f)
{if(!s||!n||!f||!f->used||n>UINT32_MAX/s)return 0;uint32_t want=(uint32_t)(s*n),done=0;while(done<want){uint32_t chunk=want-done;if(chunk>2048)chunk=2048;int rc=aster_fs_write(f->handle,(const uint8_t*)p+done,chunk);if(rc<=0){f->error=1;break;}done+=(uint32_t)rc;}f->pos+=done;if(f->pos>f->size)f->size=f->pos;return done/s;}
int fseek(FILE *f,long ofs,int whence)
{if(!f||!f->used)return -1;int64_t pos=(whence==SEEK_CUR?f->pos:whence==SEEK_END?f->size:0)+(int64_t)ofs;if(pos<0||pos>INT32_MAX)return -1;int rc=aster_fs_seek(f->handle,(int32_t)pos,ASTER_FS_SEEK_SET);if(rc<0)return -1;f->pos=(uint32_t)pos;f->eof=0;return 0;}
long ftell(FILE *f){return f?f->pos:-1;}
int fgetc(FILE *f){uint8_t c;return fread(&c,1,1,f)?c:EOF;}
int fputc(int c,FILE *f){uint8_t v=c;return fwrite(&v,1,1,f)?c:EOF;}
char *fgets(char *p,int size,FILE *f){int i=0,c;while(i<size-1&&(c=fgetc(f))!=EOF){p[i++]=c;if(c=='\n')break;}p[i]=0;return i?p:NULL;}
int fputs(const char *p,FILE *f){return fwrite(p,1,strlen(p),f)==strlen(p)?0:EOF;}
int feof(FILE *f){return f?f->eof:1;} int ferror(FILE *f){return f?f->error:1;}
int fflush(FILE *f){(void)f;return 0;}void rewind(FILE *f){fseek(f,0,SEEK_SET);}
int access(const char *p,int mode){(void)mode;FILE *f=fopen(p,"r");if(!f)return -1;fclose(f);return 0;}
int unlink(const char *p){char path[256];return path_map(p,path,sizeof(path))?aster_fs_unlink(path,(uint32_t)strlen(path)):-1;}
int remove(const char *p){return unlink(p);}
int open(const char *p,int flags,...){(void)flags;FILE *f=fopen(p,"r");return f?(int)(f-files):-1;}
int close(int fd){return fd>=0&&fd<12?fclose(&files[fd]):-1;}
ssize_t read(int fd,void *p,size_t n){return fd>=0&&fd<12?(ssize_t)fread(p,1,n,&files[fd]):-1;}
off_t lseek(int fd,off_t ofs,int whence){return fd>=0&&fd<12&&fseek(&files[fd],ofs,whence)==0?(off_t)ftell(&files[fd]):-1;}
int fstat(int fd,struct stat *st){if(fd<0||fd>=12||!files[fd].used)return -1;st->st_size=files[fd].size;st->st_mode=0;return 0;}
int stat(const char *p,struct stat *st){FILE *f=fopen(p,"r");if(!f)return -1;st->st_size=f->size;st->st_mode=0;fclose(f);return 0;}

static FILE *full_file;
static int segmented_pack,current_segment;
static uint32_t logical_pack_size;
#define PAL_SEGMENT_BYTES (3u*1048576u)
static void segment_path(char *path,int index)
{aster_snprintf(path,64,"0:/pal_full.%c%c",'0'+index/10,'0'+index%10);}
static bool read_at(void *user,uint32_t ofs,uint8_t *p,uint32_t n)
{
 if(!segmented_pack){FILE *f=user;return fseek(f,(long)ofs,SEEK_SET)==0&&fread(p,1,n,f)==n;}
 if(ofs>logical_pack_size||n>logical_pack_size-ofs)return false;
 while(n){
  uint32_t index=ofs/PAL_SEGMENT_BYTES,within=ofs%PAL_SEGMENT_BYTES;
  if(index!=(uint32_t)current_segment){
   if(full_file)fclose(full_file);
   char name[64];segment_path(name,(int)index);full_file=fopen(name,"r");current_segment=(int)index;
   if(!full_file)return false;
  }
  uint32_t amount=PAL_SEGMENT_BYTES-within;if(amount>n)amount=n;
  if(!full_file||fseek(full_file,(long)within,SEEK_SET)||fread(p,1,amount,full_file)!=amount)return false;
  ofs+=amount;p+=amount;n-=amount;
 }
 return true;
}
bool PalEngineBridge_TargetInitPacks(void)
{
    full_file=fopen("0:/pal_full.pak","r");
    if(full_file){logical_pack_size=full_file->size;segmented_pack=0;}
    else{
     full_file=fopen("0:/pal_full.00","r");if(!full_file)return false;
     uint8_t header[32];if(fread(header,1,32,full_file)!=32)return false;
     logical_pack_size=(uint32_t)header[24]|((uint32_t)header[25]<<8)|((uint32_t)header[26]<<16)|((uint32_t)header[27]<<24);
     if(logical_pack_size<32||logical_pack_size>99u*PAL_SEGMENT_BYTES)return false;
     uint32_t count=(logical_pack_size+PAL_SEGMENT_BYTES-1)/PAL_SEGMENT_BYTES;
     for(uint32_t i=0;i<count;i++){
      char source[64],mapped[256];segment_path(source,(int)i);if(!path_map(source,mapped,sizeof(mapped)))return false;
      aster_stat_t st={.size=sizeof(st),.version=1};
      uint32_t expected=logical_pack_size-i*PAL_SEGMENT_BYTES;if(expected>PAL_SEGMENT_BYTES)expected=PAL_SEGMENT_BYTES;
      if(aster_fs_stat(mapped,(uint32_t)strlen(mapped),&st,sizeof(st))<0)return false;
      if(st.filesize!=expected)return false;
     }
     segmented_pack=1;current_segment=0;
    }
    PalPackToc toc;
    if(!PalPack_OpenTocRead(&toc,read_at,full_file,logical_pack_size,pal_mem_level2_tf_toc,PAL_MEM_LEVEL2_TF_TOC_BYTES))return false;
    uint32_t size=0;
    if(!PalLevel2ResidentPack_Build(&toc,PAL_ASTER_RESIDENT_MASK,read_at,full_file,
       pal_mem_level2_resident_pack,PAL_MEM_LEVEL2_RESIDENT_PACK_BYTES,&size))return false;
    if(!PalEngineBridge_SetCorePackConst(pal_mem_level2_resident_pack,size)||
       !PalEngineBridge_SetTfPackReadAt(logical_pack_size,read_at,full_file))return false;
    aster_printf("PAL full=%u resident=%u segmented=%d",logical_pack_size,size,segmented_pack);return true;
}
bool PalEngineBridge_LoadDefaultPacks(void){return PalEngineBridge_TargetInitPacks();}
static int draw_controls(void);

ASTER_APP_EXPORT int aster_app_init(void)
{
    aster_display_info_t info={.size=sizeof(info),.version=1};
    int rc=aster_display_get_info(0,&info,sizeof(info));if(rc<0)return rc;
    screen_w=info.app_width;screen_h=info.app_height;
    if(screen_w<320||screen_h<240||screen_w>480||screen_h>480||screen_w<screen_h)return ASTER_ENOTSUP;
    if((uint32_t)screen_w*(uint32_t)screen_h>PAL_MAX_FRAME_PIXELS)return ASTER_ENOTSUP;
    view_h=screen_w*200/320;view_x=0;
    for(int x=0;x<screen_w;x++)source_x[x]=(uint16_t)(x*320/screen_w);
    for(int y=0;y<view_h;y++)source_y[y]=(uint16_t)(y*200/view_h);
    int cell=screen_h>=400?44:34;
    joy_r=cell*3/2;joy_x=joy_r+10;joy_y=screen_h-joy_r-14;
    directions[0]=(DirectionButton){joy_x-cell/2,joy_y-cell*3/2,cell,cell,SDLK_UP,"↑"};
    directions[1]=(DirectionButton){joy_x-cell*3/2,joy_y-cell/2,cell,cell,SDLK_LEFT,"←"};
    directions[2]=(DirectionButton){joy_x+cell/2,joy_y-cell/2,cell,cell,SDLK_RIGHT,"→"};
    directions[3]=(DirectionButton){joy_x-cell/2,joy_y+cell/2,cell,cell,SDLK_DOWN,"↓"};
    action_r=screen_h>=400?30:24;action_x=screen_w-action_r-14;
    back_y=screen_h-action_r-14;confirm_y=back_y-action_r*2-14;
    aster_surface_desc_t desc={.size=sizeof(desc),.version=1,.width=info.app_width,.height=info.app_height,.format=ASTER_PIXEL_RGB565,.flags=0};
    rc=aster_surface_create(&desc,sizeof(desc),&display);if(rc<0)return rc;
    surface_pixels=(aster_surface_buffer_t){.size=sizeof(surface_pixels),.version=1};
    rc=aster_surface_acquire(display,&surface_pixels,sizeof(surface_pixels));if(rc<0)return rc;
    if(!surface_pixels.ptr||surface_pixels.format!=ASTER_PIXEL_RGB565||surface_pixels.pitch<(uint32_t)screen_w*2)return ASTER_ENOTSUP;
    aster_ge_init(&controls_ge,display,info.app_width,info.app_height,control_commands,48,control_text,sizeof(control_text));
    rc=PalAsterUiInit(screen_w==480&&view_h==300);if(rc<0)return rc;
    rc=draw_controls();if(rc<0)return rc;
    async_data[0]=(uint32_t)(uintptr_t)continuation;async_data[1]=async_data[0]+sizeof(continuation);
    aster_printf("PAL Aster bridge initialized %dx%d viewport=%dx%d native_ui=%d",screen_w,screen_h,screen_w,view_h,screen_w==480);return aster_app_set_framerate(30);
}
static int key_code(uint32_t code)
{switch(code){case 103:return SDLK_UP;case 108:return SDLK_DOWN;case 105:return SDLK_LEFT;case 106:return SDLK_RIGHT;case 28:return SDLK_RETURN;case 57:return SDLK_SPACE;case 1:return SDLK_ESCAPE;default:return 0;}}
static void queue_key(int key,int pressed)
{
    if(key&&event_head-event_tail<32){SDL_Event *e=&events[event_head++%32];memset(e,0,sizeof(*e));e->type=pressed?SDL_KEYDOWN:SDL_KEYUP;e->key.keysym.sym=key;}
}
static int in_circle(int x,int y,int cx,int cy,int r)
{int dx=x-cx,dy=y-cy;return dx*dx+dy*dy<=r*r;}
static int direction_hit(int x,int y)
{
    for(int i=0;i<4;i++){const DirectionButton *b=&directions[i];
        if(x>=b->x&&x<b->x+b->w&&y>=b->y&&y<b->y+b->h)return b->key;}
    int dx=x-joy_x,dy=y-joy_y,ax=dx<0?-dx:dx,ay=dy<0?-dy:dy;
    int cell=directions[0].w;
    if(ax<cell/2&&ay<cell/2)return 0;
    if(ax>joy_r+10||ay>joy_r+10)return 0;
    return ax>ay?(dx<0?SDLK_LEFT:SDLK_RIGHT):(dy<0?SDLK_UP:SDLK_DOWN);
}
static int touch_hit(int x,int y,int action)
{
    if(action==ASTER_TOUCH_UP){touch_mode=0;return 0;}
    if(action==ASTER_TOUCH_DOWN)touch_mode=direction_hit(x,y)?1:
        (in_circle(x,y,action_x,confirm_y,action_r)||in_circle(x,y,action_x,back_y,action_r)?2:0);
    if(touch_mode==1)return direction_hit(x,y);
    if(touch_mode==2){
        if(in_circle(x,y,action_x,confirm_y,action_r))return SDLK_RETURN;
        if(in_circle(x,y,action_x,back_y,action_r))return SDLK_ESCAPE;
    }
    return 0;
}
static int control_pressed(int key)
{for(int i=0;i<10;i++)if(fingers[i].active&&fingers[i].key==key)return 1;return feedback_key==key&&(int32_t)(feedback_until-aster_time_now_ms())>0;}
static void touch_event(const aster_event_t *ev)
{
    int slot=-1,empty=-1;
    for(int i=0;i<10;i++){if(fingers[i].active&&fingers[i].slot==ev->u.touch.slot)slot=i;if(!fingers[i].active)empty=i;}
    int x=ev->u.touch.x,y=ev->u.touch.y,action=ev->u.touch.action;
    if(action==ASTER_TOUCH_DOWN){
        if(slot<0)slot=empty;if(slot<0)return;
        int mode=direction_hit(x,y)?1:(in_circle(x,y,action_x,confirm_y,action_r+6)||in_circle(x,y,action_x,back_y,action_r+6)?2:0);
        fingers[slot]=(PalFinger){ev->u.touch.slot,1,mode,0};
    }
    if(slot<0)return;
    PalFinger *f=&fingers[slot];f->key=0;
    if(action==ASTER_TOUCH_UP)f->active=0;
    else if(f->mode==1)f->key=direction_hit(x,y);
    else if(f->mode==2){
        if(in_circle(x,y,action_x,confirm_y,action_r+6))f->key=SDLK_RETURN;
        else if(in_circle(x,y,action_x,back_y,action_r+6))f->key=SDLK_ESCAPE;
    }
    const int keys[]={SDLK_UP,SDLK_LEFT,SDLK_RIGHT,SDLK_DOWN,SDLK_RETURN,SDLK_ESCAPE};
    for(int k=0;k<6;k++){
        int down=0;for(int i=0;i<10;i++)if(fingers[i].active&&fingers[i].key==keys[k])down=1;
        if(down!=held_touch[k]){queue_key(keys[k],down);held_touch[k]=down;aster_printf("PAL input key=%d down=%d",keys[k],down);}
    }
    touch_key=0;for(int i=0;i<6;i++)touch_key|=held_touch[i];
    if(f->key){feedback_key=f->key;feedback_until=aster_time_now_ms()+180;}
    controls_dirty=1;
}
static uint16_t blend565(uint16_t bg,uint16_t fg,int alpha)
{
    int r=(((bg>>11)&31)*(255-alpha)+((fg>>11)&31)*alpha)/255;
    int g=(((bg>>5)&63)*(255-alpha)+((fg>>5)&63)*alpha)/255;
    int b=((bg&31)*(255-alpha)+(fg&31)*alpha)/255;
    return (uint16_t)((r<<11)|(g<<5)|b);
}
static void overlay_circle(int cx,int cy,int r,uint16_t color,int alpha)
{
    for(int y=cy-r;y<=cy+r;y++)for(int x=cx-r;x<=cx+r;x++){
        if(x<0||y<0||x>=screen_w||y>=screen_h)continue;
        int d=(x-cx)*(x-cx)+(y-cy)*(y-cy);
        if(d<=r*r){int border=d>=(r-1)*(r-1);uint16_t *p=&rgb565[y*screen_w+x];
            *p=blend565(*p,border?ASTER_WHITE:color,border?control_border_alpha:alpha);}
    }
}
static void action_label(int cy,const char *label)
{
    int tw=aster_gfx_text_w(16,label,(uint32_t)strlen(label));
    aster_ge_text(&controls_ge,action_x-tw/2,cy+6,16,ASTER_WHITE,ASTER_GFX_NOBG,label);
}
static void direction_arrow(const DirectionButton *b,int alpha)
{
    int cx=b->x+b->w/2,cy=b->y+b->h/2,r=b->w/4,stem=r>=10?2:1;
    for(int dy=-r;dy<=r;dy++)for(int dx=-r;dx<=r;dx++){
        int u=dx,v=dy;
        if(b->key==SDLK_DOWN)v=-dy;
        else if(b->key==SDLK_LEFT){u=dy;v=dx;}
        else if(b->key==SDLK_RIGHT){u=dy;v=-dx;}
        int a=u<0?-u:u;
        if((v<=0&&a<=v+r)||(v>=0&&a<=stem)){
            uint16_t *p=&rgb565[(cy+dy)*screen_w+cx+dx];
            *p=blend565(*p,ASTER_WHITE,alpha);
        }
    }
}
static int draw_controls(void)
{
    for(int x=0;x<screen_w;x++)rgb565[x]=ASTER_RGB565(12,24,38);
    for(int y=1;y<screen_h;y++)memcpy(rgb565+y*screen_w,rgb565,screen_w*2);
    for(int y=0;y<view_h;y++){
        uint16_t *dst=rgb565+y*screen_w;
        if(y&&source_y[y]==source_y[y-1])memcpy(dst,dst-screen_w,screen_w*2);
        else {const uint8_t *src=game_pixels+source_y[y]*320;for(int x=0;x<screen_w;x++)dst[x]=palette565[src[source_x[x]]];}
    }
    PalAsterUiComposite(pal_sram_framebuffer,rgb565,screen_w,palette565);
    uint16_t dark=ASTER_RGB565(12,24,38),active=ASTER_RGB565(40,150,220);
    int big=screen_h>=400,dialog=!big&&PAL_IsInDialog();
    int idle_alpha=big?180:dialog?6:36,pressed_alpha=big?230:dialog?40:110;
    int label_alpha=big?255:dialog?55:150;
    control_border_alpha=big?130:dialog?30:80;
    for(int i=0;i<4;i++){
        const DirectionButton *b=&directions[i];int selected=control_pressed(b->key);
        for(int y=b->y;y<b->y+b->h;y++)for(int x=b->x;x<b->x+b->w;x++){
            int border=x==b->x||x==b->x+b->w-1||y==b->y||y==b->y+b->h-1;
            uint16_t *pixel=&rgb565[y*screen_w+x];
            *pixel=blend565(*pixel,border?ASTER_WHITE:selected?active:dark,border?control_border_alpha:selected?pressed_alpha:idle_alpha);
        }
    }
    overlay_circle(action_x,confirm_y,action_r,control_pressed(SDLK_RETURN)?active:dark,control_pressed(SDLK_RETURN)?pressed_alpha:idle_alpha);
    overlay_circle(action_x,back_y,action_r,control_pressed(SDLK_ESCAPE)?active:dark,control_pressed(SDLK_ESCAPE)?pressed_alpha:idle_alpha);
    for(int i=0;i<4;i++)direction_arrow(&directions[i],label_alpha);
    for(int y=0;y<screen_h;y++)memcpy((uint8_t*)surface_pixels.ptr+y*surface_pixels.pitch,rgb565+y*screen_w,screen_w*2);
    action_label(confirm_y,"确认");action_label(back_y,"返回");
    /* Legacy gfx_exec publishes its dirty region immediately. Include the
     * entire hand-painted game/control frame in that ONE publication; fading
     * text after exec then calling surface_present exposed two brightnesses
     * and mixed old/new game pixels on the S3 panel. Labels stay opaque for
     * readability; button backgrounds and arrows retain their transparency. */
#ifdef PAL_ASTER_TRACE
    if(trace_renders<6)aster_printf("PAL trace render gfx");
#endif
    int rc=aster_ge_exec(&controls_ge,0,0,screen_w,screen_h);if(rc<0)return rc;
#ifdef PAL_ASTER_TRACE
    if(trace_renders++<6)aster_printf("PAL trace render done rc=%d",rc);
#endif
    controls_dirty=0;return rc;
}
ASTER_APP_EXPORT void aster_app_event(const aster_event_t *ev)
{
    if(ended){aster_app_exit(game_code);return;}
    if(ev->type==ASTER_EVENT_KEY){queue_key(key_code(ev->u.key.code),ev->u.key.pressed);return;}
    if(ev->type==ASTER_EVENT_TOUCH){touch_event(ev);return;}
    if(ev->type!=ASTER_EVENT_FRAME)return;
    uint32_t now=aster_time_now_ms();
#ifdef PAL_ASTER_TRACE
    if(trace_frames++<6)aster_printf("PAL trace frame=%u suspended=%d",trace_frames,suspended);
#endif
    int fonts_changed=aster_gfx_fonts_pending();
    int feedback_changed=!touch_key&&feedback_key&&(int32_t)(now-feedback_until)>=0;
    if(feedback_changed)feedback_key=0;
    if(fonts_changed||feedback_changed)controls_dirty=1;
    PalAsterAudioTick();
    if(started&&(int32_t)(aster_time_now_ms()-wake_at)<0){if(controls_dirty)draw_controls();return;}
    uint32_t event_stack=pal_stack_get();
    if(suspended){memcpy((uint8_t*)&__heap_base-sizeof(saved_c_stack),saved_c_stack,sizeof(saved_c_stack));pal_stack_set(suspended_stack);pal_async_start_rewind((uint32_t)(uintptr_t)async_data);}
    started=1;
    char name[]="sdlpal-aster";char *args[]={name,NULL};
    (void)PAL_EngineMain(1,args);
#ifdef PAL_ASTER_TRACE
    if(trace_frames<=6)aster_printf("PAL trace engine returned state=%d",pal_async_get_state());
#endif
    if(pal_async_get_state()==1){suspended_stack=pal_stack_get();pal_async_stop_unwind();memcpy(saved_c_stack,(uint8_t*)&__heap_base-sizeof(saved_c_stack),sizeof(saved_c_stack));pal_stack_set(event_stack);suspended=1;}
    else{suspended=0;ended=1;}
    if(controls_dirty)draw_controls();
    uint32_t elapsed=aster_time_now_ms()-now;
    if(elapsed>callback_peak)callback_peak=elapsed;
    if((uint32_t)(aster_time_now_ms()-perf_since)>=5000){
        uint32_t span=aster_time_now_ms()-perf_since;
        aster_printf("PAL perf: presents=%u interval_ms=%u callback_peak_ms=%u",presents,span,callback_peak);
        perf_since=aster_time_now_ms();presents=0;callback_peak=0;
    }
}
ASTER_APP_EXPORT void aster_app_pause(void){const int keys[]={SDLK_UP,SDLK_LEFT,SDLK_RIGHT,SDLK_DOWN,SDLK_RETURN,SDLK_ESCAPE};for(int i=0;i<6;i++)if(held_touch[i])queue_key(keys[i],0);memset(held_touch,0,sizeof(held_touch));memset(fingers,0,sizeof(fingers));touch_key=0;feedback_key=0;touch_mode=0;controls_dirty=1;PalAsterAudioPause();}
ASTER_APP_EXPORT void aster_app_resume(void){wake_at=aster_time_now_ms();PalAsterAudioResume();}
ASTER_APP_EXPORT void aster_app_stop(void){AUDIO_CloseDevice();if(display){aster_surface_destroy(display);display=0;}for(int i=0;i<12;i++)if(files[i].used)fclose(&files[i]);}
ASTER_APP_EXPORT void aster_app_destroy(void){aster_app_stop();}
void PAL_AsterQuit(int code){aster_errfmt("PAL quit code=%d",code);game_code=code;ended=1;for(;;)yield(1000);}
