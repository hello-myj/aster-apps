#include "omf_surface.h"
/* Software implementation of upstream shaders/palette.frag and rgba.frag.
 * Uses the upstream renderer vtable; combat and all scene rendering stay upstream. */
#include <aster_imports.h>
#include "video/renderers/renderer.h"
#include "video/video.h"
#include "video/vga_state.h"
#include "video/enums.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
/* Vanilla VGA uses 8-bit indices. Match normalized shader saturation,
 * retaining all four channels in one 32-bit pixel instead of 64 bits. */
_Static_assert(VGA_PALETTE_SIZE == 256, "Compact framebuffer requires vanilla VGA");
typedef union {struct {uint8_t index,remap,tint,add;};uint32_t word;} Pixel;
_Static_assert(sizeof(Pixel)==4 && _Alignof(Pixel)==4,"Framebuffer pixels require aligned word access");
void omf_controls_overlay(uint16_t *out,unsigned pitch);
static uint16_t colors[VGA_PALETTE_SIZE];
static Pixel frame[320*200];static vga_palette pal;static vga_remap_tables maps;
/* One immutable full background expanded to the exact four-channel form.
 * Palette changes stay live in finish(); camera shake uses the normal path. */
static uint32_t *backgroundCache;
static const surface *backgroundKey;
static unsigned backgroundGuid,backgroundHits,backgroundBuilds;
static int firstDraw;
int omf_renderer_drop_cache(void){uint32_t *p=backgroundCache;if(!p)return 0;backgroundCache=NULL;backgroundKey=NULL;free(p);return 1;}
void omf_renderer_stats(void){aster_printf("OMF BG cache_bytes=%u hits=%u builds=%u",backgroundCache?(unsigned)sizeof(frame):0,backgroundHits,backgroundBuilds);backgroundHits=backgroundBuilds=0;}
static aster_handle_t screen=ASTER_HANDLE_INVALID;static int targetX,targetY;static unsigned frameOptions;static SDL_Rect captureArea;
static int bound(int x,int a,int b){return x<a?a:x>b?b:x;}
static bool available(void){return true;}static const char *name(void){return "AsterSoftware";}static const char *description(void){return "Aster paletted software renderer";}
static void create(renderer *r){r->ctx=frame;}
static void destroy(renderer *r){}
static bool setup(void *ctx,int w,int h,bool fs,bool vs,int aspect,int limit,int scale,int scaling){if(screen!=ASTER_HANDLE_INVALID)return true;aster_surface_desc_t d={.size=sizeof(d),.version=1,.width=480,.height=320,.format=ASTER_PIXEL_RGB565};return aster_surface_create(&d,sizeof(d),&screen)>=0;}
static void state(void *ctx,int *w,int *h,bool *fs,bool *vs,int *aspect,int *scale){if(w)*w=480;if(h)*h=320;if(fs)*fs=false;if(vs)*vs=false;if(aspect)*aspect=0;if(scale)*scale=1;}
static void reset(void *ctx){omf_renderer_drop_cache();}
static void close_context(void *ctx){omf_renderer_drop_cache();if(screen!=ASTER_HANDLE_INVALID){aster_surface_destroy(screen);screen=ASTER_HANDLE_INVALID;}}
static void move(void *ctx,int x,int y){targetX=x;targetY=y;}
static void draw(void *ctx,const surface *s,SDL_Rect *r,int ro,int rounds,int po,int pl,int opacity,unsigned flip,unsigned opt){
 int first=firstDraw;firstDraw=0;
 const vga_pixel *pixels=omf_surface_pixels(s);
 if(!s||!pixels||r->w<=0||r->h<=0)return;
 int x0=bound(r->x+targetX,0,320),x1=bound(r->x+targetX+r->w,0,320),y0=bound(r->y+targetY,0,200),y1=bound(r->y+targetY+r->h,0,200);
 if(first&&!opt&&!rounds&&!po&&opacity==255&&!flip&&(unsigned)s->transparent>255u&&s->w==320&&s->h==200&&r->w==320&&r->h==200&&r->x+targetX==0&&r->y+targetY==0){
  if(!backgroundCache||backgroundKey!=s||backgroundGuid!=s->guid){
   omf_renderer_drop_cache();backgroundCache=malloc(sizeof(frame));
   if(backgroundCache){for(unsigned i=0;i<320u*200u;i++)backgroundCache[i]=pixels[i];backgroundKey=s;backgroundGuid=s->guid;backgroundBuilds++;}
  }
  if(backgroundCache){memcpy(frame,backgroundCache,sizeof(frame));backgroundHits++;return;}
 }
 if(!opt&&!rounds&&!po&&opacity==255&&!flip&&r->w==s->w&&r->h==s->h){for(int y=y0;y<y1;y++){const vga_pixel *src=pixels+(y-r->y-targetY)*s->w;for(int x=x0;x<x1;x++){int v=src[x-r->x-targetX];if(v!=s->transparent)frame[y*320+x].word=(unsigned)v;}}return;}
 /* Geometry is constant across rows. Avoid a division for every pixel. */
 int sxMap[320];for(int x=x0;x<x1;x++){int sx=r->w==s->w?x-r->x-targetX:(x-r->x-targetX)*s->w/r->w;if(flip&FLIP_HORIZONTAL)sx=s->w-1-sx;sxMap[x]=sx;}
 unsigned char translated[256];int useLookup=(po||(opt&SPRITE_REMAP))&&(x1-x0)*(y1-y0)>=512;
 if(useLookup)for(int v=0;v<256;v++){int index=v;if(index<=pl)index=bound(index+po,0,pl);if((opt&SPRITE_REMAP)&&!((opt&SPRITE_HAR_QUIRKS)&&index>0x30))index=maps.tables[bound(ro,0,18)].data[bound(index,0,255)];translated[v]=index;}
 for(int y=y0;y<y1;y++){int sy=r->h==s->h?y-r->y-targetY:(y-r->y-targetY)*s->h/r->h;if(flip&FLIP_VERTICAL)sy=s->h-1-sy;float noise=opacity<255?tanf(10*1.61803398875f*(200-y)):0;
  for(int x=x0;x<x1;x++){if(opacity<255){float v=noise+107.0f*(x+.5f)/256;v-=floorf(v);if(v>opacity/255.0f)continue;}int sx=sxMap[x];
   int index=pixels[sy*s->w+sx];if(opt&SPRITE_SHADOW){int coverage=0;for(int dy=-1;dy<3;dy++){int yy=sy+dy;if(yy>=0&&yy<s->h&&pixels[yy*s->w+sx]!=s->transparent)coverage++;}if(!coverage)continue;index=coverage;}else if(index==s->transparent)continue;
   if(useLookup)index=translated[index];else{if(index<=pl)index=bound(index+po,0,pl);if((opt&SPRITE_REMAP)&&!((opt&SPRITE_HAR_QUIRKS)&&index>0x30))index=maps.tables[bound(ro,0,18)].data[bound(index,0,VGA_PALETTE_SIZE-1)];}
   Pixel *p=&frame[y*320+x];if(opt&SPRITE_DARK_TINT){p->remap=ro+12*19;p->tint=index;p->add=0;}else if(rounds>0){int encoded=ro+rounds*19+index;if((opt&SPRITE_SHADOW)&&encoded<p->remap)encoded=p->remap;p->remap=bound(encoded,0,255);}else if(opt&SPRITE_INDEX_ADD)p->add=bound(index*60,0,255);else *p=(Pixel){.index=index};
  }
 }
}
static void sync_palette(void);
static void prepare(void *ctx,unsigned options){sync_palette();memset(frame,0,sizeof(frame));frameOptions=options;targetX=targetY=0;firstDraw=1;}
static void sync_palette(void){vga_palette *p=NULL;vga_remap_tables *r=NULL;if(vga_state_is_palette_dirty(&p,NULL,NULL)){pal=*p;for(int i=0;i<VGA_PALETTE_SIZE;i++){vga_color c=pal.colors[i];colors[i]=((c.r>>3)<<11)|((c.g>>2)<<5)|(c.b>>3);}vga_state_mark_palette_flushed();}if(vga_state_is_remap_dirty(&r)){maps=*r;vga_state_mark_remaps_flushed();}}
static inline __attribute__((always_inline)) uint16_t color(Pixel p){if(!(p.word>>8))return colors[p.word&255];int idx=p.index+p.add,rounds=p.remap/19,row=p.remap%19;if(!(frameOptions&FBUFOPT_CREDITS)){if(p.tint>0&&rounds!=12)idx=p.tint;else if(p.tint>=0x60){idx=p.tint;rounds=0;}else if(p.tint>0){int behind=1+bound(maps.tables[4].data[bound(idx,0,VGA_PALETTE_SIZE-1)]-0xa8,0,7)*2;idx=(p.tint&0xf0)+((p.tint&15)*3+behind*2)/5;}for(int i=0;i<rounds;i++)idx=maps.tables[row].data[bound(idx,0,VGA_PALETTE_SIZE-1)];}return colors[bound(idx,0,VGA_PALETTE_SIZE-1)];}
static unsigned pf_count,pf_convert,pf_overlay,pf_present;
static void finish(void *ctx){uint32_t stamp=aster_time_now_ms();sync_palette();aster_surface_buffer_t b={.size=sizeof(b),.version=1};if(aster_surface_acquire(screen,&b,sizeof(b))<0)return;uint16_t *dst=b.ptr;unsigned pitch=b.pitch/2;/* Only the letterbox bars need clearing. Write adjacent RGB565 pixels as
 * aligned 32-bit words: 16-bit PSRAM writes cost much more on ESP32-S3. */
memset(dst,0,b.pitch*10);memset(dst+310*pitch,0,b.pitch*10);
for(int y=0;y<300;y++){
 uint32_t *row=(uint32_t *)(dst+(y+10)*pitch);
 if(y&&y*2/3==(y-1)*2/3){const uint32_t *prev=(const uint32_t *)(dst+(y+9)*pitch);for(unsigned k=0;k<240;k++)row[k]=prev[k];continue;}
 Pixel *src=&frame[(y*2/3)*320];
 for(int x=0;x<320;x+=4){uint32_t a=color(src[x]),b=color(src[x+1]),c=color(src[x+2]),d=color(src[x+3]);int dx=x*3/4;row[dx]=a|(a<<16);row[dx+1]=b|(c<<16);row[dx+2]=c|(d<<16);}
}
uint32_t t=aster_time_now_ms();pf_convert+=t-stamp;omf_controls_overlay(dst,pitch);uint32_t u=aster_time_now_ms();pf_overlay+=u-t;aster_surface_present(screen);pf_present+=aster_time_now_ms()-u;if(++pf_count==120){aster_printf("OMF RENDER n=120 convert_ms=%u overlay_ms=%u present_ms=%u",pf_convert,pf_overlay,pf_present);pf_count=pf_convert=pf_overlay=pf_present=0;}}
static void area_begin(void *ctx,const SDL_Rect *r){captureArea=*r;prepare(ctx,0);}
static void area_end(void *ctx,surface *dst){int x=bound(captureArea.x,0,320),y=bound(captureArea.y,0,200),w=bound(captureArea.w,0,320-x),h=bound(captureArea.h,0,200-y);surface_create(dst,w,h);for(int yy=0;yy<h;yy++)for(int xx=0;xx<w;xx++)dst->data[yy*w+xx]=frame[(y+yy)*320+x+xx].index;}
static void capture(void *ctx,video_screenshot_signal cb){}static void atlas(void *ctx,bool v){}static void scene_change(void *ctx){omf_renderer_drop_cache();}
/* Upstream video.c registers this callback name; the supplied renderer is software. */
void gl3_renderer_set_callbacks(renderer *r){*r=(renderer){.is_available=available,.get_description=description,.get_name=name,.create=create,.destroy=destroy,.setup_context=setup,.get_context_state=state,.reset_context_with=setup,.reset_context=reset,.close_context=close_context,.draw_surface=draw,.move_target=move,.render_prepare=prepare,.render_finish=finish,.render_area_prepare=area_begin,.render_area_finish=area_end,.capture_screen=capture,.signal_scene_change=scene_change,.signal_draw_atlas=atlas};}
