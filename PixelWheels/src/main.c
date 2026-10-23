/* Pixel Wheels Aster adaptation, GPL-3.0-or-later.
 * Original game/art: Aure'lien Ga^teau and contributors; vendor/upstream.
 * Runtime uses bounded tile buffers and a C vehicle solver, not libGDX/Box2D.
 */
#include <aster_imports.h>
#include <aster_gfx_engine.h>
#include "audio.h"

typedef struct {const char *id,*name,*music;} TrackDef;
typedef struct {const char *id,*name;int w,h,offset;float speed;} CarDef;
#include "catalog.h"
#define RACERS 6
#define PI 3.14159265f
#define RESOURCE "/data/resources/"
typedef struct {char magic[4];uint32_t w,h,bgN,fgN,wayN,bonusN,cw,ch,bgids,fgids,mat,mask,way,bonus,bgpix,fgpix,total,flags;} Map;
typedef struct {float x,y;} Point;
typedef struct {float x,y,vx,vy,angle,turbo,stuck,progress;int car,gate,passed,lap,finished;float finish_time;} Racer;
typedef struct {uint32_t slot;int active,x,y,button;} Finger;
typedef struct {float x,y,angle,life;} Skid;
static uint8_t map_data[MAP_CAP] __attribute__((aligned(4)));
static uint8_t car_data[CAR_BYTES],logo_data[LOGO_W*LOGO_H*3],bonus_data[20*20*3];
static Map *map;
static uint16_t *bgids,*fgids;
static uint8_t *materials,*collision;
static Point *waypoints,*bonuspoints;
static Racer racers[RACERS];
static Finger fingers[10];
static Skid skids[180];static int skidhead;
static float bonus_cooldown[128];
static aster_handle_t surface=ASTER_HANDLE_INVALID,loadfile;
static aster_gfx_t pixels;
static uint16_t *frame;static int pitch,width,height;
static uint32_t loaded;
static int phase,selected_map,selected_car=11,dirty=1,auto_gas=1,key_left,key_right,key_gas,key_brake;
static int demo_drive; static int turbo_charges=1; static int key_action,action_latch,paused,buttons[5],menu_capture=-1,menu_cancel,menu_downx,menu_downy;
static uint32_t menu_slot;
static float camera_x,camera_y,race_time,countdown,accum,finish_timer;
static uint32_t last_clock;static int clock_valid;
static int beep_number=4; static float player_drift; static int player_material;

