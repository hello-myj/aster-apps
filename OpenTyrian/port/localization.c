/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "localization.h"
#include "zh_data.h"
#include <string.h>
static const char *translate(const char *text){
 char key[192];unsigned i=0;
 for(;*text;text++){unsigned char c=*text;if(c=='~'||c==' '||c=='\t'||c=='\r'||c=='\n')continue;if(i+1>=sizeof(key))return NULL;if(c>='A'&&c<='Z')c+=32;key[i++]=c;}key[i]=0;
 for(unsigned j=0;j<sizeof(zh_dictionary)/sizeof(*zh_dictionary);j++)if(!strcmp(key,zh_dictionary[j].en))return zh_dictionary[j].zh;
 return NULL;
}
static unsigned codepoint(const char **s){const unsigned char *p=(const unsigned char*)*s;unsigned c=*p++;if(c>=0xe0){c=(c&15)<<12;c|=(*p++&63)<<6;c|=*p++&63;}else if(c>=0xc0){c=(c&31)<<6;c|=*p++&63;}*s=(const char*)p;return c;}
static int glyph(unsigned c){unsigned lo=0,hi=sizeof(zh_codes)/sizeof(*zh_codes);while(lo<hi){unsigned m=(lo+hi)/2;if(zh_codes[m]<c)lo=m+1;else hi=m;}return lo<sizeof(zh_codes)/sizeof(*zh_codes)&&zh_codes[lo]==c?(int)lo:-1;}
static unsigned style(const char *s,unsigned font){
 /* The title menu uses FONT_NORMAL, 13 px rows and a two-pixel outline.
  * Use the 10 px subset for these labels so adjacent outlined rows never touch. */
 if(font<=1){const char *items[]={"开始游戏","读取存档","最高得分","操作说明","游戏设置","自动演示","退出游戏"};for(unsigned i=0;i<7;i++)if(!strcmp(s,items[i]))return 2;}
 return font<3?font:2;
}
/* A bounded glyph cache travels through the original VGA copies by matching
 * their complete pixel footprint. An overwritten/partially obscured glyph is
 * never replayed. Cache storage is guest linear memory (PSRAM on P4).
 * Background samples let the final pass replace the small mask rather than
 * placing a smooth glyph on top of its enlarged jagged silhouette. */
