#include "pal_libc.h"
#include "main.h"
#include "pal_aster_ui.h"

#if PAL_ASTER_NATIVE_UI
int __real_SDL_BlitSurface(SDL_Surface *,const SDL_Rect *,SDL_Surface *,SDL_Rect *);
int __real_SDL_BlitScaled(SDL_Surface *,const SDL_Rect *,SDL_Surface *,SDL_Rect *);
int __real_SDL_FillRect(SDL_Surface *,const SDL_Rect *,Uint32);
void __real_SDL_FreeSurface(SDL_Surface *);
static void copy_ui(SDL_Surface *src,const SDL_Rect *s,SDL_Surface *dst,const SDL_Rect *d)
{
 if(!src||!dst||!src->pixels||!dst->pixels)return;
 int sx=s?s->x:0,sy=s?s->y:0,w=s?s->w:src->w,h=s?s->h:src->h;
 int dx=d?d->x:0,dy=d?d->y:0;
 if(d&&((d->w>0&&d->w!=w)||(d->h>0&&d->h!=h))){PalAsterUiClear(dst->pixels,dx,dy,d->w,d->h);return;}
 int left=0,top=0,right=w,bottom=h;
 if(sx<0)left=-sx;if(dx<0&&-dx>left)left=-dx;
 if(sy<0)top=-sy;if(dy<0&&-dy>top)top=-dy;
 if(src->w-sx<right)right=src->w-sx;if(dst->w-dx<right)right=dst->w-dx;
 if(src->h-sy<bottom)bottom=src->h-sy;if(dst->h-dy<bottom)bottom=dst->h-dy;
 PalAsterUiCopy(src->pixels,sx+left,sy+top,right-left,bottom-top,dst->pixels,dx+left,dy+top);
}
int __wrap_SDL_BlitSurface(SDL_Surface *s,const SDL_Rect *sr,SDL_Surface *d,SDL_Rect *dr)
{int rc=__real_SDL_BlitSurface(s,sr,d,dr);if(!rc)copy_ui(s,sr,d,dr);return rc;}
int __wrap_SDL_BlitScaled(SDL_Surface *s,const SDL_Rect *sr,SDL_Surface *d,SDL_Rect *dr)
{int rc=__real_SDL_BlitScaled(s,sr,d,dr);if(!rc)copy_ui(s,sr,d,dr);return rc;}
int __wrap_SDL_FillRect(SDL_Surface *s,const SDL_Rect *r,Uint32 c)
{int rc=__real_SDL_FillRect(s,r,c);if(!rc&&s)PalAsterUiClear(s->pixels,r?r->x:0,r?r->y:0,r?r->w:s->w,r?r->h:s->h);return rc;}
void __wrap_SDL_FreeSurface(SDL_Surface *s){if(s)PalAsterUiForget(s->pixels);__real_SDL_FreeSurface(s);}
static void clear_sprite(LPCBITMAPRLE r,SDL_Surface *s,PAL_POS p,int height)
{if(r&&s){int h=PAL_RLEGetHeight(r);if(height>=0&&height<h)h=height;PalAsterUiClear(s->pixels,PAL_X(p),PAL_Y(p),PAL_RLEGetWidth(r),h);}}
INT __real_PAL_RLEBlitToSurface(LPCBITMAPRLE,SDL_Surface *,PAL_POS);
INT __wrap_PAL_RLEBlitToSurface(LPCBITMAPRLE r,SDL_Surface *s,PAL_POS p)
{clear_sprite(r,s,p,-1);return __real_PAL_RLEBlitToSurface(r,s,p);}
INT __real_PAL_RLEBlitToSurfaceWithHeight(LPCBITMAPRLE,SDL_Surface *,PAL_POS,INT);
INT __wrap_PAL_RLEBlitToSurfaceWithHeight(LPCBITMAPRLE r,SDL_Surface *s,PAL_POS p,INT h)
{clear_sprite(r,s,p,h);return __real_PAL_RLEBlitToSurfaceWithHeight(r,s,p,h);}
INT __real_PAL_RLEBlitToSurfaceWithShadow(LPCBITMAPRLE,SDL_Surface *,PAL_POS,BOOL);
INT __wrap_PAL_RLEBlitToSurfaceWithShadow(LPCBITMAPRLE r,SDL_Surface *s,PAL_POS p,BOOL shadow)
{clear_sprite(r,s,p,-1);return __real_PAL_RLEBlitToSurfaceWithShadow(r,s,p,shadow);}
INT __real_PAL_RLEBlitWithColorShift(LPCBITMAPRLE,SDL_Surface *,PAL_POS,INT);
INT __wrap_PAL_RLEBlitWithColorShift(LPCBITMAPRLE r,SDL_Surface *s,PAL_POS p,INT shift)
{clear_sprite(r,s,p,-1);return __real_PAL_RLEBlitWithColorShift(r,s,p,shift);}
INT __real_PAL_RLEBlitMonoColor(LPCBITMAPRLE,SDL_Surface *,PAL_POS,BYTE,INT);
INT __wrap_PAL_RLEBlitMonoColor(LPCBITMAPRLE r,SDL_Surface *s,PAL_POS p,BYTE color,INT shift)
{clear_sprite(r,s,p,-1);return __real_PAL_RLEBlitMonoColor(r,s,p,color,shift);}
INT __real_PAL_FBPBlitToSurface(LPCBYTE,SDL_Surface *);
INT __wrap_PAL_FBPBlitToSurface(LPCBYTE b,SDL_Surface *s)
{if(s)PalAsterUiForget(s->pixels);return __real_PAL_FBPBlitToSurface(b,s);}
INT __real_PAL_FBPBlitChunkToSurface(FILE *,UINT,SDL_Surface *);
INT __wrap_PAL_FBPBlitChunkToSurface(FILE *f,UINT c,SDL_Surface *s)
{if(s)PalAsterUiForget(s->pixels);return __real_PAL_FBPBlitChunkToSurface(f,c,s);}
VOID __real_PAL_MakeScene(VOID);
VOID __wrap_PAL_MakeScene(VOID)
{if(gpScreen)PalAsterUiForget(gpScreen->pixels);__real_PAL_MakeScene();}
#endif
