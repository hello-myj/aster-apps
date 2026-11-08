/* Host check for presentation ownership/restore semantics, not a substitute
 * for real-game screenshots or device visual acceptance. */
#include <assert.h>
#include "../port/native_ui.c"
static int aborted;
void pal_assert(const char *expr,const char *file,int line)
{(void)expr;(void)file;(void)line;exit(1);}
void aster_logf(int level,const char *format,...){(void)level;(void)format;}
void aster_app_exit(int32_t code){aborted=code?code:1;}
static uint8_t screen[64000],backup[64000],box[400];
static uint16_t image[480*300],before[480*300],colors[256];
static SDL_Surface surface;
static void reset(void)
{count=0;enabled=1;aborted=0;memset(image,0,sizeof(image));}
int main(void)
{
 surface.pixels=screen;surface.w=320;surface.h=200;surface.pitch=320;
 glyphs=1;font[0]=0x00;font[1]=0x4e;memset(font+2,255,162);
 colors[1]=0xffff;colors[2]=0xf800;
 reset();
 assert(PalAsterUiGlyph(&surface,10,10,0x4e00,1,0));
 assert(PalAsterUiCount(screen)==1);
 assert(PalAsterUiGlyph(&surface,10,10,0x4e00,2,0));
 assert(PalAsterUiCount(screen)==1); /* selection recolor replaces */
 PalAsterUiCopy(screen,0,0,320,200,backup,0,0);
 for(int i=0;i<200;i++){
  PalAsterUiForget(screen);PalAsterUiCopy(backup,0,0,320,200,screen,0,0);
  assert(PalAsterUiCount(screen)==1&&PalAsterUiCount(backup)==1);
 }
 PalAsterUiComposite(screen,image,480,colors);
 assert(image[16*480+16]==0xf800);
 PalAsterUiClear(screen,12,12,2,2);
 memset(image,0,sizeof(image));PalAsterUiComposite(screen,image,480,colors);
 assert(image[16*480+16]==0xf800&&image[19*480+19]==0);
 /* Save a raw menu region and restore its native glyph, just like the
  * engine's fixed box-pixel buffers; owner identity must survive cropping. */
 PalAsterUiCopy(backup,8,8,20,20,box,0,0);
 PalAsterUiForget(screen);PalAsterUiCopy(box,0,0,20,20,screen,8,8);
 memset(image,0,sizeof(image));PalAsterUiComposite(screen,image,480,colors);
 assert(image[16*480+16]==0xf800);
 /* Self-copy must snapshot records before clearing the destination. */
 PalAsterUiCopy(screen,0,0,40,40,screen,4,4);
 memset(image,0,sizeof(image));PalAsterUiComposite(screen,image,480,colors);
 assert(image[22*480+22]==0xf800);
 /* Clipping a panel must preserve painter order relative to its text. */
 reset();append((UiRecord){.owner=screen,.clip={0,0,80,80},.x=0,.y=0,.w=80,.h=80,.color=2,.border=2,.kind=2});
 PalAsterUiGlyph(&surface,40,40,0x4e00,1,0);
 PalAsterUiComposite(screen,before,480,colors);
 PalAsterUiClear(screen,10,10,5,5);
 PalAsterUiComposite(screen,image,480,colors);
 for(int y=60;y<77;y++)for(int x=60;x<77;x++)assert(image[y*480+x]==before[y*480+x]);
 assert(!aborted);
 /* Capacity exhaustion fails closed, rather than overwriting a fixed owner. */
 count=UI_RECORDS;append(items[0]);assert(aborted&&count==UI_RECORDS);
 return 0;
}
