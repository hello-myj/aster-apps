/* GPL-3.0-or-later. Aster port based on ZgzInfinity/OutRun. */
#include <aster/aster.h>
#include "aster_gfx_engine.h"
#include "aster_printf.h"
#define TRY(e) do { int32_t r_=(e); if(r_<0){aster_logf(ASTER_LOG_ERROR,"OUTRUN init line=%u err=%d",(uint32_t)__LINE__,r_); return r_;} } while(0)
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))
static aster_handle_t surface;
static aster_ge_t ge; static aster_gfx_cmd_t ring[256]; static uint8_t arena[1024];
static uint32_t W,H,player_bmp,traffic_bmp,palm_bmp,steer_bmp,brake_bmp;
static bool external_resources=false;
static uint32_t player_w,player_h,traffic_w,traffic_h,palm_w,palm_h,control_w,control_h;
static int8_t road_curve[2048]; static uint32_t frames,road_pos,errors,hit_timer,last_tick,collision_count;
static uint32_t traffic_gap=350; static float speed=0.0f,lane=0.0f,traffic_lane=0.25f,steering=0.0f,accum_ms=0.0f;
static bool touch_held=false,braking=false,collided=false;
static uint16_t color(unsigned r,unsigned g,unsigned b){return ASTER_RGB565(r,g,b);}
static int load_png(const char *name,uint32_t cap_w,uint32_t cap_h,uint32_t *handle,uint32_t *w,uint32_t *h){const char *roots[]={"/data/resources/","/assets/resources/"};char path[96];for(uint32_t k=0;k<2;k++){uint32_t n=0;while(roots[k][n]){path[n]=roots[k][n];n++;}uint32_t j=0;while(name[j]&&n<94){path[n++]=name[j++];}path[n]=0;int r=aster_gfx_bmp_load(path,n,cap_w,cap_h,handle,w,h);if(r>=0){if(k==0)external_resources=true;return r;}}return ASTER_ENOENT;}
static int read_track_path(const char *p,bool external){aster_handle_t f;int r=aster_fs_open(p,(uint32_t)strlen(p),ASTER_FS_O_RDONLY,&f);if(r<0)return r;uint8_t bytes[2048];int n=aster_fs_read(f,bytes,sizeof(bytes));aster_fs_close(f);if(n!=(int)sizeof(bytes))return ASTER_EIO;for(uint32_t i=0;i<2048;i++)road_curve[i]=(int8_t)bytes[i];if(external)external_resources=true;return 0;}
static int load_track(void){int r=read_track_path("/data/resources/track.bin",true);return r<0?read_track_path("/assets/resources/track.bin",false):r;}
static void quad(int y0,int y1,int l0,int r0,int l1,int r1,uint16_t c){aster_ge_tri(&ge,l0,y0,r0,y0,r1,y1,c);aster_ge_tri(&ge,l0,y0,r1,y1,l1,y1,c);}
static int road_center(uint32_t ahead,float perspective){int bend=road_curve[(road_pos+ahead)%2048];return (int)W/2+(int)(bend*perspective*perspective*(float)W*0.006f)-(int)(lane*(float)W*0.22f*perspective);}
static void draw_road(void){
 const int horizon=(int)(H*0.39f),rows=30; int py0=horizon,c0=(int)W/2,half0=2;
 for(int i=1;i<=rows;i++){float p=(float)i/(float)rows;int py=horizon+(int)((H-horizon)*p);int half=(int)(3.0f+p*(float)W*0.445f);int c1=road_center((uint32_t)(i*8),p);int l1=c1-half,r1=c1+half;
  int edge=MAX(1,half/10);
  uint16_t rumble=((i+road_pos/3)&1)?color(241,236,214):color(217,52,43);quad(py0,py,c0-half0-edge,c0-half0,l1-half-edge,l1-half,rumble);quad(py0,py,c0+half0,c0+half0+edge,r1,r1+edge,rumble);
  uint16_t asphalt=((i+road_pos/5)&1)?color(93,94,99):color(100,101,106);quad(py0,py,c0-half0,c0+half0,l1,r1,asphalt);
  if((i+road_pos/6)%6<3){int w0=MAX(1,half0/90),w1=MAX(1,half/90);quad(py0,py,c0-w0,c0+w0,c1-w1,c1+w1,color(242,237,217));}
  py0=py;c0=c1;half0=half;
 }
}
static void draw(void){aster_ge_clear(&ge,color(105,191,232));aster_ge_rect(&ge,0,0,W,(uint32_t)(H*.40f),color(99,183,224));aster_ge_rect(&ge,0,(uint32_t)(H*.40f),W,H-(uint32_t)(H*.40f),color(55,145,65));aster_ge_rect(&ge,0,(uint32_t)(H*.39f),W,2,color(244,226,172));draw_road();
 int horizon=(int)(H*.40f),bottom=(int)H-50;int gap=(int)traffic_gap;float p=1.0f-(float)gap/1000.0f;if(p<0.10f)p=0.10f;if(p>0.92f)p=0.92f;int ty=horizon+(int)((bottom-horizon)*p);int tc=road_center(gap/12,p)+(int)(traffic_lane*(float)W*.28f*p);uint32_t ts=(uint32_t)(95+170*p);aster_ge_bmp(&ge,traffic_bmp,traffic_w,traffic_h,tc-(int)(traffic_w*ts/512)/2,ty-(int)(traffic_h*ts/512)/2,ts,255);
 /* Upstream Map1 palm sprite, placed on both verges as scenery recedes into view. */
 for(int i=0;i<4;i++){float q=(float)(i+1)/5.0f;int y=horizon+(int)((bottom-horizon)*q);int c=road_center((road_pos/14+(uint32_t)(i*181))%2048,q);int xleft=c-(int)(W*(.18f+.28f*q));int xright=c+(int)(W*(.18f+.28f*q));uint32_t s=(uint32_t)(90+q*145);aster_ge_bmp(&ge,palm_bmp,palm_w,palm_h,xleft-(int)(palm_w*s/512)/2,y-(int)(palm_h*s/512),s,255);aster_ge_bmp(&ge,palm_bmp,palm_w,palm_h,xright-(int)(palm_w*s/512)/2,y-(int)(palm_h*s/512),s,255);}
 uint32_t ps=360;int px=(int)W/2+(int)(lane*(float)W*.24f);aster_ge_bmp(&ge,player_bmp,player_w,player_h,px-(int)(player_w*ps/512)/2,(int)H-(int)(player_h*ps/512)-38,ps,255);
 /* Native bitmap alpha provides the requested translucent virtual controls. */
 aster_ge_bmp(&ge,steer_bmp,control_w,control_h,8,(int)H-52,256,150);aster_ge_bmp(&ge,brake_bmp,control_w,control_h,(int)W-104,(int)H-52,256,150);
 aster_ge_rect(&ge,0,0,W,27,color(19,39,52));char hud[96];aster_snprintf(hud,sizeof(hud),"OUTRUN  %03d KM/H  MAP 1  %s",(int)speed,collided?"COLLISION":"AUTO ACCEL");aster_ge_text(&ge,12,20,16,color(255,238,168),ASTER_GFX_NOBG,hud);
 if(aster_ge_present(&ge)<0)errors++;
}
int aster_app_init(void){aster_surface_desc_t d={.size=sizeof(d),.version=1,.format=ASTER_PIXEL_RGB565};TRY(aster_surface_create(&d,sizeof(d),&surface));aster_surface_buffer_t b={.size=sizeof(b),.version=1};TRY(aster_surface_acquire(surface,&b,sizeof(b)));W=b.width;H=b.height;aster_ge_init(&ge,surface,W,H,ring,256,arena,sizeof(arena));
 int r=load_track();if(r<0){aster_logf(ASTER_LOG_ERROR,"Missing Map1 track. Install resources under /aster/apps/aster.outrun/data/resources or rebuild bundled fallback.");return r;}
 r=load_png("player.png",48,48,&player_bmp,&player_w,&player_h);if(r<0){aster_logf(ASTER_LOG_ERROR,"Missing player.png. Install resources to /aster/apps/aster.outrun/data/resources.");return r;}r=load_png("traffic.png",32,32,&traffic_bmp,&traffic_w,&traffic_h);if(r<0){aster_logf(ASTER_LOG_ERROR,"Missing traffic.png. Install resources to /aster/apps/aster.outrun/data/resources.");return r;}r=load_png("palm.png",36,56,&palm_bmp,&palm_w,&palm_h);if(r<0){aster_logf(ASTER_LOG_ERROR,"Missing palm.png. Install resources to /aster/apps/aster.outrun/data/resources.");return r;}r=load_png("control-steer.png",96,44,&steer_bmp,&control_w,&control_h);if(r<0){aster_logf(ASTER_LOG_ERROR,"Missing control-steer.png. Install resources to /aster/apps/aster.outrun/data/resources.");return r;}r=load_png("control-brake.png",96,44,&brake_bmp,&control_w,&control_h);if(r<0){aster_logf(ASTER_LOG_ERROR,"Missing control-brake.png. Install resources to /aster/apps/aster.outrun/data/resources.");return r;}
 aster_app_set_framerate(30);aster_logf(ASTER_LOG_INFO,"OUTRUN INIT %ux%u Map1 sections=33 resources=%s",W,H,external_resources?"external":"bundled");return 0;}
