/* SPDX-License-Identifier: GPL-3.0-or-later
 * Shared 480x480 arcade chrome. All coordinates are also used for hit tests. */
#ifndef ASTER_P4_GAMEPAD_H
#define ASTER_P4_GAMEPAD_H
#include <aster_gfx_engine.h>
#define PAD_W 480
#define PAD_H 480
#define PAD_GAME_Y 40
#define PAD_GAME_H 300
#define PAD_DECK_Y 340
#define PAD_X 84
#define PAD_Y 414
#define PAD_RADIUS 54
static int pad_circle(int x,int y,int cx,int cy,int radius)
{int dx=x-cx,dy=y-cy;return dx*dx+dy*dy<=radius*radius;}
static int pad_box(int x,int y,int bx,int by,int w,int h)
{return x>=bx&&x<bx+w&&y>=by&&y<by+h;}
static int pad_direction(int x,int y)
{
 int dx=x-PAD_X,dy=y-PAD_Y,ax=dx<0?-dx:dx,ay=dy<0?-dy:dy,bits=0;
 if(ax*ax+ay*ay<14*14)return 0;
 if(ax*100>=ay*42)bits|=dx<0?1:2;
 if(ay*100>=ax*42)bits|=dy<0?4:8;
 return bits;
}
static unsigned pad_isqrt(unsigned n)
{unsigned x=0,bit=1u<<30;while(bit>n)bit>>=2;while(bit){if(n>=x+bit){n-=x+bit;x=(x>>1)+bit;}else x>>=1;bit>>=2;}return x;}
static void pad_stick_position(int x,int y,int *dx,int *dy)
{*dx=x-PAD_X;*dy=y-PAD_Y;unsigned radius=pad_isqrt((*dx)*(*dx)+(*dy)*(*dy));if(radius>30){*dx=*dx*30/(int)radius;*dy=*dy*30/(int)radius;}}
static void pad_text(aster_gfx_t *g,int x,int y,const char *s,int scale,uint16_t color)
{
 for(;*s;s++,x+=6*scale){unsigned c=(unsigned char)*s;if(c<32||c>126)c='?';const uint8_t *glyph=aster_font5x7[c-32];
  for(int yy=0;yy<7;yy++)for(int xx=0;xx<5;xx++)if(glyph[yy]&(1u<<(4-xx)))aster_gfx_fill_rect(g,x+xx*scale,y+yy*scale,scale,scale,color);
 }
}
static void pad_center(aster_gfx_t *g,int cx,int y,const char *s,int scale,uint16_t color)
{pad_text(g,cx-(int)strlen(s)*6*scale/2,y,s,scale,color);}
static void pad_round(aster_gfx_t *g,int x,int y,int w,int h,int radius,uint16_t color)
{
 aster_gfx_fill_rect(g,x+radius,y,w-2*radius,h,color);aster_gfx_fill_rect(g,x,y+radius,w,h-2*radius,color);
 aster_gfx_disk(g,x+radius,y+radius,radius,color);aster_gfx_disk(g,x+w-radius-1,y+radius,radius,color);
 aster_gfx_disk(g,x+radius,y+h-radius-1,radius,color);aster_gfx_disk(g,x+w-radius-1,y+h-radius-1,radius,color);
}
static void pad_base(aster_gfx_t *g,const char *title,uint16_t accent)
{
 aster_gfx_fill_rect(g,0,0,PAD_W,PAD_GAME_Y,0x10c4);
 aster_gfx_fill_rect(g,0,PAD_DECK_Y,PAD_W,PAD_H-PAD_DECK_Y,0x10c4);
 aster_gfx_hline(g,0,479,PAD_DECK_Y,accent);
 pad_text(g,14,13,title,2,0xffff);
}
static void pad_top_button(aster_gfx_t *g,int x,int w,const char *name,int down,uint16_t accent)
{pad_round(g,x,6,w,28,7,down?accent:0x2948);pad_center(g,x+w/2,13,name,2,0xffff);}
static void pad_action(aster_gfx_t *g,int x,int y,int radius,const char *badge,const char *caption,int down,uint16_t accent)
{
 aster_gfx_disk(g,x,y,radius+2,down?0xffff:accent);aster_gfx_disk(g,x,y,radius,down?accent:0x2127);
 if(caption){pad_center(g,x,y-19,badge,3,0xffff);pad_center(g,x,y+12,caption,1,0xffff);}
 else pad_center(g,x,y-7,badge,2,0xffff);
}
static void pad_stick(aster_gfx_t *g,int dx,int dy,int active,uint16_t accent)
{
 aster_gfx_disk(g,PAD_X,PAD_Y,56,active?accent:0x526f);
 aster_gfx_disk(g,PAD_X,PAD_Y,54,0x2127);
 aster_gfx_disk(g,PAD_X,PAD_Y,35,0x2948);
 aster_gfx_disk(g,PAD_X,PAD_Y,33,0x2127);
 aster_gfx_disk(g,PAD_X+dx,PAD_Y+dy,23,active?accent:0x6b70);
 aster_gfx_disk(g,PAD_X+dx,PAD_Y+dy,20,active?0x5d9e:0x39ab);
 aster_gfx_disk(g,PAD_X+dx-5,PAD_Y+dy-6,6,active?0xbfff:0x6b70);
}
#endif
