#include <aster_imports.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include "game.h"
#include "audio_meta.h"
static unsigned W=320,H=240,GAME_H=240;
static float SCALE=1.f,resourceScale=1.f;static int game_left,game_top;
static int mapx(float x,int unused){(void)unused;return (int)floorf(game_left+x*SCALE);}
static int mapy(float y,int unused){(void)unused;return (int)floorf(game_top+y*SCALE);}
#define RECORDS 256
#ifndef GOLDMINER_CACHE_BYTES
#define GOLDMINER_CACHE_BYTES 786432u
#endif
#define CACHE_LIMIT GOLDMINER_CACHE_BYTES
typedef struct {char name[64];uint32_t offset,size;uint16_t w,h;uint32_t kind;} Record;
typedef struct {uint8_t *data;uint32_t age;} Cache;
_Static_assert(sizeof(Record)==80,"Asset index");
static Record rec[RECORDS];static Cache cache[RECORDS];static unsigned count,cacheBytes,serial;
static aster_handle_t pack,surface;static uint16_t *fb;static unsigned pitch;static int stopped;
static uint32_t lastMs,clockMs,frames,perfStart,workMs;
static int tap_active,tap_cancel,tap_x,tap_y;static uint32_t tap_slot;
static uint32_t readBytes,readMs,cacheMisses,drawMs,logicMs,presentMs,samples;
extern uint32_t omf_heap_used(void),omf_heap_peak(void);
static int read_at(uint32_t off,void *dst,uint32_t n){uint32_t start=aster_time_now_ms();if(aster_fs_seek(pack,off,ASTER_FS_SEEK_SET)<0)return 0;uint8_t *p=dst;while(n){int r=aster_fs_read(pack,p,n>16384?16384:n);if(r<=0)return 0;p+=r;n-=r;readBytes+=r;}readMs+=aster_time_now_ms()-start;return 1;}
static int find(const char *s){for(unsigned i=0;i<count;i++)if(!strcmp(rec[i].name,s))return i;return -1;}
int omf_renderer_drop_cache(void){int oldest=-1;for(unsigned i=0;i<count;i++)if(cache[i].data&&(oldest<0||cache[i].age<cache[oldest].age))oldest=i;if(oldest<0)return 0;uint8_t *p=cache[oldest].data;cache[oldest].data=NULL;cacheBytes-=rec[oldest].size;free(p);return 1;}
static uint8_t *get(int id){if(id<0||(unsigned)id>=count)return NULL;if(!cache[id].data){cacheMisses++;while(cacheBytes+rec[id].size>CACHE_LIMIT&&omf_renderer_drop_cache()){}uint8_t *p=malloc(rec[id].size);if(!p||!read_at(rec[id].offset,p,rec[id].size)){free(p);return NULL;}cache[id].data=p;cacheBytes+=rec[id].size;}cache[id].age=++serial;return cache[id].data;}
static uint16_t mix(uint16_t d,uint16_t s,unsigned a){if(a>=255)return s;if(!a)return d;unsigned r=(((s>>11)&31)*a+((d>>11)&31)*(255-a)+127)/255,g=(((s>>5)&63)*a+((d>>5)&63)*(255-a)+127)/255,b=((s&31)*a+(d&31)*(255-a)+127)/255;return (r<<11)|(g<<5)|b;}
static void pixel(int x,int y,uint16_t c,unsigned a){if((unsigned)x<W&&y>=game_top&&(unsigned)y<GAME_H)fb[y*pitch+x]=mix(fb[y*pitch+x],c,a);}
void gm_rect(int x,int y,int w,int h,uint16_t c,unsigned a){int f=y>=240,x0=mapx(x,f),x1=mapx(x+w,f),y0=mapy(y,f),y1=mapy(y+h,f);if(x0<0)x0=0;if(y0<game_top)y0=game_top;if(x1>(int)W)x1=W;if(y1>(int)GAME_H)y1=GAME_H;if(x1<=x0||y1<=y0)return;
 if(a>=255){uint32_t pair=c|((uint32_t)c<<16);for(int yy=y0;yy<y1;yy++){uint16_t *row=fb+yy*pitch;int xx=x0;if((uintptr_t)(row+xx)&3)row[xx++]=c;for(;xx+1<x1;xx+=2)*(uint32_t*)(row+xx)=pair;if(xx<x1)row[xx]=c;}return;}
 for(int yy=y0;yy<y1;yy++)for(int xx=x0;xx<x1;xx++)pixel(xx,yy,c,a);}
