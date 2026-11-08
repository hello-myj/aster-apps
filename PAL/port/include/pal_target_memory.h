#ifndef PAL_ASTER_TARGET_MEMORY_H
#define PAL_ASTER_TARGET_MEMORY_H
#include <stdint.h>
#define PAL_EXTREME_SCREEN_WIDTH 320
#define PAL_EXTREME_SCREEN_HEIGHT 200
#define PAL_EXTREME_SCREEN_BYTES (320*200)
#define PAL_EXTREME_FBP_SCANLINE_BYTES 320
extern uint8_t pal_sram_framebuffer[320*200];
extern uint8_t pal_sram_aux_framebuffer[320*200];
extern uint8_t pal_sram_fbp_scanline[320];
#endif