static uint32_t collision_clock;
static uint32_t perf_start,perf_frames,perf_draw_ms;
static uint32_t perf_sim,perf_audio,perf_bg,perf_cars,perf_fg,perf_ui,perf_present;
static void perf_reset(void){perf_start=perf_frames=perf_draw_ms=perf_sim=perf_audio=perf_bg=perf_cars=perf_fg=perf_ui=perf_present=0;}
typedef struct {uint8_t x0,y0,x1,y1,kind;} TileInfo;
static TileInfo fg_info[2048];
static uint16_t overlay_lut[2][65536];
static int overlay_ready;
static void prepare_overlays(void){
    if(overlay_ready)return;
    const uint16_t colors[2]={0x10c5,0x0862};const int alpha[2]={180,185};
    for(int j=0;j<2;j++)for(int v=0;v<65536;v++){
        int a=alpha[j],c=colors[j];
        int r=(((v>>11)&31)*(255-a)+((c>>11)&31)*a)/255;
        int g=(((v>>5)&63)*(255-a)+((c>>5)&63)*a)/255;
        int b=((v&31)*(255-a)+(c&31)*a)/255;
        overlay_lut[j][v]=(uint16_t)((r<<11)|(g<<5)|b);
    }
    overlay_ready=1;
}
static float waypoint_heading[256];
static int copy_mode=2,copy_profiled;
static void copy_pixels(uint16_t *dst,const uint16_t *src,int count){
    if(copy_mode==0){memcpy(dst,src,count*2);return;}
    if(copy_mode==1){int x=0;for(;x+7<count;x+=8){dst[x]=src[x];dst[x+1]=src[x+1];dst[x+2]=src[x+2];dst[x+3]=src[x+3];dst[x+4]=src[x+4];dst[x+5]=src[x+5];dst[x+6]=src[x+6];dst[x+7]=src[x+7];}for(;x<count;x++)dst[x]=src[x];return;}
    // All native 32-bit accesses below are aligned. For mismatched parity,
    // reconstruct a pair from two aligned source words without unaligned loads.
    if(((uintptr_t)dst&3)&&count){*dst++=*src++;count--;}
    uint32_t *d=(uint32_t*)dst;
    if(!((uintptr_t)src&3)){
        const uint32_t *v=(const uint32_t*)src;int n=count/2,j=0;
        for(;j+3<n;j+=4){d[j]=v[j];d[j+1]=v[j+1];d[j+2]=v[j+2];d[j+3]=v[j+3];}
        for(;j<n;j++)d[j]=v[j];
    }else{
        const uint32_t *v=(const uint32_t*)(src-1);int n=count/2;
        for(int j=0;j<n;j++)d[j]=(v[j]>>16)|(v[j+1]<<16);
    }
    if(count&1)dst[count-1]=src[count-1];
}
static char failure[100];
enum {LOADING,SELECT,COUNTDOWN,RACE,FINISHED,ERROR};
static float absf(float x){return x<0?-x:x;}
static float clamp(float v,float a,float b){return v<a?a:v>b?b:v;}
static float sq(float x){return x*x;}
static float root(float n){if(n<=0)return 0;float r=n>1?n:1;for(int i=0;i<12;i++)r=.5f*(r+n/r);return r;}
static float sn(float a){while(a<0)a+=256;while(a>=256)a-=256;int i=(int)a;float f=a-i;int index=i&255;return sin_table[index]+(sin_table[(index+1)&255]-sin_table[index])*f;}
static float cs(float a){return sn(a+64);}
static float wrap(float a){while(a<0)a+=256;while(a>=256)a-=256;return a;}
static float delta_angle(float a,float b){float d=a-b;while(d>128)d-=256;while(d<-128)d+=256;return d;}
static uint16_t mix(uint16_t dst,uint16_t src,int a){
    int rb=(((dst>>11)&31)*(255-a)+((src>>11)&31)*a)/255;
    int g=(((dst>>5)&63)*(255-a)+((src>>5)&63)*a)/255;
    int b=((dst&31)*(255-a)+(src&31)*a)/255;
    return (uint16_t)((rb<<11)|(g<<5)|b);
}
static uint16_t read16(const uint8_t *p){return (uint16_t)(p[0]|((uint16_t)p[1]<<8));}
static void rect(int x,int y,int w,int h,uint16_t color,int alpha){
    int x0=x<0?0:x,y0=y<0?0:y,x1=x+w>width?width:x+w,y1=y+h>height?height:y+h;
    if(x0>=x1||y0>=y1)return;
    if(alpha==255){for(int yy=y0;yy<y1;yy++){uint16_t *d=frame+yy*pitch;for(int xx=x0;xx<x1;xx++)d[xx]=color;}return;}
    const uint16_t *lut=color==0x10c5&&alpha==180?overlay_lut[0]:color==0x0862&&alpha==185?overlay_lut[1]:0;
    if(lut&&overlay_ready){for(int yy=y0;yy<y1;yy++){
        uint16_t *d=frame+yy*pitch;int xx=x0;
        for(;xx+3<x1;xx+=4){d[xx]=lut[d[xx]];d[xx+1]=lut[d[xx+1]];d[xx+2]=lut[d[xx+2]];d[xx+3]=lut[d[xx+3]];}
        for(;xx<x1;xx++)d[xx]=lut[d[xx]];
    }return;}
    // Same RGB565 /255 blend as mix(), factor channel arithmetic out of pixels.
    uint16_t rb[32],gg[64],bb[32];int inv=255-alpha;
    int r=((color>>11)&31)*alpha,g=((color>>5)&63)*alpha,b=(color&31)*alpha;
    for(int i=0;i<32;i++){rb[i]=(uint16_t)(((i*inv+r)/255)<<11);bb[i]=(uint16_t)((i*inv+b)/255);}
    for(int i=0;i<64;i++)gg[i]=(uint16_t)(((i*inv+g)/255)<<5);
    for(int yy=y0;yy<y1;yy++){uint16_t *d=frame+yy*pitch;for(int xx=x0;xx<x1;xx++){uint16_t v=d[xx];d[xx]=rb[v>>11]|gg[(v>>5)&63]|bb[v&31];}}
}
static void line(int x,int y,int xx,int yy,uint16_t c){aster_gfx_line(&pixels,x,y,xx,yy,c);}
static void label(int x,int y,const char *s,uint16_t color,int scale){
    if(scale==1){aster_gfx_text(&pixels,x,y,s,color,0,0);return;}
    uint16_t cell[64];aster_gfx_t glyph;aster_gfx_init(&glyph,cell,8,8,16);
    for(;*s;s++,x+=6*scale){
        memset(cell,0,sizeof(cell));aster_gfx_char(&glyph,0,0,*s,0xffff,0,0);
        for(int gy=0;gy<7;gy++)for(int gx=0;gx<5;gx++)if(cell[gy*8+gx])rect(x+gx*scale,y+gy*scale,scale,scale,color,255);
    }
}
static void center(int y,const char *s,uint16_t color,int scale){label((width-(int)strlen(s)*6*scale)/2,y,s,color,scale);}
static void image(const uint8_t *data,int w,int h,int x,int y,int scale){
    int n=w*h;const uint8_t *alpha=data+n*2;
    for(int sy=0;sy<h;sy++)for(int sx=0;sx<w;sx++){
        int at=sy*w+sx,a=alpha[at];if(a){int dx=x+sx*scale,dy=y+sy*scale;if(scale==1&&a==255){if(dx>=0&&dy>=0&&dx<width&&dy<height)frame[dy*pitch+dx]=read16(data+at*2);}else rect(dx,dy,scale,scale,read16(data+at*2),a);}
    }
}
static int read_small(const char *name,void *dst,uint32_t cap){
    char path[120];aster_snprintf(path,sizeof(path),RESOURCE "%s",name);
    aster_handle_t file=0;int rc=aster_fs_open(path,strlen(path),ASTER_FS_O_RDONLY,&file);if(rc<0)return rc;
    uint32_t n=0;while(n<cap){rc=aster_fs_read(file,(uint8_t*)dst+n,cap-n);if(rc<=0)break;n+=rc;}
    aster_fs_close(file);return n==cap?ASTER_OK:ASTER_EIO;
}
static void fail(const char *operation,int rc){
    aster_snprintf(failure,sizeof(failure),"%s (%d)",operation,rc);aster_errfmt("PixelWheels %s",failure);phase=ERROR;dirty=1;
    if(loadfile){aster_fs_close(loadfile);loadfile=0;}
}
static void inputs_clear(void){memset(fingers,0,sizeof(fingers));memset(buttons,0,sizeof(buttons));key_left=key_right=key_gas=key_brake=key_action=action_latch=0;menu_capture=-1;menu_cancel=1;}
static void audio_end(void){pw_audio_stop();}
static void audio_begin(void){pw_audio_begin(track_defs[selected_map].music);}
static void audio_tick(uint32_t clock){
    float speed=root(sq(racers[0].vx)+sq(racers[0].vy));
    pw_audio_tick(clock,speed,racers[0].turbo,player_drift,player_material,phase==RACE||phase==COUNTDOWN,paused);
}
static int map_valid(uint32_t n){
    if(n<sizeof(Map)||memcmp(map_data,"PWT1",4))return 0;
    Map *m=(Map*)map_data;
    if(m->total!=n||m->w<1||m->h<1||m->w>256||m->h>256||m->wayN<3||m->wayN>256||m->bonusN>128)return 0;
    if(m->cw!=m->w*8||m->ch!=m->h*8||m->bgN>2048||m->fgN>2048)return 0;
    uint32_t ofs[]={m->bgids,m->fgids,m->mat,m->mask,m->way,m->bonus,m->bgpix,m->fgpix};
    uint32_t len[]={m->w*m->h*2,m->w*m->h*2,m->w*m->h,m->cw*m->ch,m->wayN*8,m->bonusN*8,m->bgN*2048,m->fgN*3072};
    for(int i=0;i<8;i++)if(ofs[i]<sizeof(Map)||ofs[i]>n||len[i]>n-ofs[i])return 0;
    const uint16_t *b=(const uint16_t*)(map_data+m->bgids),*f=(const uint16_t*)(map_data+m->fgids);
    for(uint32_t i=0;i<m->w*m->h;i++)if(b[i]>=m->bgN||f[i]>=m->fgN)return 0;
    return 1;
}
static float heading(float dx,float dy);
static void map_bind(void){
    prepare_overlays();
    map=(Map*)map_data;bgids=(uint16_t*)(map_data+map->bgids);fgids=(uint16_t*)(map_data+map->fgids);
    materials=map_data+map->mat;collision=map_data+map->mask;
    waypoints=(Point*)(map_data+map->way);bonuspoints=(Point*)(map_data+map->bonus);
    for(uint32_t id=0;id<map->fgN;id++){
        const uint8_t *a=map_data+map->fgpix+id*3072+2048;
        TileInfo t={32,32,0,0,0};int opaque=0;
        for(int y=0;y<32;y++)for(int x=0;x<32;x++)if(a[y*32+x]){if(x<t.x0)t.x0=x;if(y<t.y0)t.y0=y;if(x+1>t.x1)t.x1=x+1;if(y+1>t.y1)t.y1=y+1;if(a[y*32+x]==255)opaque++;else t.kind=2;}
        if(t.x1){if(!t.kind)t.kind=opaque==1024?3:1;}fg_info[id]=t;
    }
    for(uint32_t j=0;j<map->wayN;j++){
        Point before=waypoints[(j+map->wayN-1)%map->wayN],at=waypoints[j],after=waypoints[(j+1)%map->wayN];
        waypoint_heading[j]=heading(after.x-at.x,after.y-at.y);(void)before;
    }
}
static void load_map(void){
    if(loadfile){aster_fs_close(loadfile);loadfile=0;}audio_end();inputs_clear();
    char path[120];aster_snprintf(path,sizeof(path),RESOURCE "%s.pwt",track_defs[selected_map].id);
    int rc=aster_fs_open(path,strlen(path),ASTER_FS_O_RDONLY,&loadfile);if(rc<0){fail("Missing map resources",rc);return;}
    loaded=0;phase=LOADING;dirty=1;clock_valid=0;
}
static void load_step(void){
    if(loaded==MAP_CAP){fail("Map capacity exceeded",ASTER_ENOMEM);return;}
    uint32_t n=MAP_CAP-loaded;if(n>65536)n=65536;
    int rc=aster_fs_read(loadfile,map_data+loaded,n);
    if(rc<0){fail("Read map",rc);return;}
    loaded+=rc;
    if(rc==0){
        aster_fs_close(loadfile);loadfile=0;
        if(!map_valid(loaded)){fail("Invalid map data",ASTER_EINVAL);return;}
        map_bind();phase=SELECT;
        aster_printf("map %s loaded %u bytes %ux%u waypoints=%u",track_defs[selected_map].id,loaded,map->w,map->h,map->wayN);
    }
    dirty=1;
}
static int solid(float x,float y,float radius){
    for(int i=0;i<5;i++){
        float xx=x+(i==1?radius:i==2?-radius:0),yy=y+(i==3?radius:i==4?-radius:0);
        int gx=(int)(xx/4),gy=(int)(yy/4);
        if(xx<0||yy<0||gx>=(int)map->cw||gy>=(int)map->ch||collision[gy*map->cw+gx])return 1;
    }return 0;
}
static int material_at(float x,float y){int tx=(int)(x/32),ty=(int)(y/32);if(tx<0||ty<0||tx>=(int)map->w||ty>=(int)map->h)return 4;return materials[ty*map->w+tx];}
static float heading(float dx,float dy){
    if(dx==0&&dy==0)return 0;
    int lo=dx>=0?(dy>=0?0:192):(dy>=0?64:128),hi=lo+64;
    // Sine table directions are ordered within a quadrant; keep exact 256 bins.
    while(hi-lo>1){int m=(lo+hi)/2;float cross=sin_table[(m+64)&255]*dy-sin_table[m&255]*dx;if(cross>0)lo=m;else hi=m;}
    float a=dx*sin_table[(lo+64)&255]+dy*sin_table[lo&255];
    float b=dx*sin_table[(hi+64)&255]+dy*sin_table[hi&255];
    return (float)((a>=b?lo:hi)&255);
}
static void start_race(void){
    inputs_clear();memset(racers,0,sizeof(racers));memset(skids,0,sizeof(skids));memset(bonus_cooldown,0,sizeof(bonus_cooldown));
    Point p=waypoints[0],last=waypoints[map->wayN-1];
    float angle=heading(p.x-last.x,p.y-last.y),fx=cs(angle),fy=sn(angle),side_x=-fy,side_y=fx;
    for(int i=0;i<RACERS;i++){
        Racer *r=&racers[i];int row=(RACERS-1-i)/2;float lane=i%2?22:-22;
        r->x=p.x-fx*(40+row*50)+side_x*lane;r->y=p.y-fy*(40+row*50)+side_y*lane;
        if(solid(r->x,r->y,7)){r->x=p.x-fx*(30+i*25);r->y=p.y-fy*(30+i*25);}
        r->angle=angle;r->car=i==0?selected_car:(selected_car+i*2+3)%CAR_COUNT;r->gate=0;
    }
    camera_x=racers[0].x;camera_y=racers[0].y;
    player_drift=0;player_material=0;perf_reset();turbo_charges=1;phase=COUNTDOWN;countdown=3;race_time=0;finish_timer=0;accum=0;paused=0;beep_number=4;clock_valid=0;
    audio_begin();dirty=1;aster_printf("race start map=%s car=%s",track_defs[selected_map].id,car_defs[selected_car].id);
}
static void vehicle_step(Racer *r,int index,float dt,float steering,int gas,int brake){
    float speed=root(r->vx*r->vx+r->vy*r->vy),vmax=420*car_defs[r->car].speed;
    int mat=material_at(r->x,r->y);
    static const float material_speed[]={1,1.5f,.6f,.5f,0,.3f,0,.3f};
    if(r->turbo>0){r->turbo-=dt;vmax*=1.45f;}if(mat==1){if(index==0&&r->turbo<=0)pw_audio_event(PW_TURBO);r->turbo=.3f;}
    if(index>0)vmax*=.88f+index*.016f;
    float limit=vmax*material_speed[mat];
    float turn=(110-36*clamp(speed/vmax,0,1))*steering*clamp(speed/55,.5f,1);
    r->angle=wrap(r->angle+turn*dt);
    float fx=cs(r->angle),fy=sn(r->angle),forward=r->vx*fx+r->vy*fy;
    float lateral=-r->vx*fy+r->vy*fx;
    float grip=mat==7?1.3f:brake?3.3f:10.0f;
    lateral*=clamp(1-grip*dt,0,1);
    if(gas&&!brake)forward+=220*dt;
    if(brake&&forward<18)forward-=140*dt;
    forward*=clamp(1-(brake&&forward>18?4.8f:gas?.25f:1.0f)*dt,0,1);
    if(forward<-70)forward=-70;
    if(forward>limit)forward-=clamp((forward-limit)*dt*5,0,forward-limit);
    r->vx=fx*forward-fy*lateral;r->vy=fy*forward+fx*lateral;
    float nx=r->x+r->vx*dt,ny=r->y+r->vy*dt;
    if(!solid(nx,r->y,6))r->x=nx;else r->vx*=-.23f;
    if(!solid(r->x,ny,6))r->y=ny;else r->vy*=-.23f;
    if(index==0){player_material=mat;int drifting=(brake&&speed>30)||(speed>70&&absf(lateral)>20);player_drift=drifting?player_drift+dt:0;}
    if(index==0 && speed>70 && absf(lateral)>20){Skid *s=&skids[skidhead++%180];*s=(Skid){r->x,r->y,r->angle,2.5f};}
    if(index==0 && (absf(r->vx)+absf(r->vy)<speed*.45f) && speed>90){uint32_t now=aster_time_now_ms();if(now-collision_clock>350){pw_audio_event(PW_IMPACT);collision_clock=now;}}
    Point target=waypoints[r->gate];float d2=sq(r->x-target.x)+sq(r->y-target.y);
    int previous=(r->gate+map->wayN-1)%map->wayN;Point before=waypoints[previous];
    float dot=(r->x-target.x)*(target.x-before.x)+(r->y-target.y)*(target.y-before.y);
    // Ordered gates prevent counting reverse driving or jumping straight to the finish.
    if(d2<sq(index||demo_drive?18:42) || (dot>0 && d2<sq(index||demo_drive?55:150))){
        int completed=r->gate;r->gate=(r->gate+1)%map->wayN;r->passed++;
        if(completed==0 && r->passed>1){r->lap++;if(index==0){if(r->lap<3)pw_audio_event(PW_LAP);aster_printf("lap %d time=%u",r->lap,(uint32_t)(race_time*1000));}}
        if(r->lap>=3 && !r->finished){r->finished=1;r->finish_time=race_time;if(index==0){phase=FINISHED;pw_audio_finish();finish_timer=0;aster_printf("race finished %u ms",(uint32_t)(race_time*1000));}}
    }
    r->progress=(float)r->passed+1.0f/(1+root(d2)/100);
    if(mat==4||mat==6){r->stuck+=dt;}else if((index||demo_drive) && speed<25)r->stuck+=dt;else r->stuck=0;
    if(r->stuck>2.5f){r->x=before.x;r->y=before.y;r->vx=r->vy=0;r->angle=heading(target.x-before.x,target.y-before.y);r->stuck=0;}
}
static void simulate(float dt){
    if(phase==COUNTDOWN){countdown-=dt;if(countdown<=0){phase=RACE;perf_reset();pw_audio_event(PW_GO);}else{int v=(int)countdown+1;if(v!=beep_number){pw_audio_event(PW_COUNT);beep_number=v;}}return;}
    if(phase!=RACE)return;
    race_time+=dt;
    for(uint32_t j=0;j<map->bonusN;j++)if(bonus_cooldown[j]>0)bonus_cooldown[j]-=dt;
    for(int i=0;i<180;i++)if(skids[i].life>0)skids[i].life-=dt;
    for(int i=0;i<RACERS;i++){
        Racer *r=&racers[i];if(r->finished)continue;
        float steering=0;int gas=1,brake=0;
        if(i==0&&!demo_drive){steering=(float)((key_right||buttons[1])-(key_left||buttons[0]));gas=auto_gas||key_gas||buttons[3];brake=key_brake||buttons[2];}
        else{
            Point target=waypoints[r->gate];float target_angle=heading(target.x-r->x,target.y-r->y);
            float diff=delta_angle(target_angle,r->angle);steering=clamp(diff/22,-1,1);
            float speed=root(r->vx*r->vx+r->vy*r->vy);if(absf(diff)>20 && speed>105)brake=1;

            float bend=absf(delta_angle(waypoint_heading[r->gate],target_angle));
            float distance=root(sq(target.x-r->x)+sq(target.y-r->y));
            if(distance<130 && bend>22 && speed>105)brake=1;
            // Avoid cutting over a wall when the previous turn left us off the ideal line.
            float probe=clamp(speed*.3f,22,70);
            if(solid(r->x+cs(target_angle)*probe,r->y+sn(target_angle)*probe,7)){
                float best=-1e30f,best_angle=target_angle;
                for(int k=-3;k<=3;k++){
                    float a=wrap(target_angle+k*12);
                    if(solid(r->x+cs(a)*probe,r->y+sn(a)*probe,7))continue;
                    float score=cs(a-target_angle)*100-absf(delta_angle(a,r->angle))*.2f;
                    if(score>best){best=score;best_angle=a;}
                }
                steering=clamp(delta_angle(best_angle,r->angle)/22,-1,1);
                if(speed>110)brake=1;
            }
        }
        vehicle_step(r,i,dt,steering,gas,brake);
        for(uint32_t j=0;j<map->bonusN;j++)if(bonus_cooldown[j]<=0 && sq(r->x-bonuspoints[j].x)+sq(r->y-bonuspoints[j].y)<sq(23)){if(i==0){if(turbo_charges<3)turbo_charges++;pw_audio_event(PW_PICKUP);}else r->turbo=1.5f;bonus_cooldown[j]=5;}
    }
    for(int i=0;i<RACERS;i++)for(int j=i+1;j<RACERS;j++){
        Racer *a=&racers[i],*b=&racers[j];float dx=a->x-b->x,dy=a->y-b->y,d2=dx*dx+dy*dy;
        if(d2<225 && d2>.001f){float d=root(d2),push=(15-d)*.5f;float nx=dx/d,ny=dy/d;
            float ax=a->x+nx*push,ay=a->y+ny*push,bx=b->x-nx*push,by=b->y-ny*push;
            if(!solid(ax,ay,6)){a->x=ax;a->y=ay;}if(!solid(bx,by,6)){b->x=bx;b->y=by;}
            float rv=(a->vx-b->vx)*nx+(a->vy-b->vy)*ny;
            if(rv<0){if((i==0||j==0)&&rv<-35){pw_audio_event(PW_IMPACT);}a->vx-=rv*.6f*nx;a->vy-=rv*.6f*ny;b->vx+=rv*.6f*nx;b->vy+=rv*.6f*ny;}
        }
    }
    int action=key_action||buttons[4];if(action&&!action_latch&&turbo_charges>0){racers[0].turbo=1.5f;turbo_charges--;pw_audio_event(PW_TURBO);}action_latch=action;
}
static void tile(int id,int x,int y,int foreground){
    int x0=x<0?-x:0,y0=y<0?-y:0,x1=x+32>width?width-x:32,y1=y+32>height?height-y:32;
    int kind=3;
    if(foreground){TileInfo t=fg_info[id];kind=t.kind;if(!kind)return;if(x0<t.x0)x0=t.x0;if(y0<t.y0)y0=t.y0;if(x1>t.x1)x1=t.x1;if(y1>t.y1)y1=t.y1;}
    if(x0>=x1||y0>=y1)return;
    const uint8_t *src=map_data+(foreground?map->fgpix:map->bgpix)+id*(foreground?3072:2048);
    for(int yy=y0;yy<y1;yy++){
        uint16_t *dst=frame+(y+yy)*pitch+x;
        if(kind==3)copy_pixels(dst+x0,(const uint16_t*)(src+yy*64)+x0,x1-x0);
        else for(int xx=x0;xx<x1;xx++){int k=yy*32+xx,a=src[2048+k];if(a)dst[xx]=a==255?read16(src+k*2):mix(dst[xx],read16(src+k*2),a);}
    }
}
static void map_layer(int cx,int cy,int foreground){
    int left=cx-width/2,top=cy-height/2;
    for(int ty=top/32-1;ty<=(top+height)/32;ty++)for(int tx=left/32-1;tx<=(left+width)/32;tx++){
        if(tx<0||ty<0||tx>=(int)map->w||ty>=(int)map->h)continue;
        int at=ty*map->w+tx;tile(foreground?fgids[at]:bgids[at],tx*32-left,ty*32-top,foreground);
    }
}
static void profile_background(void){
    if(copy_profiled)return;
    // Compare only the APP background-copy paths on the actual Host at startup.
    // No presentation or extra resource allocation; the menu overwrites this buffer.
    uint32_t best=0xffffffffu,times[3];int winner=0;
    for(int mode=0;mode<3;mode++){
        copy_mode=mode;uint32_t elapsed=0;
        for(int k=0;k<4;k++){
            uint32_t at=aster_time_now_ms();
            // Exercise both source/destination parities, representative of scrolling.
            map_layer(width/2+k,height/2+k,0);
            elapsed+=aster_time_now_ms()-at;
        }
        times[mode]=elapsed;if(elapsed<best){best=elapsed;winner=mode;}
    }
    copy_mode=winner;copy_profiled=1;
    aster_printf("PW COPY memcpy_ms=%u halfword_ms=%u word_ms=%u selected=%d",times[0],times[1],times[2],winner);
}
static void sprite_car(int ci,float x,float y,float angle,int scale){
    const CarDef *c=&car_defs[ci];const uint8_t *src=car_data+c->offset;int n=c->w*c->h;
    float co=cs(angle+64),si=sn(angle+64);
    int bx=(int)((absf(co)*c->w+absf(si)*c->h)*scale*.5f)+2;
    int by=(int)((absf(si)*c->w+absf(co)*c->h)*scale*.5f)+2;
    int px=(int)x,py=(int)y,x0=-bx,y0=-by,x1=bx,y1=by;
    if(px+x0<0)x0=-px;if(py+y0<0)y0=-py;if(px+x1>=width)x1=width-1-px;if(py+y1>=height)y1=height-1-py;
    int ic=(int)(co*65536/scale),is=(int)(si*65536/scale);
    for(int yy=y0;yy<=y1;yy++){
        int u=x0*ic+yy*is+c->w*32768,v=-x0*is+yy*ic+c->h*32768;
        uint16_t *d=frame+(py+yy)*pitch+px;
        for(int xx=x0;xx<=x1;xx++,u+=ic,v-=is){
            int sx=u/65536,sy=v/65536;if(sx<0||sy<0||sx>=c->w||sy>=c->h)continue;
            int at=sy*c->w+sx,a=src[n*2+at];if(a)d[xx]=a==255?read16(src+at*2):mix(d[xx],read16(src+at*2),a);
        }
    }
}
static void overview(int x,int y,int w,int h){
    for(int dy=0;dy<h;dy++)for(int dx=0;dx<w;dx++){
        int wx=dx*(int)(map->w*32)/w,wy=dy*(int)(map->h*32)/h,cell=wy/32*map->w+wx/32,k=wy%32*32+wx%32;
        const uint8_t *b=map_data+map->bgpix+bgids[cell]*2048+k*2,*f=map_data+map->fgpix+fgids[cell]*3072;
        frame[(y+dy)*pitch+x+dx]=mix(read16(b),read16(f+k*2),f[2048+k]);
    }
}
static void button(int x,int y,int w,int h,const char *name,int down){
    rect(x,y,w,h,down?0x4e9d:0x10c5,180);line(x,y,x+w-1,y,0x7bd0);line(x,y+h-1,x+w-1,y+h-1,0x4a69);
    label(x+(w-(int)strlen(name)*6)/2,y+(h-7)/2,name,0xffff,1);
}
static void menu_render(void){
    rect(0,0,width,height,0x10a3,255);
    for(int i=0;i<height;i+=8)line(0,i,width-1,i,0x10c4);
    image(logo_data,LOGO_W,LOGO_H,18,2,1);
    label(282,28,"ASTER EDITION",0x9f93,1);label(282,44,"9 TRACKS / 15 CARS",0x9cf3,1);
    rect(16,112,250,148,0x2948,255);overview(20,116,242,140);
    char b[100];aster_snprintf(b,sizeof(b),"%d/9  %s",selected_map+1,track_defs[selected_map].name);
    label(20,104,b,0xffff,1);
    button(18,214,35,40,"<",0);button(228,214,35,40,">",0);
    rect(282,96,180,164,0x1926,255);
    label(294,106,"CHOOSE YOUR RIDE",0xaed9,1);
    sprite_car(selected_car,372,174,-64,3);
    center(0,"",0,1);
    const char *name=car_defs[selected_car].name;label(372-(int)strlen(name)*3,232,name,0xffff,1);
    button(288,154,29,43,"<",0);button(428,154,29,43,">",0);
    button(156,270,168,38,"START RACE",0);button(374,66,88,24,"DEMO DRIVE",0);
    label(18,286,"< > TRACK",0x7bef,1);label(350,286,"UP/DOWN CAR",0x7bef,1);
    center(312,"ENTER START / ESC MENU / SPACE TURBO",0x7bef,1);
}
static void render(void){
    uint32_t draw_at=aster_time_now_ms();
    aster_surface_buffer_t b={.size=sizeof(b),.version=1};int rc=aster_surface_acquire(surface,&b,sizeof(b));
    if(rc<0||!b.ptr||b.format!=ASTER_PIXEL_RGB565){fail("Acquire surface",rc<0?rc:ASTER_ENOTSUP);return;}
    frame=(uint16_t*)b.ptr;pitch=b.pitch/2;aster_gfx_init(&pixels,b.ptr,b.width,b.height,b.pitch);
    if(phase==LOADING||phase==ERROR||phase==SELECT||map->w*32<(uint32_t)width||map->h*32<(uint32_t)height)aster_gfx_clear(&pixels,0x10a3);
    if(phase==LOADING){image(logo_data,LOGO_W,LOGO_H,128,40,1);center(180,"LOADING ORIGINAL TRACK...",0xffff,1);rect(90,208,(int)(loaded*300/MAP_CAP),6,0x87f0,255);}
    else if(phase==ERROR){center(100,"RESOURCE ERROR",0xfaa8,2);center(150,failure,0xffff,1);center(185,"RUN tools/build.ps1 AND USE run-sim.ps1",0xbdf7,1);}
    else if(phase==SELECT){profile_background();menu_render();}
    else{
        int cx=(int)camera_x,cy=(int)camera_y,ox=width/2-cx,oy=height/2-cy;
        uint32_t part=aster_time_now_ms();
        map_layer(cx,cy,0);perf_bg+=aster_time_now_ms()-part;part=aster_time_now_ms();
        for(int i=0;i<180;i++)if(skids[i].life>0){Skid *s=&skids[i];float dx=cs(s->angle)*7,dy=sn(s->angle)*7;
            line((int)(s->x-dx)+ox,(int)(s->y-dy)+oy,(int)(s->x+dx)+ox,(int)(s->y+dy)+oy,0x3186);
        }
        for(uint32_t j=0;j<map->bonusN;j++)if(bonus_cooldown[j]<=0)image(bonus_data,20,20,(int)bonuspoints[j].x+ox-10,(int)bonuspoints[j].y+oy-10,1);
        for(int i=RACERS-1;i>=0;i--){Racer *r=&racers[i];if(r->x+ox<-60||r->y+oy<-60||r->x+ox>width+60||r->y+oy>height+60)continue;
            rect((int)r->x+ox-7,(int)r->y+oy-10,17,25,0x1082,100);
            if(r->turbo>0){float fx=cs(r->angle),fy=sn(r->angle);line((int)(r->x-fx*15)+ox,(int)(r->y-fy*15)+oy,(int)(r->x-fx*35)+ox,(int)(r->y-fy*35)+oy,0xfd60);}
            sprite_car(r->car,r->x+ox,r->y+oy,r->angle,1);
            if(i==0){label((int)r->x+ox-9,(int)r->y+oy-27,"YOU",0xffe0,1);}
        }
        perf_cars+=aster_time_now_ms()-part;part=aster_time_now_ms();map_layer(cx,cy,1);perf_fg+=aster_time_now_ms()-part;part=aster_time_now_ms();
        char s[100];int rank=1;for(int i=1;i<RACERS;i++)if(racers[i].progress>racers[0].progress)rank++;
        rect(0,0,width,32,0x0862,185);
        aster_snprintf(s,sizeof(s),"%d/6",rank);label(14,8,s,0xffe0,2);
        aster_snprintf(s,sizeof(s),"LAP %d/3",racers[0].lap>=3?3:racers[0].lap+1);label(92,10,s,0xffff,1);
        aster_snprintf(s,sizeof(s),"%u.%us",(uint32_t)race_time,(uint32_t)(race_time*10)%10);label(204,10,s,0xffff,1);
        int speed=(int)(root(sq(racers[0].vx)+sq(racers[0].vy))*.36f);
        aster_snprintf(s,sizeof(s),"%d KM/H",speed);label(294,10,s,0x9f93,1);
        button(409,3,64,26,"MENU",paused);
        button(8,height-72,76,64,"<",key_left||buttons[0]);button(94,height-72,76,64,">",key_right||buttons[1]);
        button(390,height-72,82,64,"BRAKE",key_brake||buttons[2]);
        if(!auto_gas)button(300,height-72,82,64,"GAS",key_gas||buttons[3]);
        aster_snprintf(s,sizeof(s),"TURBO x%d",turbo_charges);button(390,height-130,82,48,s,key_action||buttons[4]);button(8,42,92,28,auto_gas?"AUTO GAS":"MANUAL GAS",0);button(108,42,64,28,"RESET",0);
        if(demo_drive)label(184,52,"DEMO / A TO DRIVE",0xffe0,1);
        // Tiny route overview, kept out of the steering area.
        int mx=width-98,my=43;rect(mx-4,my-4,92,58,0x0862,150);
        for(uint32_t j=0;j<map->wayN;j++){Point p=waypoints[j],q=waypoints[(j+1)%map->wayN];line(mx+(int)(p.x*82/(map->w*32)),my+(int)(p.y*48/(map->h*32)),mx+(int)(q.x*82/(map->w*32)),my+(int)(q.y*48/(map->h*32)),0x9cf3);}
        rect(mx+(int)(racers[0].x*82/(map->w*32))-2,my+(int)(racers[0].y*48/(map->h*32))-2,4,4,0xffe0,255);
        if(phase==COUNTDOWN){aster_snprintf(s,sizeof(s),"%d",(int)countdown+1);center(height/2-18,s,0xffff,5);}
        else if(phase==RACE && race_time<.7f)center(height/2-22,"GO!",0x87f0,4);
        perf_ui+=aster_time_now_ms()-part;
        if(paused||phase==FINISHED){
            rect(96,70,288,176,0x0862,235);center(84,phase==FINISHED?"FINISH!":"PAUSED",0xffe0,3);
            if(phase==FINISHED){aster_snprintf(s,sizeof(s),"TIME %u.%us  PLACE %d/6",(uint32_t)race_time,(uint32_t)(race_time*10)%10,rank);center(118,s,0xffff,1);}
            button(126,140,228,30,phase==FINISHED?"RACE AGAIN":"RESUME",0);
            button(126,177,228,30,"TRACK & CAR SELECT",0);
            center(220,"ENTER RESUME / ESC SELECT",0x9cf3,1);
        }
    }
    uint32_t present_at=aster_time_now_ms();rc=aster_surface_present(surface);perf_present+=aster_time_now_ms()-present_at;if(rc<0){aster_errfmt("present %d",rc);aster_app_exit(rc);return;}dirty=0;
    if(phase==RACE&&!paused){
        uint32_t end=aster_time_now_ms();if(!perf_frames)perf_start=draw_at;
        perf_frames++;perf_draw_ms+=end-draw_at;
        if(perf_frames==60){uint32_t elapsed=end-perf_start;
            aster_printf("PW PERF frames=60 elapsed_ms=%u draw_ms=%u map=%s demo=%d",elapsed,perf_draw_ms,track_defs[selected_map].id,demo_drive);
            aster_printf("PW COST sim=%u audio=%u bg=%u cars=%u fg=%u ui=%u present=%u",perf_sim,perf_audio,perf_bg,perf_cars,perf_fg,perf_ui,perf_present);
            perf_frames=perf_draw_ms=perf_sim=perf_audio=perf_bg=perf_cars=perf_fg=perf_ui=perf_present=0;
        }
    }
}
static int control_at(int x,int y){
    if(x>=8&&x<84&&y>=height-72&&y<height-8)return 0;
    if(x>=94&&x<170&&y>=height-72&&y<height-8)return 1;
    if(x>=390&&x<472&&y>=height-72&&y<height-8)return 2;
    if(!auto_gas&&x>=300&&x<382&&y>=height-72&&y<height-8)return 3;
    if(x>=390&&x<472&&y>=height-130&&y<height-82)return 4;
    return -1;
}
static int menu_at(int x,int y){
    if(phase==SELECT){
        if(x>=156&&x<324&&y>=270&&y<308)return 1;
        if(x>=374&&x<462&&y>=66&&y<90)return 10;
        if(x>=18&&x<53&&y>=214&&y<254)return 2;
        if(x>=228&&x<263&&y>=214&&y<254)return 3;
        if(x>=288&&x<317&&y>=154&&y<197)return 4;
        if(x>=428&&x<457&&y>=154&&y<197)return 5;
    }else if(paused||phase==FINISHED){
        if(x>=126&&x<354&&y>=140&&y<170)return 6;
        if(x>=126&&x<354&&y>=177&&y<207)return 7;
    }else if(phase==RACE||phase==COUNTDOWN){
        if(x>=409&&x<473&&y>=3&&y<29)return 8;
        if(x>=8&&x<100&&y>=42&&y<70)return 9;
        if(x>=108&&x<172&&y>=42&&y<70)return 11;
    }return -1;
}
static void choose_map(int delta){selected_map=(selected_map+delta+TRACK_COUNT)%TRACK_COUNT;load_map();}
static void choose_car(int delta){selected_car=(selected_car+delta+CAR_COUNT)%CAR_COUNT;dirty=1;}
static void menu_action(int id){
    if(id==1){demo_drive=0;start_race();}else if(id==2)choose_map(-1);else if(id==3)choose_map(1);
    else if(id==4)choose_car(-1);else if(id==5)choose_car(1);
    else if(id==6){if(phase==FINISHED)start_race();else{paused=0;inputs_clear();clock_valid=0;pw_audio_resume();}}
    else if(id==7){audio_end();phase=SELECT;paused=0;inputs_clear();dirty=1;}
    else if(id==8){paused=1;inputs_clear();pw_audio_pause();dirty=1;}
    else if(id==11){start_race();}
    else if(id==10){demo_drive=1;start_race();}
    else if(id==9){auto_gas=!auto_gas;inputs_clear();dirty=1;}
}
static void touch(const aster_event_t *e){
    int action=e->u.touch.action,x=e->u.touch.x,y=e->u.touch.y;uint32_t slot=e->u.touch.slot;
    if(phase==LOADING||phase==ERROR)return;
    int i=-1,free=-1;for(int j=0;j<10;j++){if(fingers[j].active&&fingers[j].slot==slot)i=j;if(!fingers[j].active&&free<0)free=j;}
    if(action==ASTER_TOUCH_DOWN){
        if(i<0)i=free;if(i<0)return;
        fingers[i]=(Finger){slot,1,x,y,-1};int m=menu_at(x,y);
        if(m>0 && menu_capture<0){menu_capture=m;menu_slot=slot;menu_cancel=0;menu_downx=x;menu_downy=y;}
    }
    if(i<0)return;
    fingers[i].x=x;fingers[i].y=y;
    if(slot==menu_slot&&menu_capture>0&&(absf(x-menu_downx)>12||absf(y-menu_downy)>12))menu_cancel=1;
    if(action==ASTER_TOUCH_UP){
        fingers[i].active=0;
        if(slot==menu_slot&&menu_capture>0){int id=menu_capture;menu_capture=-1;if(!menu_cancel&&menu_at(x,y)==id){menu_action(id);return;}}
    }
    memset(buttons,0,sizeof(buttons));
    if(!paused&&(phase==RACE||phase==COUNTDOWN))for(int j=0;j<10;j++)if(fingers[j].active&&!(fingers[j].slot==menu_slot&&menu_capture>0)){
        int b=control_at(fingers[j].x,fingers[j].y);if(b>=0)buttons[b]=1;
    }
    dirty=1;
}
static void key(const aster_event_t *e){
    int c=e->u.key.code,on=e->u.key.pressed;
    if(phase==LOADING||phase==ERROR)return;
    if(phase==SELECT){if(on){if(c==105)choose_map(-1);else if(c==106)choose_map(1);else if(c==103)choose_car(-1);else if(c==108)choose_car(1);else if(c==28){demo_drive=0;start_race();}else if(c==30){demo_drive=1;start_race();}}return;}
    if(on&&c==1){if(paused||phase==FINISHED)menu_action(7);else menu_action(8);return;}
    if(on&&c==46){Racer *r=&racers[0];aster_printf("drive x=%d y=%d vx=%d vy=%d angle=%d gate=%d passed=%d mat=%d blocked=%d demo=%d",(int)r->x,(int)r->y,(int)r->vx,(int)r->vy,(int)r->angle,r->gate,r->passed,material_at(r->x,r->y),solid(r->x,r->y,6),demo_drive);return;}
    if(on&&c==19){start_race();return;}
    if(paused||phase==FINISHED){if(on&&c==28)menu_action(6);return;}
    if(on&&c==30){demo_drive=!demo_drive;dirty=1;return;}
    if(c==105)key_left=on;if(c==106)key_right=on;if(c==103||c==29)key_gas=on;if(c==108||c==56)key_brake=on;if(c==57)key_action=on;
}
ASTER_APP_EXPORT int aster_app_init(void){
    aster_display_info_t d={.size=sizeof(d),.version=1};int rc=aster_display_get_info(0,&d,sizeof(d));if(rc<0)return rc;
    width=d.app_width;height=d.app_height;if(width!=480||height!=320)return ASTER_ENOTSUP;
    aster_surface_desc_t desc={.size=sizeof(desc),.version=1,.width=width,.height=height,.format=ASTER_PIXEL_RGB565,.flags=0};
    rc=aster_surface_create(&desc,sizeof(desc),&surface);if(rc<0)return rc;
    rc=read_small("cars.bin",car_data,sizeof(car_data));if(rc<0){fail("Load cars",rc);return 0;}
    rc=read_small("title.bin",logo_data,sizeof(logo_data));if(rc<0){fail("Load title",rc);return 0;}
    rc=read_small("bonus.bin",bonus_data,sizeof(bonus_data));if(rc<0){fail("Load bonus",rc);return 0;}
    rc=aster_app_set_framerate(30);if(rc<0)return rc;
    load_map();aster_printf("PixelWheels init: 9 original tracks, 15 cars, map_cap=%u",(uint32_t)MAP_CAP);return ASTER_OK;
}
ASTER_APP_EXPORT void aster_app_event(const aster_event_t *e){
    if(!e||surface==ASTER_HANDLE_INVALID)return;
    if(e->type==ASTER_EVENT_TOUCH)touch(e);
    else if(e->type==ASTER_EVENT_KEY)key(e);
    else if(e->type==ASTER_EVENT_FRAME){
        uint32_t now=aster_time_now_ms();float dt=clock_valid?(float)(uint32_t)(now-last_clock)*.001f:0;last_clock=now;clock_valid=1;dt=clamp(dt,0,.1f);
        int fonts_changed=aster_gfx_fonts_pending();(void)fonts_changed;
        if(phase==LOADING)load_step();
        else if(!paused&&(phase==RACE||phase==COUNTDOWN)){
            uint32_t sim_at=aster_time_now_ms();accum+=dt;int steps=0;while(accum>=1.0f/60&&steps<6){simulate(1.0f/60);accum-=1.0f/60;steps++;}
            float tx=racers[0].x+cs(racers[0].angle)*45,ty=racers[0].y+sn(racers[0].angle)*45;
            camera_x+=(tx-camera_x)*clamp(dt*7,0,1);camera_y+=(ty-camera_y)*clamp(dt*7,0,1);
            camera_x=clamp(camera_x,width*.5f,map->w*32-width*.5f);camera_y=clamp(camera_y,height*.5f,map->h*32-height*.5f);perf_sim+=aster_time_now_ms()-sim_at;
            dirty=1;
        }
        uint32_t audio_at=aster_time_now_ms();audio_tick(now);perf_audio+=aster_time_now_ms()-audio_at;
        if(dirty)render();
    }
}
ASTER_APP_EXPORT void aster_app_pause(void){inputs_clear();clock_valid=0;if(phase==RACE||phase==COUNTDOWN)paused=1;pw_audio_pause();}
ASTER_APP_EXPORT void aster_app_resume(void){inputs_clear();clock_valid=0;dirty=1;}
static void release(void){
    inputs_clear();pw_audio_release();
    if(loadfile){aster_fs_close(loadfile);loadfile=0;}if(surface!=ASTER_HANDLE_INVALID){aster_surface_destroy(surface);surface=ASTER_HANDLE_INVALID;}
}
ASTER_APP_EXPORT void aster_app_stop(void){release();}
ASTER_APP_EXPORT void aster_app_destroy(void){release();}