void gm_line(int x,int y,int x2,int y2,uint16_t c,unsigned a){int f=y>=240;x=mapx(x,f);x2=mapx(x2,f);y=mapy(y,f);y2=mapy(y2,f);int dx=abs(x2-x),sx=x<x2?1:-1,dy=-abs(y2-y),sy=y<y2?1:-1,e=dx+dy;for(unsigned n=0;n<2048;n++){pixel(x,y,c,a);if(x==x2&&y==y2)break;int z=2*e;if(z>=dy){e+=dy;x+=sx;}if(z<=dx){e+=dx;y+=sy;}}}
static int meta(const char *name){char s[80];snprintf(s,sizeof(s),"image/%s",name);return find(s);}
int gm_width(const char *name){int i=meta(name);return i>=0?rec[i].w:0;}
int gm_height(const char *name){int i=meta(name);return i>=0?rec[i].h:0;}
void gm_blit(const char *name,int frame,float x,float y,float angle,float sx,float sy,float ox,float oy){
 char key[80];snprintf(key,sizeof(key),"pixel/%s/%d",name,frame);int id=find(key);if(id<0){aster_printf("Missing sprite %s",key);return;}
 x=game_left+x*SCALE;y=game_top+y*SCALE;ox*=resourceScale;oy*=resourceScale;
 sx*=SCALE/resourceScale;sy*=SCALE/resourceScale;
 uint8_t *data=get(id);if(!data)return;unsigned w=rec[id].w,h=rec[id].h,step=rec[id].kind==0?2:3;
 if(angle==0&&sx==1&&sy==1){
  int dx=floorf(x-ox),dy=floorf(y-oy),left=dx<0?-dx:0,right=w;if(dx+right>W)right=W-dx;
  if(right<=left)return;
  for(unsigned yy=0;yy<h;yy++)if(dy+(int)yy>=game_top&&(unsigned)(dy+(int)yy)<GAME_H){
   if(step==2)memcpy(fb+(dy+yy)*pitch+dx+left,data+(yy*w+left)*2,(right-left)*2);
   else for(int xx=left;xx<right;xx++){const uint8_t *p=data+(yy*w+xx)*3;if(p[2])pixel(dx+xx,dy+yy,p[0]|(p[1]<<8),p[2]);}
  }
  return;
 }
 if(angle==0&&sx>0&&sy>0){
  int dx=floorf(x-ox*sx),dy=floorf(y-oy*sy);
  int x0=fmaxf(0,dx),x1=fminf(W,ceilf(dx+w*sx));
  int y0=fmaxf(game_top,dy),y1=fminf(GAME_H,ceilf(dy+h*sy));
  if(x1<=x0||y1<=y0)return;
  unsigned columns[480];
  for(int xx=x0;xx<x1;xx++){unsigned u=(xx-dx)/sx;columns[xx]=u<w?u:w-1;}
  unsigned previous=~0u;
  for(int yy=y0;yy<y1;yy++){unsigned v=(yy-dy)/sy;if(v>=h)v=h-1;
   if(step==2){uint16_t *dst=fb+yy*pitch;const uint16_t *src=(const uint16_t*)data+v*w;
    if(v==previous)memcpy(dst+x0,dst-pitch+x0,(x1-x0)*2);else for(int xx=x0;xx<x1;xx++)dst[xx]=src[columns[xx]];
   }else for(int xx=x0;xx<x1;xx++){const uint8_t *p=data+(v*w+columns[xx])*3;if(p[2])pixel(xx,yy,p[0]|(p[1]<<8),p[2]);}
   previous=v;}
  return;
 }
 if(sx==0||sy==0)return;float cs=cosf(angle),sn=sinf(angle),minx=1e9f,maxx=-1e9f,miny=1e9f,maxy=-1e9f;
 for(int cy=0;cy<2;cy++)for(int cx=0;cx<2;cx++){float px=((cx?w:0)-ox)*sx,py=((cy?h:0)-oy)*sy,xx=x+cs*px-sn*py,yy=y+sn*px+cs*py;minx=fminf(minx,xx);maxx=fmaxf(maxx,xx);miny=fminf(miny,yy);maxy=fmaxf(maxy,yy);}
 int x0=fmaxf(0,floorf(minx)),x1=fminf(W,ceilf(maxx)),y0=fmaxf(game_top,floorf(miny)),y1=fminf(GAME_H,ceilf(maxy));
 for(int yy=y0;yy<y1;yy++)for(int xx=x0;xx<x1;xx++){float dx=xx+.5f-x,dy=yy+.5f-y;int u=floorf((cs*dx+sn*dy)/sx+ox),v=floorf((-sn*dx+cs*dy)/sy+oy);if((unsigned)u>=w||(unsigned)v>=h)continue;const uint8_t *p=data+(v*w+u)*step;pixel(xx,yy,p[0]|(p[1]<<8),step==2?255:p[2]);}
}
typedef struct {int16_t x,y,w,h;uint16_t advance,reserved;uint32_t offset;} Glyph;
_Static_assert(sizeof(Glyph)==16,"Glyph table");
void gm_text(int font,const char *s,float x,float y,uint16_t color){
 char name[20];snprintf(name,sizeof(name),"font/%d",font);int id=find(name);uint8_t *p=get(id);if(!p)return;
 int footer=y>=240;x=mapx(x,footer);y=mapy(y,footer);unsigned line=p[0]|(p[1]<<8);const Glyph *glyph=(const Glyph*)(p+4);const uint8_t *bits=p+4+95*sizeof(Glyph);float origin=x;
 float fontscale=SCALE/resourceScale;
 for(const unsigned char *ch=(const unsigned char*)s;*ch;ch++){if(*ch=='\n'){x=origin;y+=line*fontscale;continue;}if(*ch<32||*ch>126)continue;const Glyph *g=&glyph[*ch-32];int gh=ceilf(g->h*fontscale),gw=ceilf(g->w*fontscale);int dx=floorf(x+g->x*fontscale),dy=floorf(y+g->y*fontscale);unsigned columns[128];if(gw>128)continue;for(int xx=0;xx<gw;xx++){unsigned u=xx/fontscale;columns[xx]=u<(unsigned)g->w?u:g->w-1;}for(int yy=0;yy<gh;yy++){unsigned v=yy/fontscale;if(v>=(unsigned)g->h)v=g->h-1;const uint8_t *row=bits+g->offset+v*g->w;for(int xx=0;xx<gw;xx++){unsigned a=row[columns[xx]];if(a)pixel(dx+xx,dy+yy,color,a);}}x+=g->advance/64.f*fontscale;}
}
static const char *soundName[]={"money.wav","hook_reset.wav","grab_start.wav","grab_back.wav","explosive.wav","high_value.wav","normal_value.wav","low_value.wav","goal.mp3","made_goal.mp3"};
static int handles[10],bg=-1;static uint32_t until[10],used[10],audioSerial;
int gm_audio_playing(int id){
 if(id<0||id>=10)return 0;
 if(id>=8&&clockMs>=until[id]+100)return 0;
 if(id>=8&&bg==id){aster_audio_state_t s={.size=sizeof(s),.version=2};if(aster_audio_get_state(&s,sizeof(s))>=0)return s.state!=ASTER_AUDIO_IDLE;}
 return clockMs<until[id];
}
void gm_audio_play(int id){
 if(id<0||id>=10||gm_audio_playing(id))return;char path[100];snprintf(path,sizeof(path),"/assets/assets/audio/%s",soundName[id]);
 if(id>=8){int rc=aster_audio_start_file(path,strlen(path));until[id]=clockMs+(rc>=0?sound_ms[id]:0);bg=id;return;}
 if(handles[id]<=0){
  int live=0;for(int i=0;i<8;i++)if(handles[i]>0)live++;
  if(live>=4){int v=-1;for(int i=0;i<8;i++)if(handles[i]>0&&(v<0||(until[i]<=clockMs&&until[v]>clockMs)||((until[i]<=clockMs)==(until[v]<=clockMs)&&used[i]<used[v])))v=i;if(v>=0){aster_audio_effect_unload(handles[v]);handles[v]=0;until[v]=0;}}
  handles[id]=aster_audio_effect_load(path,strlen(path));
 }
 if(handles[id]>0){aster_audio_effect_play(handles[id]);until[id]=clockMs+sound_ms[id];used[id]=++audioSerial;}else aster_printf("GoldMiner effect error %d id=%d",handles[id],id);
}
void gm_audio_stop(int id){if(id<0||id>=10)return;if(id>=8&&bg==id){aster_audio_stop();bg=-1;}if(handles[id]>0)aster_audio_effect_stop(handles[id],0);until[id]=0;}
void gm_audio_pause(int pause){if(pause)aster_audio_pause();else aster_audio_resume();}
void gm_save(int score,int level){uint32_t data[]={0x314d5347,score,level};aster_handle_t f;const char path[]="/data/savedata.bin";if(aster_fs_open(path,sizeof(path)-1,ASTER_FS_O_WRONLY|ASTER_FS_O_CREAT|ASTER_FS_O_TRUNC,&f)>=0){int rc=aster_fs_write(f,data,sizeof(data));aster_fs_close(f);if(rc!=sizeof(data))aster_printf("GoldMiner save failed=%d",rc);}}
void gm_load(int *score,int *level){uint32_t data[3];aster_handle_t f;const char path[]="/data/savedata.bin";if(aster_fs_open(path,sizeof(path)-1,ASTER_FS_O_RDONLY,&f)>=0){int n=aster_fs_read(f,data,sizeof(data));aster_fs_close(f);if(n==sizeof(data)&&data[0]==0x314d5347&&data[1]<1000000000u&&data[2]<1000000u){*score=data[1];*level=data[2];}}}
static void controls(void){
 gm_rect(266,2,52,24,0x0841,72);
 gm_line(266,2,317,2,0xffff,110);gm_line(266,25,317,25,0xffff,110);
 gm_text(1,"MENU",272,6,0xffff);
}
static void release(void){if(stopped)return;stopped=1;aster_audio_stop();for(int i=0;i<8;i++)if(handles[i]>0){aster_audio_effect_unload(handles[i]);handles[i]=0;}while(omf_renderer_drop_cache()){}if(pack){aster_fs_close(pack);pack=0;}if(surface){aster_surface_destroy(surface);surface=0;}}
ASTER_APP_EXPORT int aster_app_init(void){
 aster_display_info_t info={.size=sizeof(info),.version=1};int rc=aster_display_get_info(0,&info,sizeof(info));if(rc<0)return rc;
 unsigned width=info.app_width,height=info.app_height;if(width<height){unsigned swap=width;width=height;height=swap;}
 aster_surface_desc_t d={.size=sizeof(d),.version=1,.width=width,.height=height,.format=ASTER_PIXEL_RGB565};rc=aster_surface_create(&d,sizeof(d),&surface);if(rc<0)return rc;
 aster_surface_buffer_t screen={.size=sizeof(screen),.version=1};
 if(aster_surface_acquire(surface,&screen,sizeof(screen))<0||screen.width>480||screen.height>480||screen.width<320||screen.height<240){release();return ASTER_ENOTSUP;}
 W=screen.width;H=screen.height;SCALE=fminf(W/320.f,H/240.f);unsigned view_h=(unsigned)(240*SCALE);game_top=(H-view_h)/2;GAME_H=game_top+view_h;game_left=(W-(unsigned)(320*SCALE))/2;
 const char file[]="/assets/assets/game.gma";if(aster_fs_open(file,sizeof(file)-1,ASTER_FS_O_RDONLY,&pack)<0){release();return ASTER_ENOENT;}
 uint32_t head[4]={0};if(!read_at(0,head,8)||(head[0]!=0x31414d47&&head[0]!=0x32414d47)||head[1]>RECORDS){release();return ASTER_ECORRUPT;}unsigned index=8;resourceScale=1.2f;if(head[0]==0x32414d47){if(!read_at(8,head+2,8)||head[2]<32768||head[2]>131072||head[3]){release();return ASTER_ECORRUPT;}resourceScale=head[2]/65536.f;index=16;}count=head[1];if(!read_at(index,rec,count*sizeof(Record))){release();return ASTER_EIO;}
 gm_init();lastMs=perfStart=aster_time_now_ms();aster_app_set_framerate(30);aster_printf("GoldMiner native C init assets=%u heap=%u",count,omf_heap_used());return ASTER_OK;
}
ASTER_APP_EXPORT void aster_app_event(const aster_event_t *e){
 if(!e||stopped)return;
 if(e->type==ASTER_EVENT_FRAME){uint32_t now=aster_time_now_ms(),dt=now-lastMs;lastMs=now;if(dt>250)dt=250;if(!gm_paused)clockMs+=dt;
  gm_update(dt/1000.f);uint32_t updated=aster_time_now_ms();logicMs+=updated-now;aster_surface_buffer_t b={.size=sizeof(b),.version=1};if(aster_surface_acquire(surface,&b,sizeof(b))<0||!b.ptr)return;fb=b.ptr;pitch=b.pitch/2;
  for(unsigned y=0;y<H;y++)memset(fb+y*pitch,0,W*2);gm_render();controls();uint32_t drawn=aster_time_now_ms();drawMs+=drawn-updated;aster_surface_present(surface);uint32_t shown=aster_time_now_ms();presentMs+=shown-drawn;workMs+=shown-now;
  frames++;samples++;if(frames==30||samples==120){uint32_t elapsed=aster_time_now_ms()-perfStart;aster_printf("GoldMiner PERF frames=%u elapsed_ms=%u work_ms=%u heap=%u peak=%u cache=%u state=%s",samples,elapsed,workMs,omf_heap_used(),omf_heap_peak(),cacheBytes,gm_state_name());aster_printf("GoldMiner PHASE logic=%u draw=%u present=%u read_ms=%u read_bytes=%u misses=%u",logicMs,drawMs,presentMs,readMs,readBytes,cacheMisses);perfStart=aster_time_now_ms();workMs=logicMs=drawMs=presentMs=readMs=readBytes=cacheMisses=samples=0;}
 }else if(e->type==ASTER_EVENT_TOUCH){
 int x=e->u.touch.x,y=e->u.touch.y,action=e->u.touch.action;
 if(action==ASTER_TOUCH_DOWN){if(tap_active)tap_cancel=1;else{tap_active=1;tap_cancel=0;tap_slot=e->u.touch.slot;tap_x=x;tap_y=y;}}
 if(tap_active&&e->u.touch.slot==tap_slot){int dx=x-tap_x,dy=y-tap_y;if(dx*dx+dy*dy>196)tap_cancel=1;
  if(action==ASTER_TOUCH_UP){if(!tap_cancel&&x>=game_left&&x<game_left+(int)(320*SCALE)&&y>=game_top&&y<(int)GAME_H)gm_touch((x-game_left)/SCALE,(y-game_top)/SCALE);tap_active=0;}}
 }
 else if(e->type==ASTER_EVENT_KEY&&e->u.key.pressed){unsigned c=e->u.key.code;if(c==1||c==25)gm_key("pause");else if(c==28)gm_key("return");else if(c==57)gm_key("space");else if(c==103||c==17)gm_key("up");else if(c==108||c==31)gm_key("down");else if(c==105||c==30)gm_key("left");else if(c==106||c==32)gm_key("right");else if(c==44||c==36)gm_key("return");else if(c==45||c==37||c==22||c==23)gm_key("up");}
 if(gm_exit)aster_app_exit(0);
}
ASTER_APP_EXPORT void aster_app_pause(void){tap_active=0;gm_audio_pause(1);}
ASTER_APP_EXPORT void aster_app_resume(void){lastMs=aster_time_now_ms();gm_audio_pause(gm_paused);}
ASTER_APP_EXPORT void aster_app_stop(void){release();}
ASTER_APP_EXPORT void aster_app_destroy(void){release();}
