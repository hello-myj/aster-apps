#pragma once
#include <stdint.h>
enum {S_MENU,S_HIGH,S_NEW_HIGH,S_GOAL,S_MADE_GOAL,S_SHOP,S_GAME,S_OVER};
enum {A_MONEY,A_RESET,A_START,A_BACK,A_EXPLODE,A_HIGH,A_NORMAL,A_LOW,A_GOAL,A_MADE};
extern int gm_state,gm_paused,gm_exit;
void gm_init(void);
void gm_update(float dt);
void gm_render(void);
void gm_key(const char *key);
void gm_touch(int x,int y);
const char *gm_state_name(void);
void gm_blit(const char *name,int frame,float x,float y,float angle,float sx,float sy,float ox,float oy);
int gm_width(const char *name);
int gm_height(const char *name);
void gm_text(int font,const char *text,float x,float y,uint16_t color);
void gm_rect(int x,int y,int w,int h,uint16_t color,unsigned alpha);
void gm_line(int x,int y,int x2,int y2,uint16_t color,unsigned alpha);
void gm_audio_play(int id);
void gm_audio_stop(int id);
int gm_audio_playing(int id);
void gm_audio_pause(int pause);
void gm_save(int score,int level);
void gm_load(int *score,int *level);
#define YELLOW 0xfea4
#define GREEN 0x4508
#define ORANGE 0xeb60
#define DEEP_ORANGE 0xc440