static void update_fixed(float dt){if(braking)speed-=60.0f*dt;else speed+=24.0f*dt;if(speed<0)speed=0;if(speed>100)speed=100;lane+=steering*speed*dt*0.018f;if(lane>1.35f)lane=1.35f;if(lane< -1.35f)lane=-1.35f;
 if(lane>1.0f||lane< -1.0f)speed=MAX(0,(int)(speed-14.0f*dt));
 /* Traffic is an independent moving vehicle; contact is based on longitudinal and lateral proximity. */
 float relative=(speed-52.0f)*0.18f*dt*30.0f;if(relative>0)traffic_gap=(uint32_t)MAX(0,(int)traffic_gap-(int)relative);else traffic_gap+=(uint32_t)(-relative);
 collided=false;if(traffic_gap<34){if((lane-traffic_lane<0.28f)&&(traffic_lane-lane<0.28f)){speed=MAX(18,(int)(speed*.45f));hit_timer=32;traffic_gap=750;collision_count++;aster_logf(ASTER_LOG_INFO,"OUTRUN COLLISION count=%u speed=%u",collision_count,(uint32_t)speed);}else traffic_gap=760;}
 if(hit_timer){collided=true;hit_timer--;}
 road_pos+=(uint32_t)(speed*dt*5.5f+1.0f);
}
void aster_app_event(const aster_event_t *e){if(e->type==ASTER_EVENT_TOUCH){uint32_t x=e->u.touch.x,y=e->u.touch.y;if(e->u.touch.action==ASTER_TOUCH_DOWN||e->u.touch.action==ASTER_TOUCH_MOVE){touch_held=true;braking=false;if(y>H-62&&x>=W-116){braking=true;steering=0;}else if(y>H-62&&x<116){steering=x<58?-1.0f:1.0f;}else{steering=x<W/2?-1.0f:1.0f;}aster_logf(ASTER_LOG_INFO,"OUTRUN INPUT steer=%d brake=%u",(int)(steering*100),braking?1:0);}else if(e->u.touch.action==ASTER_TOUCH_UP){touch_held=false;braking=false;steering=0;aster_logf(ASTER_LOG_INFO,"OUTRUN INPUT released");}return;}
 if(e->type!=ASTER_EVENT_FRAME)return;frames++;uint32_t now=(uint32_t)e->timestamp_ms;if(!last_tick)last_tick=now;uint32_t elapsed=now-last_tick;last_tick=now;if(elapsed>100)elapsed=100;accum_ms+=(float)elapsed;int steps=0;while(accum_ms>=33.333f&&steps<3){update_fixed(1.0f/30.0f);accum_ms-=33.333f;steps++;}if(!touch_held)steering*=0.90f;draw();if(frames%150==0)aster_logf(ASTER_LOG_INFO,"OUTRUN frames=%u pos=%u speed=%u gap=%u lane=%d errors=%u",frames,road_pos,(uint32_t)speed,traffic_gap,(int)(lane*100),errors);}
void aster_app_pause(void){touch_held=false;braking=false;steering=0;}void aster_app_resume(void){}void aster_app_stop(void){aster_logf(ASTER_LOG_INFO,"OUTRUN END frames=%u errors=%u",frames,errors);}void aster_app_destroy(void){if(player_bmp)aster_gfx_bmp_free(player_bmp);if(traffic_bmp)aster_gfx_bmp_free(traffic_bmp);if(palm_bmp)aster_gfx_bmp_free(palm_bmp);if(steer_bmp)aster_gfx_bmp_free(steer_bmp);if(brake_bmp)aster_gfx_bmp_free(brake_bmp);if(surface)aster_surface_destroy(surface);}



