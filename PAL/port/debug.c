#include "main.h"
/* Read-only harness inspection, never used to move or modify game state. */
unsigned pal_debug_main_menu(void){return gpGlobals?(unsigned)gpGlobals->iCurMainMenuItem:0;}
unsigned pal_debug_system_menu(void){return gpGlobals?(unsigned)gpGlobals->iCurSystemMenuItem:0;}
unsigned pal_debug_viewport(void){return gpGlobals?gpGlobals->viewport:0;}
unsigned pal_debug_scene(void){return gpGlobals?gpGlobals->wNumScene:0;}
