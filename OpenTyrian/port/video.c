/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "tyrian_port.h"
#include "video.h"
#include "video_scale.h"
#include "palette.h"
#include <stdlib.h>
#include <string.h>

const char *const scaling_mode_names[ScalingMode_MAX]={"Center","Integer","Fit 8:5","Fit 4:3"};
int fullscreen_display=-1;
ScalingMode scaling_mode=SCALE_ASPECT_8_5;
SDL_Surface *VGAScreen,*VGAScreenSeg,*VGAScreen2,*game_screen;
SDL_Window *main_window;
SDL_PixelFormat *main_window_tex_format;
uint scaler;
const struct Scalers scalers[]={{320,200,NULL,NULL,"Nearest"}};
const uint scalers_count=1;
static aster_handle_t surface;
static unsigned width,height,left,top,view_w,view_h;
static uint8_t pending[320*200];
static uint16_t pending_palette[256];
static int dirty;
void tyrian_video_invalidate(void){dirty=1;}

int tyrian_video_init(void){
 aster_display_info_t info={.size=sizeof(info),.version=1};
 int rc=aster_display_get_info(0,&info,sizeof(info));if(rc<0)return rc;
 width=info.app_width;height=info.app_height;
 if(width<320||height<200||width>1024||height>768)return ASTER_ENOTSUP;
 view_w=width;view_h=view_w*200/320;
 if(view_h>height){view_h=height;view_w=view_h*320/200;}
 left=(width-view_w)/2;top=(height-view_h)/2;
 aster_surface_desc_t desc={.size=sizeof(desc),.version=1,.width=width,.height=height,.format=ASTER_PIXEL_RGB565};
 return aster_surface_create(&desc,sizeof(desc),&surface);
}
void init_video(void){
 VGAScreen=VGAScreenSeg=SDL_CreateRGBSurface(0,320,200,8,0,0,0,0);
 VGAScreen2=SDL_CreateRGBSurface(0,320,200,8,0,0,0,0);
 game_screen=SDL_CreateRGBSurface(0,320,200,8,0,0,0,0);
 main_window_tex_format=SDL_AllocFormat(SDL_PIXELFORMAT_RGB565);
 if(!VGAScreen||!VGAScreen2||!game_screen||!main_window_tex_format)tyrian_exit(ASTER_ENOMEM);
}
void deinit_video(void){SDL_FreeSurface(VGAScreen);SDL_FreeSurface(VGAScreen2);SDL_FreeSurface(game_screen);VGAScreen=VGAScreenSeg=VGAScreen2=game_screen=NULL;SDL_FreeFormat(main_window_tex_format);main_window_tex_format=NULL;}
void tyrian_video_destroy(void){if(surface){aster_surface_destroy(surface);surface=0;}}
void JE_clr256(SDL_Surface *s){memset(s->pixels,0,s->pitch*s->h);}
void JE_showVGA(void){
 if(!VGAScreen)return;
 for(int y=0;y<200;y++)memcpy(pending+y*320,(uint8_t*)VGAScreen->pixels+y*VGAScreen->pitch,320);
 for(unsigned i=0;i<256;i++)pending_palette[i]=(uint16_t)rgb_palette[i];
 dirty=1;
 /* Yield at most once per simulated millisecond, so even untimed animation
  * loops remain cooperative without forcing one Host callback per VGA flip. */
 tyrian_yield(1);
}
int tyrian_video_present(void){
 if(!dirty)return 0;
 aster_surface_buffer_t b={.size=sizeof(b),.version=1};int rc=aster_surface_acquire(surface,&b,sizeof(b));
 if(rc<0)return rc;if(!b.ptr||b.format!=ASTER_PIXEL_RGB565)return ASTER_ENOTSUP;
 uint16_t *out=b.ptr;unsigned pitch=b.pitch/2;
 for(unsigned y=0;y<height;y++)memset(out+y*pitch,0,width*2);
 unsigned xmap[1024];for(unsigned x=0;x<view_w;x++)xmap[x]=x*320/view_w;
 for(unsigned y=0;y<view_h;y++){
  const uint8_t *src=pending+(y*200/view_h)*320;uint16_t *dst=out+(top+y)*pitch+left;
  for(unsigned x=0;x<view_w;x++)dst[x]=pending_palette[src[xmap[x]]];
 }
 tyrian_overlay(out,pitch,width,height);
 rc=aster_surface_present(surface);if(rc<0)return rc;dirty=0;return 1;
}
void reinit_fullscreen(int display){(void)display;fullscreen_display=-1;}
void toggle_fullscreen(void){}
void video_on_win_resize(void){}
bool init_scaler(unsigned int s){(void)s;scaler=0;return true;}
void set_scaler_by_name(const char *name){(void)name;scaler=0;}
bool set_scaling_mode_by_name(const char *name){for(unsigned i=0;i<ScalingMode_MAX;i++)if(!strcmp(name,scaling_mode_names[i])){scaling_mode=i;return true;}return false;}
void mapScreenPointToWindow(Sint32 *x,Sint32 *y){*x=left+*x*(int)view_w/320;*y=top+*y*(int)view_h/200;}
void mapWindowPointToScreen(Sint32 *x,Sint32 *y){*x=(*x-(int)left)*320/(int)view_w;*y=(*y-(int)top)*200/(int)view_h;}
void scaleWindowDistanceToScreen(Sint32 *x,Sint32 *y){*x=*x*320/(int)view_w;*y=*y*200/(int)view_h;}
