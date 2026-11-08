#ifndef PAL_ASTER_UI_H
#define PAL_ASTER_UI_H
#include "SDL.h"
#include <stdint.h>
int PalAsterUiInit(int enabled);
int PalAsterUiGlyph(SDL_Surface *,int x,int y,uint16_t cp,uint8_t color,int shadow);
void PalAsterUiPanel(SDL_Surface *,int x,int y,int w,int h);
void PalAsterUiCopy(const void *src,int sx,int sy,int w,int h,const void *dst,int dx,int dy);
void PalAsterUiForget(const void *owner);
void PalAsterUiClear(const void *owner,int x,int y,int w,int h);
void PalAsterUiComposite(const void *owner,uint16_t *pixels,int pitch,const uint16_t *palette);
unsigned PalAsterUiCount(const void *owner);
#endif