#define NATIVE_GLYPHS 128
struct native_glyph {int16_t x,y;uint16_t glyph;uint8_t font,color,valid;uint8_t background[256];};
static struct native_glyph native_glyphs[NATIVE_GLYPHS];
static unsigned native_next;
static int native_matches(const struct native_glyph *g,const uint8_t *pixels,unsigned pitch){
 unsigned n=zh_sizes[g->font];
 for(unsigned yy=0;yy<n;yy++)for(unsigned xx=0;xx<n;xx++){
  int x=g->x+(int)xx,y=g->y+(int)yy;if(x<0||x>=320||y<0||y>=200)continue;
  uint8_t expected=(zh_bits[g->font][g->glyph][yy]&(1u<<xx))?g->color:g->background[yy*n+xx];
  if(pixels[y*pitch+x]!=expected)return 0;
 }return 1;
}
static void native_remember(SDL_Surface *surface,int x,int y,unsigned at,unsigned font,unsigned color){
 if(surface->w!=320||surface->h!=200)return;
 struct native_glyph *g=NULL;unsigned n=zh_sizes[font];int reuse=0;
 for(unsigned i=0;i<NATIVE_GLYPHS;i++)if(native_glyphs[i].valid&&native_glyphs[i].x==x&&native_glyphs[i].y==y&&native_glyphs[i].font==font&&native_glyphs[i].glyph==at){g=&native_glyphs[i];reuse=native_matches(g,surface->pixels,surface->pitch);break;}
 if(!g)g=&native_glyphs[native_next++%NATIVE_GLYPHS];
 if(!reuse)for(unsigned yy=0;yy<n;yy++)for(unsigned xx=0;xx<n;xx++)g->background[yy*n+xx]=(x+(int)xx>=0&&x+(int)xx<320&&y+(int)yy>=0&&y+(int)yy<200)?((uint8_t*)surface->pixels)[(y+yy)*surface->pitch+x+xx]:0;
 g->x=x;g->y=y;g->glyph=at;g->font=font;g->color=color;g->valid=1;
}
static uint16_t native_blend(uint16_t bg,uint16_t fg,unsigned a){
 unsigned r=((bg>>11)*(3-a)+(fg>>11)*a+1)/3;
 unsigned g=(((bg>>5)&63)*(3-a)+((fg>>5)&63)*a+1)/3;
 unsigned b=((bg&31)*(3-a)+(fg&31)*a+1)/3;return (r<<11)|(g<<5)|b;
}
void tyrian_zh_present(uint16_t *out,unsigned pitch,const uint8_t *source,const uint16_t *palette){
 for(unsigned i=0;i<NATIVE_GLYPHS;i++){
  const struct native_glyph *g=&native_glyphs[i];if(!g->valid||!native_matches(g,source,320))continue;
  unsigned n=zh_sizes[g->font],size=zh_native_sizes[g->font];
  const uint8_t *bits=zh_native_bits[g->font]+g->glyph*zh_native_stride[g->font];
  int left=g->x*3/2,top=g->y*3/2;
  for(unsigned yy=0;yy<size;yy++)for(unsigned xx=0;xx<size;xx++){
   int dx=left+xx,dy=top+yy;if(dx<0||dx>=480||dy<0||dy>=300)continue;
   int sx=dx*2/3-g->x,sy=dy*2/3-g->y;
   uint8_t bg=(sx>=0&&sx<(int)n&&sy>=0&&sy<(int)n)?g->background[sy*n+sx]:source[(dy*2/3)*320+dx*2/3];
   unsigned bit=yy*size+xx,a=(bits[bit/4]>>((bit%4)*2))&3;
   unsigned edge=a;
   for(int oy=-1;oy<=1;oy++)for(int ox=-1;ox<=1;ox++){
    int nx=(int)xx+ox,ny=(int)yy+oy;if(nx<0||ny<0||nx>=(int)size||ny>=(int)size)continue;
    unsigned at=ny*size+nx,v=(bits[at/4]>>((at%4)*2))&3;if(v>edge)edge=v;
   }
   out[(dy+40)*pitch+dx]=native_blend(native_blend(palette[bg],0,edge),palette[g->color],a);
  }
 }
}
int tyrian_zh_width(const char *text,unsigned font){const char *s=translate(text);if(!s)return -1;unsigned n=zh_sizes[style(s,font)];int width=0;while(*s){unsigned c=codepoint(&s);width+=c<128?n/2:n;}return width;}
int tyrian_zh_draw(SDL_Surface *surface,int x,int y,const char *text,unsigned font,unsigned hue,int brightness,int dark){
 const char *s=translate(text);if(!s)return 0;if(dark)return 1;font=style(s,font);int n=zh_sizes[font];/* Keep positive menu brightness values distinct instead of clipping both
  * selected (+6) and ordinary (+3) labels to palette shade 15. */
 int highlight=0;for(const char *p=text;*p;p++)if(*p=='~'){highlight=4;break;}
 brightness+=highlight;
 /* Negative values keep the original readable base; positive values use
  * the remaining shades without flattening the menu's +3 / +6 states. */
 int shade=12+(brightness>0?(brightness+2)/3:brightness);if(shade<1)shade=1;if(shade>15)shade=15;unsigned color=(hue&15)*16+shade;
 if(y+n>surface->h)y=surface->h-n;
 while(*s){unsigned c=codepoint(&s);int at=glyph(c);if(at>=0&&!dark&&brightness>-8)native_remember(surface,x,y,at,font,color);if(at>=0)for(int yy=0;yy<n;yy++)for(int xx=0;xx<n;xx++)if((zh_bits[font][at][yy]&(1u<<xx))&&x+xx>=0&&x+xx<surface->w&&y+yy>=0&&y+yy<surface->h){uint8_t *p=(uint8_t*)surface->pixels+(y+yy)*surface->pitch+x+xx;*p=dark?0:color;}x+=c<128?n/2:n;}
 return 1;
}
void tyrian_zh_rgb(uint16_t *p,unsigned pitch,int x,int y,const char *s,uint16_t color){while(*s){unsigned c=codepoint(&s);int at=glyph(c);if(at>=0)for(int yy=0;yy<16;yy++)for(int xx=0;xx<16;xx++)if(zh_bits[0][at][yy]&(1u<<xx))p[(y+yy)*pitch+x+xx]=color;x+=c<128?8:16;}}
