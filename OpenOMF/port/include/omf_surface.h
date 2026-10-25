#ifndef OMF_COMPACT_SURFACE_H
#define OMF_COMPACT_SURFACE_H
#include "video/surface.h"
const vga_pixel *omf_surface_pixels(const surface *s);
void omf_surface_pack(surface *s);
int omf_surface_encoded(surface *s,int w,int h,char **data,unsigned len);
void omf_surface_promote(surface *s);
void omf_surface_forget(surface *s);
unsigned omf_surface_saved(void);
unsigned omf_surface_resident(void);
#endif
