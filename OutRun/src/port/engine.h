/* GPL-3.0-or-later. Platform port of ZgzInfinity/OutRun. */
#ifndef OUTRUN_PORT_H
#define OUTRUN_PORT_H
#include <aster/aster.h>
#include "aster_printf.h"
#include "aster_libc.h"
void *malloc(size_t);void *calloc(size_t,size_t);void free(void *);
#include <stdint.h>
#include <stdbool.h>

static inline float fabsf(float x){return x<0?-x:x;}
static inline float ceilf(float x){int n=(int)x;return (float)(n+(x>n));}
static inline float fmodf(float x,float y){return x-(int)(x/y)*y;}
#define CLAMP(v,a,b) ((v)<(a)?(a):((v)>(b)?(b):(v)))
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))
#define CAMERA_D 0.57735026919f
#define ROAD_W 1600.0f
#define SEGMENT 150.0f
#define PLAYER_Z (800.0f*CAMERA_D+241.0f)
#define DRAW_DISTANCE 200
typedef struct {float y0,y1,curve,distance;uint32_t mirror;} RoadLine;
typedef struct {uint32_t image;float scale,width,collide,plx,ply,prx,pry,pclx,pcly,pcrx,pcry,terrain;} ObjectMeta;
typedef struct {uint32_t line,object;float x,y;uint32_t side;} Placement;
typedef struct {uint32_t magic,count,metas,objects,time,body,checkpoint,id,terrain,road_terrain;} MapHead;
typedef struct {MapHead h;uint16_t colors[12];RoadLine *line;ObjectMeta *meta;Placement *obj;uint32_t *first;} Map;
typedef struct {uint16_t w,h,ow,oh;uint16_t *pixel;uint32_t id,last,bytes;uint8_t *alpha;uint16_t *left,*right;} Image;
typedef struct {float x,y,w,scale;} Projected;
typedef struct {Projected a,b,c,d;float clip;uint32_t line;bool valid;} View;
typedef struct {float z,offset,speed,target;uint8_t model,side;uint32_t passed;} Traffic;
typedef struct {
 float position,x,speed,threshold,remaining,lap,countdown,crash,skid,score,bg_offset;
 uint32_t frames,steps,errors,collisions,gear,stage,branch,map_id,body_frame,anim,route_choice;
 int steer,touch_steer,prev_steer,elevation;bool brake,touch_brake,gas,touch_gas,auto_gas,paused,offroad,menu_open;
 int menu_choice;
 float crash_accel,crash_target;uint32_t crash_phase,crash_ticks;
 int state; /* 0 attract, 1 countdown, 2 round, 3 timeout, 4 goal */
} Game;
typedef struct {uint32_t road,bg,sprites,hud,update,acquire,present,io,bytes,hits,misses;} FrameProfile;
extern FrameProfile profile;
extern Map map;extern Game game;extern Traffic cars[20];extern View views[DRAW_DISTANCE];
extern uint16_t *pixels;extern uint32_t screen_w,screen_h,stride,cache_peak,cache_bytes,asset_errors;
extern Image font;
int read_file(const char *,void *,uint32_t);int load_map(uint32_t);void free_map(void);
Image *get_image(uint32_t);void free_images(void);int load_font(void);
void draw_frame(void);void blit(Image *,int,int,int,int,int,int);void text_at(int,int,const char *,uint16_t,int);
void rect(int,int,int,int,uint16_t);void blend_rect(int,int,int,int,uint16_t,int);
void game_reset_map(uint32_t);void game_reset(void);void game_select(int);void game_update(float);void clear_input(void);
void audio_beep(void);void audio_prepare(void);void audio_begin(void);void audio_tick(void);void audio_crash(void);void audio_end(void);
#endif






