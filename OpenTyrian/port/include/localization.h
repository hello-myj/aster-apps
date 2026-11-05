/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <SDL.h>
int tyrian_zh_width(const char *text,unsigned font);
int tyrian_zh_draw(SDL_Surface *surface,int x,int y,const char *text,unsigned font,unsigned hue,int brightness,int dark);
void tyrian_zh_rgb(uint16_t *pixels,unsigned pitch,int x,int y,const char *text,uint16_t color);

void tyrian_zh_present(uint16_t *out,unsigned pitch,const uint8_t *source,const uint16_t *palette);
