/* OpenOMF / Aster adaptation preview, GPL-3.0-or-later.
 * AF/BK formats, sprite coordinates, frame tags and pixel-hitpoint rules adapted
 * from OpenOMF (MIT), copyright its contributors. See vendor/UPSTREAM.md.
 * This bounded runtime implements a subset of the desktop game's mechanics.
 */
#include <aster_imports.h>
#include <aster_gfx_engine.h>
#include "catalog.h"
#define W 480
#define H 320
#define SW 320
#define SH 200
#define GROUND 190
#define L 1
#define R 2
#define U 4
#define D 8
#define P 16
#define K 32
#define SPRMAX 65536

typedef struct {char magic[4];uint32_t bytes,health;float forward,reverse,jump,fall;uint32_t moves,sprites,frames,coords,pixels,ns,nf,nc,reserved;} Header;
typedef struct {uint32_t sf,sn,ff,fn,cf,cn;int16_t damage,category,next,hit;char input[24];int16_t constraints,blockDamage,blockStun,throwDuration;} Move;
typedef struct {int16_t x,y;uint16_t w,h;uint32_t offset,length;} Sprite;
typedef struct {int16_t sprite,ticks,ox,oy,x,y,spawn,mx,my,sound,flags,q,knock,bj,restart,pad,chain,chainId;} Frame;
typedef struct {int16_t x,y,frame,pad;} Coord;
typedef struct {uint8_t pixels[SPRMAX];int sprite,owner;Sprite *sp;} Pose;
typedef struct {int owner,move,frame,tick,dir,busy,attack,hit,health,stun,guard,aiTimer,aiHeld;float x,y,vx,vy;char history[20];int lastDir,lastHeld,punchBuffer,kickBuffer,airAttacked,jumpDelay,animStride,animReverse;uint32_t inputMs;} Actor;
typedef struct {int active,owner,move,frame,tick,dir,hit,life;float x,y,vx,vy;} Shot;
typedef struct {uint32_t slot;int active,x,y,control,menu;} Finger;
static uint8_t fighter_data[2][FIGHTER_CAP] __attribute__((aligned(4)));
static uint8_t arena_data[64520] __attribute__((aligned(4)));
static Header *heads[2];static Move *moves[2];static Sprite *sprites[2];static Frame *frames[2];static Coord *coords[2];
static Pose poses[6];static Actor actors[2];static Shot shots[4];static Finger fingers[10];
static uint8_t keyDown[128];
static int held_controls(void);
static void queue_input(Actor *a,int held);
static uint16_t scene[SW*SH],palette[256];static uint16_t *out;static int pitch;
static aster_gfx_t gfx;static aster_handle_t surface=ASTER_HANDLE_INVALID,file;
static int phase=0,chosen[2]={0,1},arena=0,loadPart,loadN,loadedReady,dirty=1,demo,paused,keys,menuTimer;
static int wins[2],roundN=1,winner=-1,freezeTicks;static float clockAcc,countdown,roundTime=99,winTime;
static uint32_t lastClock,rng=2977,perfStart,perfFrames,perfDraw;
static int32_t sounds[4];static int soundIds[4]={-1,-1,-1,-1};static uint32_t soundAt[4],soundClock;
static int audioReady,musicPlaying,errorCount;
static char problem[96];
enum {LOADING,SELECT,COUNTDOWN,FIGHT,RESULT,ERROR};
static float absf(float v){return v<0?-v:v;}
static float clampf(float v,float a,float b){return v<a?a:v>b?b:v;}
static uint32_t random32(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static uint16_t blend(uint16_t a,uint16_t b,int v){return (uint16_t)((((((a>>11)&31)*(255-v)+((b>>11)&31)*v)/255)<<11)|(((((a>>5)&63)*(255-v)+((b>>5)&63)*v)/255)<<5)|(((a&31)*(255-v)+(b&31)*v)/255));}
static void rect(int x,int y,int w,int h,uint16_t c,int alpha){int x0=x<0?0:x,y0=y<0?0:y,x1=x+w>W?W:x+w,y1=y+h>H?H:y+h;for(int yy=y0;yy<y1;yy++)for(int xx=x0;xx<x1;xx++){uint16_t *d=out+yy*pitch+xx;*d=alpha==255?c:blend(*d,c,alpha);}}
static void label(int x,int y,const char *t,uint16_t c,int size){if(size==1){aster_gfx_text(&gfx,x,y,t,c,0,0);return;}uint16_t buf[64];aster_gfx_t g;aster_gfx_init(&g,buf,8,8,16);for(;*t;t++,x+=size*6){memset(buf,0,sizeof(buf));aster_gfx_char(&g,0,0,*t,0xffff,0,0);for(int yy=0;yy<7;yy++)for(int xx=0;xx<5;xx++)if(buf[yy*8+xx])rect(x+xx*size,y+yy*size,size,size,c,255);}}
static void center(int y,const char *s,uint16_t c,int scale){label((W-(int)strlen(s)*6*scale)/2,y,s,c,scale);}
static void button(int x,int y,int w,int h,const char *name,int down){rect(x,y,w,h,down?0x19f4:0x0842,down?210:155);aster_gfx_frame_rect(&gfx,x,y,w,h,down?0xbfff:0x7c96);label(x+(w-(int)strlen(name)*6)/2,y+(h-7)/2,name,0xffff,1);}
static void audio_error(const char *op,int rc){if(rc<0&&errorCount++<8)aster_logf(ASTER_LOG_WARN,"OMF AUDIO %s %d",op,rc);}
static void audio_stop(void){if(audioReady)aster_audio_stop();musicPlaying=0;}
static void sound(int id){if(!audioReady||!musicPlaying||id<0||id>298)return;uint32_t now=aster_time_now_ms();if(now-soundClock<55)return;soundClock=now;int at=-1;for(int i=0;i<4;i++)if(soundIds[i]==id)at=i;if(at<0){at=0;for(int i=1;i<4;i++)if(soundAt[i]<soundAt[at])at=i;if(sounds[at]>0){aster_audio_effect_stop(sounds[at],0);aster_audio_effect_unload(sounds[at]);sounds[at]=0;}char path[96];aster_snprintf(path,sizeof(path),"/data/resources/sound%d.wav",id);sounds[at]=aster_audio_effect_load(path,strlen(path));soundIds[at]=id;}if(sounds[at]>0){aster_audio_effect_play_desc_t d={.size=sizeof(d),.version=1,.pitch_q16=65536,.gain_q16=42000,.fade_in_ms=4};audio_error("play",aster_audio_effect_play_ex(sounds[at],&d,sizeof(d)));soundAt[at]=now;}else audio_error("load",sounds[at]);}
static void inputs_clear(void){memset(fingers,0,sizeof(fingers));memset(keyDown,0,sizeof(keyDown));keys=0;actors[0].lastHeld=0;actors[0].punchBuffer=actors[0].kickBuffer=0;}
static void fail(const char *where,int rc){aster_snprintf(problem,sizeof(problem),"%s (%d)",where,rc);aster_errfmt("OMF %s",problem);phase=ERROR;dirty=1;if(file){aster_fs_close(file);file=0;}}
static int range(uint32_t off,uint32_t count,uint32_t stride,uint32_t max){return off<=max&&count<65536&&count*stride<=max-off;}
static int bind(int i){Header *h=(Header*)fighter_data[i];if(memcmp(h->magic,"OMF2",4)||h->bytes!=(uint32_t)loadN||!range(h->moves,70,sizeof(Move),h->bytes)||!range(h->sprites,h->ns,sizeof(Sprite),h->bytes)||!range(h->frames,h->nf,sizeof(Frame),h->bytes)||!range(h->coords,h->nc,sizeof(Coord),h->bytes)||h->pixels>h->bytes)return -1;heads[i]=h;moves[i]=(Move*)(fighter_data[i]+h->moves);sprites[i]=(Sprite*)(fighter_data[i]+h->sprites);frames[i]=(Frame*)(fighter_data[i]+h->frames);coords[i]=(Coord*)(fighter_data[i]+h->coords);return 0;}
static void play_move(Actor *a,int move,int attack){if(move<0||move>=70||!moves[a->owner][move].fn)move=11;a->move=move;a->animStride=1;a->animReverse=0;a->frame=0;a->tick=0;a->attack=attack;a->busy=attack;a->hit=0;if(attack&&a->y>=GROUND)a->vx=0;}
static void reset_actor(int i){Actor *a=&actors[i];memset(a,0,sizeof(*a));a->owner=i;a->x=i?225:95;a->y=GROUND;a->dir=i?-1:1;a->health=heads[i]->health;memset(a->history,0,sizeof(a->history));play_move(a,11,0);poses[i].sprite=-1;}
static void begin_load(void){if(file){aster_fs_close(file);file=0;}audio_stop();inputs_clear();phase=LOADING;loadPart=loadN=0;loadedReady=0;dirty=1;}
static void load_step(void){
 char path[100];uint32_t cap=loadPart<2?FIGHTER_CAP:sizeof(arena_data);uint8_t *data=loadPart<2?fighter_data[loadPart]:arena_data;
 if(!file){if(loadPart<2)aster_snprintf(path,sizeof(path),"/data/resources/fighter%d.bin",chosen[loadPart]);else aster_snprintf(path,sizeof(path),"/data/resources/arena%d.bin",arena);int rc=aster_fs_open(path,strlen(path),ASTER_FS_O_RDONLY,&file);if(rc<0){fail("Missing resources",rc);return;}}
 if((uint32_t)loadN>=cap){int rc=aster_fs_read(file,problem,1);if(rc>0){fail("Resource too large",-1);return;}aster_fs_close(file);file=0;if(loadPart<2&&bind(loadPart)<0){fail("Invalid fighter",-1);return;}loadPart++;loadN=0;}
 else{uint32_t n=cap-loadN;if(n>65536)n=65536;int rc=aster_fs_read(file,data+loadN,n);if(rc<0){fail("Read resource",rc);return;}loadN+=rc;if(!rc){aster_fs_close(file);file=0;if(loadPart<2&&bind(loadPart)<0){fail("Invalid fighter",-1);return;}if(loadPart==2&&(loadN!=(int)sizeof(arena_data)||memcmp(arena_data,"OMB1",4))){fail("Invalid arena",-1);return;}loadPart++;loadN=0;}}
 if(loadPart==3){for(int i=0;i<256;i++)palette[i]=(uint16_t)(arena_data[8+i*2]|(arena_data[9+i*2]<<8));for(int side=0;side<2;side++)for(int j=1;j<48;j++)palette[side*48+j]=player_colors[side][j];for(int i=0;i<6;i++)poses[i].sprite=-1;reset_actor(0);reset_actor(1);loadedReady=1;phase=SELECT;aster_printf("OMF loaded %s vs %s arena=%d RLE caps=%u",fighter_names[chosen[0]],fighter_names[chosen[1]],arena,(uint32_t)FIGHTER_CAP);}
 dirty=1;
}
static Sprite *decode_pose(int owner,int move,int fi,Pose *pose){
 if(move<0||move>=70)return 0;Move *m=&moves[owner][move];if(fi<0||fi>=(int)m->fn)return 0;Frame *f=&frames[owner][m->ff+fi];if(f->sprite<0||f->sprite>=(int)m->sn)return 0;uint32_t id=m->sf+f->sprite;if(id>=heads[owner]->ns)return 0;Sprite *s=&sprites[owner][id];if(!s->w||!s->h||s->w*s->h>SPRMAX||!range(heads[owner]->pixels+s->offset,s->length,1,heads[owner]->bytes))return 0;
 if(pose->sprite==(int)id&&pose->owner==owner)return s;memset(pose->pixels,0,s->w*s->h);const uint8_t *r=fighter_data[owner]+heads[owner]->pixels+s->offset;uint32_t i=0,x=0,y=0;
 while(i+1<s->length){uint32_t c=r[i]|(r[i+1]<<8),n=c>>2;int op=c&3;i+=2;if(op==0)x=n;else if(op==2)y=n;else if(op==1){while(n--&&i<s->length){if(x<s->w&&y<s->h)pose->pixels[y*s->w+x]=r[i];x++;i++;}x=0;}else break;}
 pose->sprite=id;pose->owner=owner;pose->sp=s;return s;
}
static Frame *frame_of(int owner,int move,int f){if(move<0||move>=70)return 0;Move *m=&moves[owner][move];return f>=0&&f<(int)m->fn?&frames[owner][m->ff+f]:0;}
/* The upstream command buffer contains directions only (newest first).
 * Punch/kick are independent three-dynamic-tick buffers, never direction chars. */
static int input_direction(int held,int facing){int x=((held&R)!=0)-((held&L)!=0),y=((held&D)!=0)-((held&U)!=0);return 5+x*facing+y*-3;}
static void queue_input(Actor *a,int held){
 uint32_t now=aster_time_now_ms();int d=input_direction(held,1),edge=held&~a->lastHeld;
 if(d!=a->lastDir){if(now-a->inputMs>252)memset(a->history,0,sizeof(a->history));memmove(a->history+1,a->history,9);a->history[0]='0'+d;a->history[10]=0;a->lastDir=d;a->inputMs=now;}
 if(edge&K)a->kickBuffer=3;else if(edge&P)a->punchBuffer=3;
 a->lastHeld=held;
}
static int chain_allowed(Actor *a,Move *m,int id){
 Frame *f=frame_of(a->owner,a->move,a->frame);int air=a->y<GROUND-1,idle=!a->busy&&!a->stun;
 if((m->constraints&1)&&a->x>20&&a->x<300)return 0;
 if(m->constraints&2)idle=0;
 if(f&&f->chainId==id)return !a->stun;
 int tag=m->category==2?1:m->category==4?2:m->category==5?4:m->category==6?8:m->category==7?16:m->category==8?32:0;
 if(a->stun||!tag)return 0;
 if(m->category==2&&(air||actors[1-a->owner].y<GROUND-1||absf(actors[1-a->owner].x-a->x)>34))return 0;
 if(f&&((f->chain&tag)||((f->chain&64)&&m->category==7)))return 1;
 return idle&&(m->category==7?(air&&!a->airAttacked):!air);
}
static int attack_move(Actor *a,int held,int unused){
 char input[11]={0};uint32_t age=aster_time_now_ms()-a->inputMs;
 for(int j=0;j<10&&a->history[j];j++){int n=a->history[j]-'0';if(a->dir<0){if(n==1)n=3;else if(n==3)n=1;else if(n==4)n=6;else if(n==6)n=4;else if(n==7)n=9;else if(n==9)n=7;}input[j]='0'+n;if(age>252)break;}
 /* Preserve AF move order, including conditional close/air variants. */
 for(int pass=0;pass<2;pass++){char prefix=pass?'K':'P';if(!(pass?a->kickBuffer:a->punchBuffer))continue;
  for(int i=15;i<70;i++){Move *m=&moves[a->owner][i];if(!m->fn||m->input[0]!=prefix||!chain_allowed(a,m,i))continue;int len=strlen(m->input);if(len>1&&strncmp(m->input+1,input,len-1))continue;
   a->punchBuffer=a->kickBuffer=0;if(len>1)memset(a->history,0,sizeof(a->history));if(a->y<GROUND-1)a->airAttacked=1;play_move(a,i,1);aster_printf("OMF command p%d move=%d input=%s",a->owner+1,i,m->input);return i;
  }
 }
 (void)held;(void)unused;return -1;
}
static void spawn(int owner,Frame *f,float x,float y,int dir){int id=f->spawn;if(id<0||id>=70||!moves[owner][id].fn)return;for(int i=0;i<4;i++)if(!shots[i].active){shots[i]=(Shot){.active=1,.owner=owner,.move=id,.dir=dir,.x=x+f->mx*dir,.y=y+f->my,.vx=dir*5,.life=180};poses[2+i].sprite=-1;return;}}
static void enter_actor_frame(Actor *a){Frame *f=frame_of(a->owner,a->move,a->frame);if(!f)return;if(f->sound>=0&&f->sound<30)sound((int)sound_tables[chosen[a->owner]][f->sound]-1);if(f->spawn>=0)spawn(a->owner,f,a->x,a->y,a->dir);if(f->q>0)a->hit=0;if(f->flags&64){a->y=GROUND;a->vy=0;}if(f->flags&32)a->vx=0;}
static int looped(int move){return move==11||move==10||move==4||move==5||move==6||move==3;}
static void seek_actor(Actor *a,int tick){
 Move *m=&moves[a->owner][a->move];int total=0;for(uint32_t i=0;i<m->fn;i++)total+=frames[a->owner][m->ff+i].ticks;
 if(tick<0)tick+=total;if(tick<0)tick=0;if(tick>=total)tick=total-1;
 for(uint32_t i=0;i<m->fn;i++){int ticks=frames[a->owner][m->ff+i].ticks;if(tick<ticks){a->frame=i;a->tick=tick;return;}tick-=ticks;}
}
static void advance_actor(Actor *a){
 int count=a->animStride;while(count-->0){Frame *f=frame_of(a->owner,a->move,a->frame);if(!f)return;
  if(a->animReverse){if(--a->tick<0){if(a->frame>0){a->frame--;f=frame_of(a->owner,a->move,a->frame);a->tick=f->ticks-1;}else seek_actor(a,-1);}}
  else if(++a->tick>=f->ticks){a->tick=0;a->frame++;Move *m=&moves[a->owner][a->move];if(a->frame>=(int)m->fn){if(looped(a->move))a->frame=0;else if(a->move==1&&a->y<GROUND)a->frame=m->fn-1;else if(a->move==48||a->move==49){a->frame=m->fn-1;a->tick=-30000;return;}else{a->busy=0;a->attack=0;play_move(a,a->y<GROUND?1:11,0);return;}}}
 }
}
static void actor_step(Actor *a,int held){
 Actor *b=&actors[1-a->owner];Header *h=heads[a->owner];if(!a->busy)a->dir=b->x>=a->x?1:-1;
 queue_input(a,held);if(a->jumpDelay)a->jumpDelay--;if(a->health>0&&!a->stun&&(a->punchBuffer||a->kickBuffer))attack_move(a,held,0);if(a->punchBuffer)a->punchBuffer--;if(a->kickBuffer)a->kickBuffer--;
 if(a->stun>0){a->stun--;a->vx*=.8f;if(!a->stun){a->busy=0;play_move(a,11,0);}}
 else if(!a->busy&&a->health>0){
  a->guard=(held&(a->dir==1?L:R))!=0;
  if(!a->busy){int x=((held&R)!=0)-((held&L)!=0);if(a->y>=GROUND){if((held&U)&&!a->jumpDelay){a->vy=h->jump*216/256;a->vx=x*(x==a->dir?h->forward:h->reverse);a->airAttacked=0;play_move(a,1,0);a->animStride=x?7:1;a->animReverse=x&&x!=a->dir;seek_actor(a,a->animReverse?-110:110);}else if(held&D){a->vx=0;if(a->move!=4)play_move(a,4,0);}else if(x){a->vx=x*(x==a->dir?h->forward:h->reverse);if(a->move!=10)play_move(a,10,0);}else{a->vx=0;if(a->move!=11)play_move(a,11,0);}}}
 }
 a->lastHeld=held;
 if(a->tick==0)enter_actor_frame(a);
 Frame *motion=frame_of(a->owner,a->move,a->frame);if(motion){if(motion->x!=32767){if(motion->flags&16)a->vx=motion->x*a->dir;else a->x+=motion->x*a->dir;}if(motion->y!=32767){if(motion->flags&16)a->vy=motion->y;else a->y+=motion->y;}}
 a->x+=a->vx;a->y+=a->vy;if(a->y<GROUND){a->vy+=h->fall;if(a->vy>13)a->vy=13;}else{if(a->vy>0){a->y=GROUND;a->vy=0;a->vx=0;a->airAttacked=0;a->jumpDelay=3;if(!a->busy)play_move(a,11,0);}a->y=GROUND;}
 a->x=clampf(a->x,20,300);
 Frame *f=frame_of(a->owner,a->move,a->frame);if(!f)return;
 advance_actor(a);
}
static int touches(int owner,int move,int fi,float x,float y,int facing,Actor *target){
 Frame *f=frame_of(owner,move,fi);if(!f||f->flags&4)return 0;Move *m=&moves[owner][move];Sprite *t=decode_pose(target->owner,target->move,target->frame,&poses[target->owner]);if(!t)return 0;Frame *tf=frame_of(target->owner,target->move,target->frame);if(tf&&(tf->flags&128))return 0;
 int flip=(target->dir<0)^((tf->flags&1)!=0);int tx=(int)target->x+(flip?-t->x-t->w:t->x),ty=(int)target->y+t->y+tf->oy;int attackDir=facing*((f->flags&1)?-1:1);
 for(uint32_t j=0;j<m->cn;j++){Coord c=coords[owner][m->cf+j];if(c.frame!=f->sprite)continue;int px=(int)x+c.x*attackDir-tx,py=(int)y+c.y-ty;if(px<0||py<0||px>=t->w||py>=t->h)continue;int sx=flip?t->w-1-px:px;uint8_t pixel=poses[target->owner].pixels[py*t->w+sx];if(pixel&&pixel<96)return 1;}
 if(m->category==2&&fi>=2&&absf(x-target->x)<29&&absf(y-target->y)<12)return 1;
 return 0;
}
static void hit_actor(Actor *attacker,int move,Actor *target){
 Move *m=&moves[attacker->owner][move];Frame *f=frame_of(attacker->owner,attacker->move,attacker->frame);int direction=input_direction(target->lastHeld,target->dir);int canBlock=!target->busy||target->move==5||target->move==6;int blocked=canBlock&&target->y>=GROUND&&(direction==1||direction==4)&&!(f&&f->flags&256)&&m->category!=2;
 if(blocked&&((m->category==4&&!(target->lastHeld&D))||(m->category==7&&(target->lastHeld&D))))blocked=0;
 int damage=blocked?m->blockDamage:m->damage;if(damage<1&&!blocked)damage=6;target->health-=damage;if(target->health<0)target->health=0;
 target->stun=blocked?(m->blockStun>0?m->blockStun:5):9+damage/5;play_move(target,blocked?((target->lastHeld&D)?6:5):9,0);target->busy=1;target->vx=attacker->dir*(blocked?1.0f:2.0f);target->vy=0;
 freezeTicks=blocked?2:3;sound((int)sound_tables[chosen[target->owner]][blocked?4:3]-1);
 aster_printf("OMF hit p%d move=%d damage=%d block=%d hp=%d",attacker->owner+1,move,damage,blocked,target->health);
}
static int ai_input(Actor *a){Actor *b=&actors[1-a->owner];if(a->aiTimer-->0)return a->aiHeld;int held=0;float dist=absf(a->x-b->x);int toward=b->x>a->x?R:L;uint32_t v=random32();a->aiTimer=2+(v&3);if(dist>48)held=toward;else{if((v&7)==0)held=toward==R?L:R;else if((v&3)==0)held=D;if((v&1)==0)held|=P;else held|=K;}if(dist>90&&(v%19)==0)held|=U;a->aiHeld=held;return held;}
static int held_controls(void){int held=0;for(int i=0;i<10;i++)if(fingers[i].active)held|=fingers[i].control;return held;}
static void begin_round(int reset){inputs_clear();if(reset){wins[0]=wins[1]=0;roundN=1;}for(int i=0;i<2;i++)reset_actor(i);memset(shots,0,sizeof(shots));countdown=2.2f;roundTime=99;winner=-1;freezeTicks=0;phase=COUNTDOWN;paused=0;clockAcc=0;lastClock=aster_time_now_ms();perfStart=perfFrames=perfDraw=0;audio_stop();if(audioReady){char path[80];aster_snprintf(path,sizeof(path),"/data/resources/arena%d.mp3",arena);audio_error("music",aster_audio_start_file(path,strlen(path)));musicPlaying=1;}dirty=1;aster_printf("OMF round=%d %s vs %s demo=%d",roundN,fighter_names[chosen[0]],fighter_names[chosen[1]],demo);}
static void simulate(void){
 if(phase==COUNTDOWN){countdown-=.028f;if(countdown<=0){phase=FIGHT;aster_printf("OMF FIGHT");}return;}if(phase!=FIGHT)return;if(freezeTicks>0){freezeTicks--;return;}
 roundTime-=.028f;actor_step(&actors[0],demo?ai_input(&actors[0]):held_controls());actor_step(&actors[1],ai_input(&actors[1]));
 if(absf(actors[0].x-actors[1].x)<24&&absf(actors[0].y-actors[1].y)<40){float d=actors[0].x<actors[1].x?1:-1;actors[0].x-=d*2;actors[1].x+=d*2;}
 for(int i=0;i<2;i++){Actor *a=&actors[i],*b=&actors[1-i];if(a->attack&&!a->hit&&touches(i,a->move,a->frame,a->x,a->y,a->dir,b)){a->hit=1;hit_actor(a,a->move,b);}}
 for(int i=0;i<4;i++)if(shots[i].active){Shot *s=&shots[i];Frame *f=frame_of(s->owner,s->move,s->frame);if(!f){s->active=0;continue;}if(!s->tick){if(f->x!=32767)s->vx=f->x*s->dir;if(f->y!=32767)s->vy=f->y;if(f->sound>=0&&f->sound<30)sound((int)sound_tables[chosen[s->owner]][f->sound]-1);}s->x+=s->vx;s->y+=s->vy;s->tick++;if(s->tick>=f->ticks){s->tick=0;s->frame++;if(s->frame>=(int)moves[s->owner][s->move].fn)s->frame=0;}if(touches(s->owner,s->move,s->frame,s->x,s->y,s->dir,&actors[1-s->owner])){hit_actor(&actors[s->owner],s->move,&actors[1-s->owner]);s->active=0;}if(--s->life<=0||s->x<-100||s->x>420)s->active=0;}
 if(actors[0].health<=0||actors[1].health<=0||roundTime<=0){winner=actors[0].health>=actors[1].health?0:1;wins[winner]++;phase=RESULT;winTime=0;play_move(&actors[winner],48,0);play_move(&actors[1-winner],49,0);actors[0].vx=actors[1].vx=0;aster_printf("OMF round done winner=%d score=%d:%d",winner+1,wins[0],wins[1]);}
}
static void draw_robot(int owner,int move,int fi,float x,float y,int dir,Pose *pose){Sprite *s=decode_pose(owner,move,fi,pose);Frame *f=frame_of(owner,move,fi);if(!s||!f)return;int flip=(dir<0)^((f->flags&1)!=0),vf=(f->flags&2)!=0;int dx=(int)x+(flip?-s->x-f->ox-s->w:s->x+f->ox),dy=(int)y+(vf?-s->y+f->oy-s->h:s->y+f->oy);for(int sy=0;sy<s->h;sy++){int yy=dy+sy;if(yy<0||yy>=SH)continue;for(int sx=0;sx<s->w;sx++){int xx=dx+sx;if(xx<0||xx>=SW)continue;int at=(vf?s->h-1-sy:sy)*s->w+(flip?s->w-1-sx:sx);uint8_t v=pose->pixels[at];if(v){if(owner&&v<48)v+=48;scene[yy*SW+xx]=palette[v];}}}}
static void arena_scene(int preview){const uint8_t *bg=arena_data+520;for(int i=0;i<SW*SH;i++)scene[i]=palette[bg[i]];for(int i=0;i<2;i++){Actor *a=&actors[i];draw_robot(i,preview?11:a->move,preview?((menuTimer/90)%2):a->frame,preview?(i?230:90):a->x,preview?GROUND:a->y,a->dir,&poses[i]);}if(!preview)for(int i=0;i<4;i++)if(shots[i].active){Shot *s=&shots[i];draw_robot(s->owner,s->move,s->frame,s->x,s->y,s->dir,&poses[2+i]);}for(int yy=0;yy<300;yy++){int sy=yy*2/3;uint16_t *d=out+(yy+10)*pitch;if(yy&&sy==(yy-1)*2/3){memcpy(d,d-pitch,W*2);continue;}for(int xx=0;xx<W;xx++)d[xx]=scene[sy*SW+xx*2/3];}}
static void menu_render(void){arena_scene(1);rect(0,0,W,H,0x0023,90);rect(0,0,W,60,0x0023,210);center(9,"OPEN OMF",0xffdf,4);center(44,"ONE MUST FALL 2097 / ASTER PREVIEW",0x7efc,1);char text[100];label(28,79,"YOUR ROBOT",0x7efc,1);label(297,79,"CPU ROBOT",0xfb90,1);label(35,100,fighter_names[chosen[0]],0xffff,2);label(291,100,fighter_names[chosen[1]],0xffff,2);button(14,150,36,45,"<",0);button(190,150,36,45,">",0);button(254,150,36,45,"<",0);button(430,150,36,45,">",0);aster_snprintf(text,sizeof(text),"ARENA %d/5",arena+1);center(219,text,0xffff,1);button(120,207,42,32,"<",0);button(318,207,42,32,">",0);button(42,252,190,42,"START FIGHT",0);button(248,252,190,42,"DEMO FIGHT",0);center(307,"ARROWS MOVE / Z PUNCH / X KICK / C P+K",0xbdf7,1);}
static void render(void){uint32_t begin=aster_time_now_ms();aster_surface_buffer_t b={.size=sizeof(b),.version=1};int rc=aster_surface_acquire(surface,&b,sizeof(b));if(rc<0||!b.ptr){fail("Surface",rc);return;}out=(uint16_t*)b.ptr;pitch=b.pitch/2;aster_gfx_init(&gfx,b.ptr,b.width,b.height,b.pitch);aster_gfx_clear(&gfx,0);
 if(phase==LOADING){center(90,"OPEN OMF",0xffff,4);center(170,"LOADING ORIGINAL GAME DATA",0x7efc,1);rect(80,220,320,6,0x2104,255);rect(80,220,(loadPart*320+loadN*320/FIGHTER_CAP)/3,6,0x6ffc,255);}else if(phase==ERROR){center(100,"RESOURCE ERROR",0xf800,2);center(150,problem,0xffff,1);}else if(phase==SELECT)menu_render();else{arena_scene(0);char s[80];rect(0,0,W,39,0x0023,190);label(9,3,fighter_names[chosen[0]],0x7efc,1);label(300,3,fighter_names[chosen[1]],0xfb90,1);rect(9,17,178,9,0x2104,255);rect(293,17,178,9,0x2104,255);int left=actors[0].health*176/heads[0]->health,right=actors[1].health*176/heads[1]->health;rect(10,18,left,7,0x4ff2,255);rect(470-right,18,right,7,0xfba8,255);aster_snprintf(s,sizeof(s),"%02d",(int)(roundTime>0?roundTime:0));center(5,s,0xffff,3);aster_snprintf(s,sizeof(s),"ROUND %d   %d : %d",roundN,wins[0],wins[1]);center(37,s,0xffff,1);button(409,42,63,25,"MENU",paused);
 int held=held_controls();button(8,250,54,30,"<",held&L);button(120,250,54,30,">",held&R);button(64,220,54,30,"JUMP",held&U);button(64,280,54,30,"DOWN",held&D);button(8,220,54,30,"UP-L",(held&(U|L))==(U|L));button(120,220,54,30,"UP-R",(held&(U|R))==(U|R));button(8,280,54,30,"DN-L",(held&(D|L))==(D|L));button(120,280,54,30,"DN-R",(held&(D|R))==(D|R));button(300,254,73,56,"PUNCH",held&P);button(390,254,82,56,"KICK",held&K);button(390,204,82,42,"P+K",(held&(P|K))==(P|K));if(demo)label(188,60,"DEMO",0xffe0,1);if(phase==COUNTDOWN){aster_snprintf(s,sizeof(s),"%d",(int)countdown+1);center(130,s,0xffff,5);}if(paused||phase==RESULT){rect(85,84,310,151,0x0023,235);if(phase==RESULT){center(98,winner==0?"YOU WIN":"CPU WINS",0xffe0,3);}else center(98,"PAUSED",0xffe0,3);button(120,146,240,30,phase==RESULT?(wins[0]>=2||wins[1]>=2?"REMATCH":"NEXT ROUND"):"RESUME",0);button(120,187,240,30,"ROBOT & ARENA SELECT",0);}}
 rc=aster_surface_present(surface);if(rc<0){fail("Present",rc);return;}dirty=0;if(phase==FIGHT&&!paused){uint32_t now=aster_time_now_ms();if(!perfFrames)perfStart=begin;perfDraw+=now-begin;if(++perfFrames==60){aster_printf("OMF PERF frames=60 elapsed_ms=%u draw_ms=%u demo=%d",now-perfStart,perfDraw,demo);perfFrames=perfDraw=0;}}
}
static int menu_at(int x,int y){if(phase==SELECT){if(x>=42&&x<232&&y>=252&&y<294)return 1;if(x>=248&&x<438&&y>=252&&y<294)return 2;if(y>=150&&y<195){if(x>=14&&x<50)return 3;if(x>=190&&x<226)return 4;if(x>=254&&x<290)return 5;if(x>=430&&x<466)return 6;}if(y>=207&&y<239){if(x>=120&&x<162)return 7;if(x>=318&&x<360)return 8;}}else if(paused||phase==RESULT){if(x>=120&&x<360&&y>=146&&y<176)return 10;if(x>=120&&x<360&&y>=187&&y<217)return 11;}else if(x>=409&&y>=42&&x<472&&y<67)return 9;return 0;}
static void menu_action(int id){if(id==1||id==2){demo=id==2;begin_round(1);}else if(id>=3&&id<=6){int who=id>=5,delta=id%2?10:1;chosen[who]=(chosen[who]+delta)%11;begin_load();}else if(id==7||id==8){arena=(arena+(id==7?4:1))%5;begin_load();}else if(id==9){paused=1;inputs_clear();if(audioReady)aster_audio_pause();dirty=1;}else if(id==10){if(phase==RESULT){roundN++;begin_round(wins[0]>=2||wins[1]>=2);}else{paused=0;if(audioReady)aster_audio_resume();lastClock=aster_time_now_ms();inputs_clear();}}else if(id==11){audio_stop();paused=0;phase=SELECT;reset_actor(0);reset_actor(1);inputs_clear();dirty=1;}}
static int control_at(int x,int y){int held=0;if(x>=8&&x<174&&y>=220&&y<310){if(x<63)held|=L;if(x>117)held|=R;if(y<250)held|=U;if(y>=280)held|=D;}if(x>=300&&x<373&&y>=254&&y<310)held|=P;if(x>=390&&x<472&&y>=254&&y<310)held|=K;if(x>=390&&x<472&&y>=204&&y<246)held|=(P|K);return held;}
static void touch(const aster_event_t *e){if(phase==LOADING||phase==ERROR)return;int index=-1;for(int i=0;i<10;i++)if(fingers[i].active&&fingers[i].slot==e->u.touch.slot)index=i;if(e->u.touch.action==ASTER_TOUCH_DOWN&&index<0){for(int i=0;i<10;i++)if(!fingers[i].active){index=i;break;}if(index>=0)fingers[index]=(Finger){.slot=e->u.touch.slot,.active=1,.x=e->u.touch.x,.y=e->u.touch.y,.menu=menu_at(e->u.touch.x,e->u.touch.y)};}if(index<0)return;Finger *f=&fingers[index];f->x=e->u.touch.x;f->y=e->u.touch.y;if(!f->menu&&!paused&&phase!=RESULT)f->control=control_at(f->x,f->y);if(e->u.touch.action==ASTER_TOUCH_UP){int action=f->menu;f->active=0;f->control=0;if(action&&action==menu_at(f->x,f->y))menu_action(action);}if(phase==FIGHT&&!paused&&!demo)queue_input(&actors[0],held_controls());dirty=1;}
/* PC keys own virtual touch slots. Aliases are aggregated before transitions,
 * so releasing A while Left is held does not release the left touch contact. */
static int key_control(int c){return c==105||c==30?L:c==106||c==32?R:c==103||c==17?U:c==108||c==31?D:c==44?P:c==45?K:c==46?(P|K):0;}
static void key(const aster_event_t *e){
 int c=e->u.key.code,on=e->u.key.pressed;if(c<0||c>=128)return;
 if(keyDown[c]==on)return;keyDown[c]=on;
 if(phase==SELECT){if(on){if(c==28){demo=0;begin_round(1);}else if(c==57){demo=1;begin_round(1);}else if(c==105||c==106){chosen[0]=(chosen[0]+(c==105?10:1))%11;begin_load();}else if(c==103||c==108){arena=(arena+(c==103?4:1))%5;begin_load();}}return;}
 if(on&&c==1){if(paused)menu_action(10);else if(phase==RESULT)menu_action(11);else menu_action(9);return;}
 if(on&&c==28&&(paused||phase==RESULT)){menu_action(10);return;}
 if(on&&c==19&&loadedReady){begin_round(1);return;}
 if(paused||phase==RESULT||phase==LOADING||phase==ERROR)return;
 int next=0;for(int i=0;i<128;i++)if(keyDown[i])next|=key_control(i);
 static const int xy[6][2]={{34,265},{148,265},{91,238},{91,291},{335,280},{430,280}};
 for(int i=0;i<6;i++){int bit=1<<i;if((keys&bit)==(next&bit))continue;aster_event_t t={0};t.type=ASTER_EVENT_TOUCH;t.u.touch.slot=0x80000000u+i;t.u.touch.x=xy[i][0];t.u.touch.y=xy[i][1];t.u.touch.action=(next&bit)?ASTER_TOUCH_DOWN:ASTER_TOUCH_UP;touch(&t);}
 keys=next;dirty=1;
}
ASTER_APP_EXPORT int aster_app_init(void){aster_display_info_t d={.size=sizeof(d),.version=1};int rc=aster_display_get_info(0,&d,sizeof(d));if(rc<0)return rc;if(d.app_width!=W||d.app_height!=H)return ASTER_ENOTSUP;aster_surface_desc_t desc={.size=sizeof(desc),.version=1,.width=W,.height=H,.format=ASTER_PIXEL_RGB565};rc=aster_surface_create(&desc,sizeof(desc),&surface);if(rc<0)return rc;audioReady=aster_system_has_feature(ASTER_FEATURE_AUDIO_OUTPUT)>0;aster_app_set_framerate(30);begin_load();lastClock=aster_time_now_ms();aster_printf("OpenOMF preview init: original 11 HARs, 5 arenas, cap=%u",(uint32_t)FIGHTER_CAP);return 0;}
ASTER_APP_EXPORT void aster_app_event(const aster_event_t *e){if(!e)return;if(e->type==ASTER_EVENT_KEY)key(e);else if(e->type==ASTER_EVENT_TOUCH)touch(e);else if(e->type==ASTER_EVENT_FRAME){uint32_t now=aster_time_now_ms();float dt=(now-lastClock)*.001f;lastClock=now;if(dt>.2f)dt=.2f;if(phase==LOADING)load_step();else if(phase==SELECT){menuTimer+=(int)(dt*1000);if(menuTimer%100<34)dirty=1;}else if(!paused&&(phase==COUNTDOWN||phase==FIGHT)){clockAcc+=dt;int steps=0;while(clockAcc>=.028f&&steps++<8){simulate();clockAcc-=.028f;}dirty=1;}else if(phase==RESULT&&!paused){winTime+=dt;dirty=winTime<.15f;}if(dirty)render();}}
ASTER_APP_EXPORT void aster_app_pause(void){inputs_clear();if(phase==COUNTDOWN||phase==FIGHT)paused=1;if(audioReady)aster_audio_pause();}
ASTER_APP_EXPORT void aster_app_resume(void){inputs_clear();lastClock=aster_time_now_ms();dirty=1;}
static void release(void){inputs_clear();audio_stop();for(int i=0;i<4;i++)if(sounds[i]>0){aster_audio_effect_unload(sounds[i]);sounds[i]=0;soundIds[i]=-1;}if(file){aster_fs_close(file);file=0;}if(surface!=ASTER_HANDLE_INVALID){aster_surface_destroy(surface);surface=ASTER_HANDLE_INVALID;}}
ASTER_APP_EXPORT void aster_app_stop(void){release();}
ASTER_APP_EXPORT void aster_app_destroy(void){release();}
