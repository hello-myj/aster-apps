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
 /* Title menu rows are only 13 pixels apart despite using FONT_LARGE. */
 if(font==0){const char *items[]={"开始游戏","读取存档","最高得分","操作说明","游戏设置","自动演示","退出游戏"};for(unsigned i=0;i<7;i++)if(!strcmp(s,items[i]))return 2;}
 return font<3?font:2;
}
int tyrian_zh_width(const char *text,unsigned font){const char *s=translate(text);if(!s)return -1;unsigned n=zh_sizes[style(s,font)];int width=0;while(*s){unsigned c=codepoint(&s);width+=c<128?n/2:n;}return width;}
int tyrian_zh_draw(SDL_Surface *surface,int x,int y,const char *text,unsigned font,unsigned hue,int brightness,int dark){
 const char *s=translate(text);if(!s)return 0;font=style(s,font);int n=zh_sizes[font];int shade=12+brightness;if(shade<1)shade=1;if(shade>15)shade=15;unsigned color=(hue&15)*16+shade;
 if(y+n>surface->h)y=surface->h-n;
 while(*s){unsigned c=codepoint(&s);int at=glyph(c);if(at>=0)for(int yy=0;yy<n;yy++)for(int xx=0;xx<n;xx++)if((zh_bits[font][at][yy]&(1u<<xx))&&x+xx>=0&&x+xx<surface->w&&y+yy>=0&&y+yy<surface->h){uint8_t *p=(uint8_t*)surface->pixels+(y+yy)*surface->pitch+x+xx;*p=dark?0:color;}x+=c<128?n/2:n;}
 return 1;
}
void tyrian_zh_rgb(uint16_t *p,unsigned pitch,int x,int y,const char *s,uint16_t color){while(*s){unsigned c=codepoint(&s);int at=glyph(c);if(at>=0)for(int yy=0;yy<16;yy++)for(int xx=0;xx<16;xx++)if(zh_bits[0][at][yy]&(1u<<xx))p[(y+yy)*pitch+x+xx]=color;x+=c<128?8:16;}}
