/* P4 presentation only. Legacy surfaces, game coordinates and save data remain
 * untouched. A bounded list of clipped glyph/panel records follows indexed
 * screen backups; scene pixels never contain a resampled text glyph. */
#include "pal_libc.h"
#include "pal_aster_ui.h"
#include <aster_imports.h>

#if PAL_ASTER_NATIVE_UI
#define UI_RECORDS 4096
#define UI_COPY_RECORDS 2048
#define UI_GLYPHS 3000
typedef struct {int16_t x0,y0,x1,y1;} Clip;
typedef struct {
 const void *owner;
 Clip clip;
 int16_t x,y,w,h;
 uint16_t glyph;
 uint8_t color,shadow,kind,border;
} UiRecord;
/* Named, fixed owners. No game allocator, runtime font rasterizer or cache. */
static UiRecord items[UI_RECORDS],clear_work[UI_RECORDS],copy_work[UI_COPY_RECORDS];
static uint8_t font[UI_GLYPHS*164];
static unsigned count,glyphs;
static int enabled;
static uint16_t le16(const uint8_t *p){return p[0]|((uint16_t)p[1]<<8);}
static void overflow(void){aster_errfmt("PAL native UI record capacity exceeded");aster_app_exit(1);enabled=0;}
static int intersect(Clip a,Clip b,Clip *out)
{
 out->x0=a.x0>b.x0?a.x0:b.x0;out->y0=a.y0>b.y0?a.y0:b.y0;
 out->x1=a.x1<b.x1?a.x1:b.x1;out->y1=a.y1<b.y1?a.y1:b.y1;
 return out->x0<out->x1&&out->y0<out->y1;
}
static Clip rect(int x,int y,int w,int h){return (Clip){x,y,x+w,y+h};}
static int clipped(UiRecord *r,Clip box){Clip c;if(!intersect(r->clip,box,&c))return 0;r->clip=c;return 1;}
static void append(UiRecord r){if(count>=UI_RECORDS){overflow();return;}items[count++]=r;}
int PalAsterUiInit(int use)
{
 enabled=0;count=0;if(!use)return 0;
 FILE *file=fopen("ui18.bin","rb");if(!file)return ASTER_ENOENT;
 uint8_t header[16];
 if(fread(header,1,16,file)!=16||memcmp(header,"PALUI18\0",8)||le16(header+8)!=1||le16(header+12)!=162){fclose(file);return ASTER_EINVAL;}
 glyphs=le16(header+10);
 if(!glyphs||glyphs>UI_GLYPHS||fread(font,164,glyphs,file)!=glyphs){fclose(file);return ASTER_EINVAL;}
 fclose(file);
 for(unsigned i=1;i<glyphs;i++)if(le16(font+i*2)<=le16(font+(i-1)*2))return ASTER_EINVAL;
 enabled=1;aster_printf("PAL native UI: 18px glyphs=%u record_bytes=%u font_capacity=%u",glyphs,(unsigned)(sizeof(items)+sizeof(clear_work)+sizeof(copy_work)),(unsigned)sizeof(font));return 0;
}
void PalAsterUiForget(const void *owner)
{if(!enabled)return;unsigned n=0;for(unsigned i=0;i<count;i++)if(items[i].owner!=owner)items[n++]=items[i];count=n;}
void PalAsterUiClear(const void *owner,int x,int y,int w,int h)
{
 if(!enabled||!owner||w<=0||h<=0)return;
 Clip box=rect(x,y,w,h);unsigned n=0;
 for(unsigned i=0;i<count;i++){
  UiRecord r=items[i];Clip hit;
  if(r.owner!=owner||!intersect(r.clip,box,&hit)){
   if(n>=UI_RECORDS){overflow();return;}clear_work[n++]=r;continue;
  }
  Clip parts[]={rect(r.clip.x0,r.clip.y0,r.clip.x1-r.clip.x0,hit.y0-r.clip.y0),
   rect(r.clip.x0,hit.y1,r.clip.x1-r.clip.x0,r.clip.y1-hit.y1),
   rect(r.clip.x0,hit.y0,hit.x0-r.clip.x0,hit.y1-hit.y0),
   rect(hit.x1,hit.y0,r.clip.x1-hit.x1,hit.y1-hit.y0)};
  for(unsigned j=0;j<4;j++)if(parts[j].x0<parts[j].x1&&parts[j].y0<parts[j].y1){
   if(n>=UI_RECORDS){overflow();return;}r.clip=parts[j];clear_work[n++]=r;
  }
 }
 memcpy(items,clear_work,n*sizeof(*items));count=n;
}
void PalAsterUiCopy(const void *src,int sx,int sy,int w,int h,const void *dst,int dx,int dy)
{
 if(!enabled||!src||!dst||w<=0||h<=0)return;
 unsigned n=0;Clip source=rect(sx,sy,w,h);
 for(unsigned i=0;i<count;i++)if(items[i].owner==src){
  UiRecord r=items[i];if(!clipped(&r,source))continue;
  if(n>=UI_COPY_RECORDS){overflow();return;}
  r.owner=dst;r.x+=dx-sx;r.y+=dy-sy;
  r.clip.x0+=dx-sx;r.clip.x1+=dx-sx;r.clip.y0+=dy-sy;r.clip.y1+=dy-sy;
  copy_work[n++]=r;
 }
 PalAsterUiClear(dst,dx,dy,w,h);
 for(unsigned i=0;i<n;i++)append(copy_work[i]);
}
int PalAsterUiGlyph(SDL_Surface *surface,int x,int y,uint16_t cp,uint8_t color,int shadow)
{
 if(!enabled||!surface||!surface->pixels)return 0;
 unsigned lo=0,hi=glyphs;
 while(lo<hi){unsigned mid=(lo+hi)/2;if(le16(font+mid*2)<cp)lo=mid+1;else hi=mid;}
 if(lo>=glyphs||le16(font+lo*2)!=cp){aster_errfmt("PAL native UI missing glyph U+%04x",cp);aster_app_exit(1);return 1;}
 /* Replace a selection recolor or changed character without accumulating
  * records, and preserve panels/other overlapping glyphs in painter order. */
 unsigned n=0;
 for(unsigned i=0;i<count;i++)if(!(items[i].owner==surface->pixels&&items[i].kind==1&&items[i].x==x&&items[i].y==y))items[n++]=items[i];
 count=n;
 UiRecord r={.owner=surface->pixels,.clip=rect(x,y,cp<128?7:13,13),.x=x,.y=y,.glyph=lo,.color=color,.shadow=shadow,.kind=1};
 if(clipped(&r,rect(0,0,surface->w,surface->h)))append(r);
 return 1;
}
void PalAsterUiPanel(SDL_Surface *s,int x,int y,int w,int h)
{
 if(!enabled||!s||!s->pixels||w<=2||h<=2)return;
 int cx=x+w/2,cy=y+h/2;
 if(cx<0||cy<0||cx>=s->w||cy>=s->h)return;
 const uint8_t *pixels=s->pixels;
 uint8_t fill=pixels[cy*s->pitch+cx];
 /* Keep the original palette's fill and border, including palette fades. */
 int bx=x+w/2,by=y+1;if(by<0)by=0;if(by>=s->h)by=s->h-1;
 UiRecord r={.owner=s->pixels,.clip=rect(x,y,w,h),.x=x,.y=y,.w=w,.h=h,.color=fill,.border=pixels[by*s->pitch+bx],.kind=2};
 if(clipped(&r,rect(0,0,s->w,s->h)))append(r);
}
static uint16_t blend(uint16_t a,uint16_t b,unsigned alpha)
{
 unsigned r=(((a>>11)&31)*(15-alpha)+((b>>11)&31)*alpha+7)/15;
 unsigned g=(((a>>5)&63)*(15-alpha)+((b>>5)&63)*alpha+7)/15;
 unsigned v=((a&31)*(15-alpha)+(b&31)*alpha+7)/15;
 return (uint16_t)((r<<11)|(g<<5)|v);
}
static int scale(int x){return x>=0?x*3/2:-((-x*3+1)/2);}
void PalAsterUiComposite(const void *owner,uint16_t *pixels,int pitch,const uint16_t *palette)
{
 if(!enabled)return;
 for(unsigned i=0;i<count;i++){
  const UiRecord *r=&items[i];if(r->owner!=owner)continue;
  Clip c=r->clip;if(!intersect(c,rect(0,0,320,200),&c))continue;
  int x0=scale(c.x0),y0=scale(c.y0),x1=scale(c.x1),y1=scale(c.y1);
  int gx=scale(r->x),gy=scale(r->y);
  if(r->kind==2){
   int right=scale(r->x+r->w),bottom=scale(r->y+r->h);
   for(int y=y0;y<y1;y++)for(int x=x0;x<x1;x++){
    int edge=x-gx<2||right-x<=2||y-gy<2||bottom-y<=2;
    pixels[y*pitch+x]=palette[edge?r->border:r->color];
   }
  }else{
   const uint8_t *bitmap=font+glyphs*2+r->glyph*162;
   for(int pass=r->shadow?1:0;pass>=0;pass--){
    int offset=pass;uint16_t color=palette[pass?0:r->color];
    for(int y=0;y<18;y++)for(int x=0;x<18;x++){
     int dx=gx+x+offset,dy=gy+y+offset;
     if(dx<x0||dy<y0||dx>=x1||dy>=y1)continue;
     unsigned j=y*18+x,alpha=(bitmap[j/2]>>(j&1?0:4))&15;
     if(alpha){uint16_t *p=pixels+dy*pitch+dx;*p=blend(*p,color,alpha);}
    }
   }
  }
 }
}
unsigned PalAsterUiCount(const void *owner){unsigned n=0;for(unsigned i=0;i<count;i++)if(items[i].owner==owner)n++;return n;}
#else
int PalAsterUiInit(int enabled){(void)enabled;return 0;}
int PalAsterUiGlyph(SDL_Surface *s,int x,int y,uint16_t cp,uint8_t c,int sh){(void)s;(void)x;(void)y;(void)cp;(void)c;(void)sh;return 0;}
void PalAsterUiPanel(SDL_Surface *s,int x,int y,int w,int h){(void)s;(void)x;(void)y;(void)w;(void)h;}
void PalAsterUiCopy(const void *s,int x,int y,int w,int h,const void *d,int a,int b){(void)s;(void)x;(void)y;(void)w;(void)h;(void)d;(void)a;(void)b;}
void PalAsterUiForget(const void *s){(void)s;}
void PalAsterUiClear(const void *s,int x,int y,int w,int h){(void)s;(void)x;(void)y;(void)w;(void)h;}
void PalAsterUiComposite(const void *s,uint16_t *p,int w,const uint16_t *pal){(void)s;(void)p;(void)w;(void)pal;}
unsigned PalAsterUiCount(const void *s){(void)s;return 0;}
#endif
