/* GPL-3.0-or-later.
 * Acceleration/gear/steering/centrifugal formulas from PlayerCar.cpp;
 * 60 Hz world motion from Map::updateMap, terrain and map-tree progression.
 */
#include "engine.h"
#include "catalog.h"
Game game;Traffic cars[20];static int beep_second=-1;
static uint32_t rand_state=2463534242u;
static float random01(void){rand_state^=rand_state<<13;rand_state^=rand_state>>17;rand_state^=rand_state<<5;return (rand_state&65535)/65535.f;}
void clear_input(void){game.steer=game.touch_steer=0;game.brake=game.gas=game.touch_brake=game.touch_gas=false;}
static void traffic_reset(void){
 static const float z[]={190,170,165,160,155,150},offset[]={.5f,0,-.5f,-.25f,0,.25f};
 for(int i=0;i<20;i++){
  cars[i].z=i<6?z[i]*SEGMENT:(600+i*93)*SEGMENT;
  cars[i].offset=i<6?offset[i]:(random01()-.5f)*1.2f;
  cars[i].speed=i<6?120:100+(i%7)*10;cars[i].model=i%TRAFFIC_MODELS;cars[i].side=i<6?1:i%2;cars[i].target=cars[i].offset;cars[i].passed=0;
 }
}
void game_reset_map(uint32_t id){
 id=CLAMP(id,1,15);audio_end();beep_second=-1;clear_input();memset(&game,0,sizeof(game));
 game.map_id=id;game.stage=1;game.position=300*SEGMENT;game.auto_gas=true;game.body_frame=1;game.countdown=3;
 if(load_map(id)<0){game.state=-1;return;}
 game.remaining=id==1?160:map.h.time;
 while(game.stage<5&&id>game.stage*(game.stage+1)/2)game.stage++;
 game.branch=id-game.stage*(game.stage-1)/2-1;traffic_reset();
}
void game_reset(void){game_reset_map(1);}
void game_select(int delta){game_reset_map((game.map_id+14+delta)%15+1);}
/* Source engine's wheel-in-road limits, including separated dual carriageways. */
static bool on_road(float x,float distance){
 return (x>=-1831&&x<=1831)||(x>=distance-1831&&x<=distance+1831);
}
static void animation(void){
 if(game.steps%(game.crash>0&&game.crash_phase==1?7:3))return;
 int first,last;
 if(game.crash>0){if(game.crash_phase==0){first=121;last=128;}else if(game.crash_phase==1){first=129;last=133;}else {first=1;last=4;}}
 else {
  int base=(game.brake?60:0)+game.elevation*20;
  if(game.steer<0){first=base+(game.prev_steer<0?9:5);last=base+12;}
  else if(game.steer>0){first=base+(game.prev_steer>0?17:13);last=base+20;}
  else {first=base+1;last=base+4;}
 }
 game.body_frame++;if(game.body_frame<(uint32_t)first||game.body_frame>(uint32_t)last)game.body_frame=first;
 if(game.body_frame==(uint32_t)last)game.prev_steer=game.steer;
}
static void crash(void){
 if(game.crash>0||game.speed<1)return;game.crash=1;game.crash_phase=game.crash_ticks=0;game.collisions++;
 game.crash_accel=game.speed/((game.speed<=20?1:game.speed<=60?2:game.speed<=120?3:4)*8);
 RoadLine *line=&map.line[(uint32_t)((game.position+PLAYER_Z)/SEGMENT)%map.h.count];game.crash_target=fabsf(game.x*ROAD_W-line->distance)<fabsf(game.x*ROAD_W)?line->distance/ROAD_W:0;audio_crash();
 aster_logf(ASTER_LOG_INFO,"COLLISION stage=%u map=%u line=%u speed=%u",game.stage,game.map_id,(uint32_t)(game.position/SEGMENT),(uint32_t)game.speed);
}
void game_update(float dt){
 if(game.paused||game.state<0)return;game.steps++;
 float player_pos=game.position+PLAYER_Z;uint32_t index=(uint32_t)(player_pos/SEGMENT)%map.h.count;RoadLine *l=&map.line[index];
 if(game.state==0){animation();return;}
 if(game.state==1){
  audio_prepare();int second=(int)ceilf(game.countdown);if(second!=beep_second){beep_second=second;audio_beep();}game.countdown-=dt;if(game.countdown<=0){game.countdown=0;game.state=2;audio_begin();}
 }else if(game.state==2){
  if(!game.crash){game.remaining-=dt;game.lap+=dt;}if(game.remaining<=0){game.remaining=0;game.state=3;audio_end();}
  bool left=on_road(game.x*ROAD_W-210,l->distance),right=on_road(game.x*ROAD_W+210,l->distance);game.offroad=!left||!right;
  float max_speed=game.offroad?75:(game.gear?200:100);bool gas=game.gas||game.touch_gas||game.auto_gas;
  if(game.crash>0){
 game.crash_ticks++;if(game.crash_phase==0){if(game.steps%3==0)game.speed=MAX(0,game.speed-game.crash_accel);float cv=fabsf(l->curve);if(cv>=2)cv*=.3f;if(cv<.01f)cv=1.67f;game.x+=(game.x<game.crash_target?1:-1)*MIN(game.speed/200,1)*.2f*cv*dt;if(game.speed<=0){game.crash_phase=1;game.crash_ticks=0;game.gear=0;}}
 else if(game.crash_phase==1){if(game.crash_ticks>=105){game.crash_phase=2;game.crash_ticks=0;}}
 else {float dif=game.crash_target-game.x;if(fabsf(dif)>.05f)game.x+=dif>0?.012f:-.012f;else {game.crash=0;game.threshold=0;}}
 }
  else if(game.brake){game.speed=MAX(0,game.speed-50*dt);if(game.gear&&game.speed<100)game.gear=0;}
  else if(gas&&!game.offroad){
   if(game.speed<=max_speed){game.speed=MIN(max_speed,game.speed+(100.f/6.5f)*dt);}
   else game.speed=MAX(max_speed,game.speed-45*dt);
   if(game.gear==0&&game.speed>=100&&!game.offroad)game.gear=1;
  }else {game.speed=MAX(0,game.speed-(100.f/6.5f)*.75f*dt);if(game.speed<100)game.gear=0;}
  /* Same threshold convergence and easing as source accelerationControl. */
  float threshold=game.speed*dt;
  if(game.steer<0){if(game.threshold>-threshold)game.threshold-=.3f;else if(game.threshold<-threshold)game.threshold+=.3f;}
  else if(game.steer>0){if(game.threshold<threshold)game.threshold+=.3f;else if(game.threshold>threshold)game.threshold-=.3f;}
  else {game.threshold-=game.threshold*.1f;if(fabsf(game.threshold)<.1f)game.threshold=0;}
  if(!game.crash)game.x+=game.threshold*dt;
  float centrifugal=game.speed>26?.5f:0;if(game.speed>=100)centrifugal=(game.speed-50)/90;
  bool rightroad=fabsf(game.x*ROAD_W-l->distance)<fabsf(game.x*ROAD_W);
  float sign=(rightroad&&l->mirror)?1:-1;
  if(!game.crash&&game.speed>=30.5f)game.x+=sign*l->curve*MIN(game.speed/MAX(max_speed,1),1.f)*centrifugal*dt;
  game.x=CLAMP(game.x,-1.48f,l->distance/ROAD_W+1.48f);
  if(game.speed<=0&&game.offroad&&!game.crash){
 float target=rightroad?l->distance/ROAD_W:0,dif=target-game.x;
 if(fabsf(dif)>.05f)game.x+=dif>0?.012f:-.012f;
}
if(l->mirror&&l->distance/ROAD_W>3.5f&&game.x>1.75f&&game.x<l->distance/ROAD_W-1.75f)game.x=rightroad?l->distance/ROAD_W-1.73f:1.73f;
game.position+=game.speed;game.bg_offset+=l->curve*game.speed/SEGMENT*2.f;
  game.score+=game.speed*.01f;
  float slope=l->y1-l->y0;game.elevation=slope>1?1:slope< -1?2:0;
  /* Collision uses the source world line and per-object collider/pivot metadata. */
  for(uint32_t j=map.first[index];!game.crash&&game.speed>1&&j<map.first[index+1];j++){
   Placement *o=&map.obj[j];ObjectMeta *m=&map.meta[o->object];if(!m->collide||fabsf(o->y)>.1f)continue;
   float x=(o->side?l->distance:0)+o->x*ROAD_W;float world_scale=1.6f*(.3f/170.f)*2.f*ROAD_W*m->scale;float half=m->width*world_scale*.5f;
   float center=x+image_size[m->image][0]*world_scale*(o->x>=0?m->pcrx:-(1-m->pclx));
   if(fabsf(game.x*ROAD_W+13.2f-center)<325.5f+half)crash();
  }
  if(index>map.h.body+190&&game.route_choice==0)game.route_choice=rightroad?2:1;
  if(game.position>(map.h.count-210)*SEGMENT){
   if(game.stage>=5){if(load_map(0)<0){game.state=-1;return;}game.map_id=0;game.position=0;game.x=game.threshold=0;game.crash=0;game.state=5;l=&map.line[0];aster_logf(ASTER_LOG_INFO,"GOAL APPROACH");}
   else{
    uint32_t choice=game.route_choice==2;game.branch+=choice;game.stage++;
    uint32_t next=game.stage*(game.stage-1)/2+1+game.branch;
    if(load_map(next)<0){game.state=-1;return;}
    game.map_id=next;l=&map.line[0];game.position=0;game.x=0;game.bg_offset=0;game.route_choice=0;game.remaining+=map.h.time*(game.stage==2?.53125f:game.stage==3?.5f:game.stage==4?.46875f:.4375f);
    traffic_reset();aster_logf(ASTER_LOG_INFO,"CHECKPOINT map=%u stage=%u remaining=%u",next,game.stage,(uint32_t)game.remaining);
   }
  }
 }
 if(game.state==5){
  game.brake=game.position>=500*SEGMENT;if(game.position<500*SEGMENT)game.speed=MIN(200,game.speed+(100.f/6.5f)*dt);else game.speed=MAX(0,game.speed-(100.f/6.5f)*3*dt);if(game.speed<100)game.gear=0;
  if(game.x>.05f)game.x-=.5f*dt;else if(game.x< -.05f)game.x+=.5f*dt;else game.x=0;
  game.position+=game.speed;game.elevation=0;game.steer=game.x<0?1:game.x>0?-1:0;
  if(game.position>=500*SEGMENT&&game.speed<=0){game.state=4;audio_end();}
 }
 /* Map::updateCars advances each traffic vehicle by source speed per 60 Hz tick. */
 if(game.state==1||game.state==2)for(int c=0;c<20;c++){
  Traffic *t=&cars[c];t->z+=t->speed;
  float ahead=t->z-(game.position+PLAYER_Z);
  if(ahead>0&&ahead<DRAW_DISTANCE*SEGMENT)t->passed=1;
  if(ahead< -3000&&t->passed){
   game.score+=20000;
   t->z=game.position+(500+c*37)*SEGMENT;t->passed=0;
  }
  if(fabsf(ahead)<20*SEGMENT){

   if(c%3==1){if(t->target==t->offset)t->target=CLAMP(t->offset+(t->offset>0?-.5f:.5f),-.9f,.9f);}
   else if(c%3==2)t->target=CLAMP(game.x-(t->side?l->distance/ROAD_W:0),-.9f,.9f);
   if(t->offset<t->target)t->offset=MIN(t->target,t->offset+.005f);
   else if(t->offset>t->target)t->offset=MAX(t->target,t->offset-.005f);
  }  if(game.state==2&&fabsf(ahead)<SEGMENT&&fabsf(game.x-(t->offset+(t->side?l->distance/ROAD_W:0)))<(325.5f+image_size[traffic_ids[t->model%TRAFFIC_MODELS][0]][0]*9.0353f*1.2f*.5f)/ROAD_W)crash();
 }
 animation();
}
