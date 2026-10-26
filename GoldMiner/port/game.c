/* Native C adaptation of GoldMiner-Rebirth's GameStates.lua, Entities.lua
 * and props.lua (MIT, Lazy_V). Original values and level data are retained.
 * A fixed 60 Hz update preserves the original frame-based mole movement. */
#include "game.h"
#include <aster_imports.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
enum {MINIGOLD,GOLD,GOLDPLUS,BIGGOLD,MINIROCK,ROCK,BIGROCK,DIAMOND,BAG,MOLE,DIAMONDMOLE,SKULL,BONE,TNT};
typedef struct {uint8_t type;int16_t x,y;int8_t dir;} Seed;
typedef struct {const char *name;uint8_t bg;unsigned n;const Seed *seeds;} Level;
#include "levels.h"
static const char *images[]={"gold_mini.png","gold_normal.png","gold_normal_plus.png","gold_big.png","rock_mini.png","rock_normal.png","rock_big.png","diamond.png","question_bag.png","mole_sheet.png","mole_with_diamond_sheet.png","skull.png","bone.png","tnt.png"};
static const float masses[]={2,3.5,5,7,5.5,7,10,1.5,1,1.5,1.5,2,3,1};
static const int bonuses[]={50,100,250,500,11,20,100,600,50,2,602,20,7,2};
static const int bonusSound[]={A_NORMAL,A_NORMAL,A_NORMAL,A_HIGH,A_LOW,A_LOW,A_LOW,A_HIGH,A_NORMAL,A_LOW,A_HIGH,A_LOW,A_LOW,A_LOW};
typedef struct {float x,y,cx,cy,r,mass,dest,idle,anim,shine,blast,bx,by;int type,active,grabbed,dir,moving,bonus,extra,exploded;unsigned w,h;} Entity;
typedef struct {int level,money,viewMoney,goal,goalAdd,bombs,flags,currentBonus;float strength,useTimer,anim;char real[16];} Player;
typedef struct {float angle,length,x,y,cx,cy,bonusTimer,explosion;int rotateRight,firing,backing,grabbed,stopped,showBonus,frame;float ex,ey;} Hook;
static Entity entities[96];static unsigned entityCount;static Player p;static Hook h;
static const Level *layout;static int highScore,highLevel,menuIndex,selectIndex,itemCount,items[5],prices[5],bought,finishShopping,shopSad;
static float stateAge,timeLeft,accum,bonusView,strengthView,shoppingTimer,madeDelay;
static uint32_t rng=0x7143ac59;int gm_state,gm_paused,gm_exit;
static const char *stateNames[]={"MENU","HIGH SCORE","NEW RECORD","GOAL","LEVEL COMPLETE","SHOP","GAME","GAME OVER"};
const char *gm_state_name(void){return stateNames[gm_state];}
static unsigned randomn(unsigned n){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng%n+1;}
static void image(const char *name,int frame,float x,float y,float angle,float sx,float sy,float ox,float oy){gm_blit(name,frame,x,y,angle*.01745329251994f,sx,sy,ox,oy);}
static void plain(const char *name,float x,float y){image(name,0,x,y,0,1,1,0,0);}
static int near(float ax,float ay,float ar,float bx,float by,float br){float dx=ax-bx,dy=ay-by,r=ar+br;return dx*dx+dy*dy<r*r;}
static void reset_hook(void){h.grabbed=-1;h.firing=h.backing=h.stopped=h.showBonus=0;h.angle=75;h.length=0;h.frame=0;gm_audio_play(A_RESET);}
static void reset_buffs(void){p.strength=1;p.flags=0;}
static void init_level(void){
 layout=&levels[0];for(unsigned i=0;i<sizeof(levels)/sizeof(levels[0]);i++)if(!strcmp(levels[i].name,p.real)){layout=&levels[i];break;}
 entityCount=layout->n;if(entityCount>96)entityCount=96;memset(entities,0,sizeof(entities));
 for(unsigned i=0;i<entityCount;i++){
  const Seed *seed=&layout->seeds[i];Entity *e=&entities[i];e->type=seed->type;e->x=seed->x;e->y=seed->y;e->dir=seed->dir;
  e->w=gm_width(images[e->type]);e->h=gm_height(images[e->type]);
  if(e->type==MOLE||e->type==DIAMONDMOLE){e->w=18;e->h=13;e->moving=1;e->idle=1;e->dest=e->x+e->dir*135;}
  e->r=(e->w/2.0f+e->h/2.0f)/2;e->cx=e->x+e->w/2.0f;e->cy=e->y+e->h/2.0f;
  e->mass=masses[e->type];e->bonus=bonuses[e->type];e->active=1;e->bx=e->cx;e->by=e->cy;
  if(e->type==BAG){e->mass=randomn(9);e->bonus=randomn(16)*50;e->extra=20;}
 }
 memset(&h,0,sizeof(h));h.angle=75;h.x=h.cx=158;h.y=30;h.cy=43;h.grabbed=-1;h.rotateRight=1;h.bonusTimer=1;gm_audio_play(A_RESET);
 p.viewMoney=p.money;p.useTimer=0;timeLeft=61;bonusView=strengthView=0;
 aster_printf("GoldMiner level=%u layout=%s entities=%u goal=%u",p.level,layout->name,entityCount,p.goal);
}
static void enter(int state){
 if(gm_state==S_GAME&&state!=S_GAME)reset_buffs();gm_state=state;stateAge=0;
 if(state==S_MENU)menuIndex=0;
 else if(state==S_GOAL){if(p.level>1&&p.level<=9)p.goalAdd+=270;p.goal+=p.goalAdd;gm_audio_play(A_GOAL);}
 else if(state==S_MADE_GOAL){madeDelay=.5f;p.level++;int level=p.level<=3?p.level:(p.level-3)%7+3;snprintf(p.real,sizeof(p.real),"L%d_%u",level,randomn(3));gm_audio_play(A_MADE);}
 else if(state==S_GAME)init_level();
 else if(state==S_SHOP){
  itemCount=0;selectIndex=0;bought=finishShopping=shopSad=0;shoppingTimer=1.5f;
  prices[0]=randomn(300)+1+p.level*2;prices[1]=randomn(300)+100;prices[2]=randomn(p.level*50)+1+p.level*2;prices[3]=randomn(150)+1;prices[4]=randomn(p.level*100)+201;
  for(int i=0;i<5;i++)if(randomn(3)>=2)items[itemCount++]=i;
  if(!itemCount)items[itemCount++]=0;
 }
 aster_printf("GoldMiner scene=%s",gm_state_name());
}
static void start(void){memset(&p,0,sizeof(p));p.level=1;p.goal=375;p.goalAdd=275;p.strength=1;strcpy(p.real,"L1_1");enter(S_GOAL);}
static void take_effect(Entity *e){
 if(e->type==BIGGOLD)e->shine=.5f;
 if(p.flags&1)e->mass/=1.5f;
 if((p.flags&2)&&e->type==BAG)e->extra*=2;
 if((p.flags&4)&&(e->type==MINIROCK||e->type==ROCK||e->type==BIGROCK))e->bonus*=3;
 if(p.flags&8){if(e->type==DIAMOND)e->bonus=e->bonus*3/2;else if(e->type==DIAMONDMOLE)e->bonus=(e->bonus-2)*3/2+2;}
}
static void use_bomb(void){
 if(p.bombs<=0||h.grabbed<0||h.stopped)return;
 Entity *e=&entities[h.grabbed];h.explosion=.5f;h.ex=e->x;h.ey=e->y;e->active=0;h.grabbed=-1;h.frame=0;p.useTimer=.39f;gm_audio_play(A_EXPLODE);p.bombs--;
}
static void hook_update(float dt){
 if(!h.stopped){
  if(fabsf(h.angle-75)<1)h.rotateRight=1;if(fabsf(h.angle+75)<1)h.rotateRight=0;
  if(!h.firing)h.angle+=(h.rotateRight?-1:1)*dt*65;
  float r=h.angle*.01745329251994f;float dx=-sinf(r),dy=cosf(r);
  h.x=158+dx*h.length;h.y=30+dy*h.length;h.cx=158+dx*(h.length+13);h.cy=30+dy*(h.length+13);
  if(h.explosion>0)h.explosion-=dt;
 }
 if(h.firing){
  if(!h.backing)h.length+=dt*100;
  if(h.length>=230||h.grabbed>=0)h.backing=1;
  if(h.backing){h.length-=dt*100*(h.grabbed>=0?p.strength/entities[h.grabbed].mass:1);gm_audio_play(A_BACK);}
 }
 if(h.length<=0&&h.backing){
  if(h.grabbed>=0){
   Entity *e=&entities[h.grabbed];
   if(h.bonusTimer==1){
    if(e->extra&&(int)randomn(100)<=e->extra){if(randomn(100)<=20){if(p.bombs<12)p.bombs++;}else{p.strength=fminf(6,p.strength*1.5f+1);strengthView=1;}gm_audio_play(A_HIGH);}
    else{e->extra=0;p.money+=e->bonus;p.currentBonus=e->bonus;aster_printf("GoldMiner collected=%u money=%u goal=%u",e->bonus,p.money,p.goal);bonusView=1;h.showBonus=1;gm_audio_play(A_MONEY);}
    h.stopped=1;h.firing=0;
   }
   h.bonusTimer-=dt;
   if(h.bonusTimer<=0){if(!e->extra)p.viewMoney+=e->bonus;e->active=0;h.bonusTimer=1;reset_hook();}
  }else reset_hook();
 }
}
static void destroy_entity(Entity *e){if(e->type==TNT){if(!e->exploded){e->exploded=1;e->blast=.5f;}}else e->active=0;}
static void entity_update(unsigned index,float dt){
 Entity *e=&entities[index];if(!e->active)return;e->anim+=dt;
 if(e->shine>0)e->shine-=dt;if(e->blast>0)e->blast-=dt;
 int moving=e->type==MOLE||e->type==DIAMONDMOLE;
 if(e->grabbed){e->x=h.cx;e->y=h.cy;}
 e->cx=e->x+(moving?0:e->w/2.0f);e->cy=e->y+(moving?0:e->h/2.0f);
 if(near(e->cx,e->cy,e->r,h.cx,h.cy,6)&&h.grabbed<0&&!h.backing){
  take_effect(e);e->grabbed=1;h.grabbed=index;gm_audio_play(bonusSound[e->type]);h.frame=e->r<6?2:1;aster_printf("GoldMiner caught type=%u value=%u mass=%u/100",e->type,e->bonus,(unsigned)(e->mass*100));
  if(e->type==TNT){if(!e->exploded){e->exploded=1;e->blast=.5f;}h.frame=2;}
 }
 if(moving){
  if(!e->moving)e->idle-=dt;
  if(e->idle<=0){e->moving=1;e->idle=1;}
  if(e->moving){e->x+=e->dir;if(fabsf(e->x-e->dest)<=1){e->moving=0;e->dir=-e->dir;e->dest=e->x+e->dir*135;}}
 }
 if(e->type==TNT&&e->exploded){
  gm_audio_play(A_EXPLODE);
  for(unsigned j=0;j<entityCount;j++)if(j!=index&&entities[j].active&&near(e->bx,e->by,50,entities[j].cx,entities[j].cy,entities[j].r))destroy_entity(&entities[j]);
  if(e->blast<=0){if(!e->grabbed)e->active=0;e->exploded=0;}
 }
}
void gm_init(void){rng^=aster_time_now_ms();gm_load(&highScore,&highLevel);if(highLevel<1)highLevel=1;gm_state=S_MENU;enter(S_MENU);}
void gm_update(float elapsed){
 if(gm_paused)return;accum=fminf(accum+elapsed,.25f);
 while(accum>=1/60.f){float dt=1/60.f;accum-=dt;stateAge+=dt;p.anim+=dt;if(p.useTimer>0)p.useTimer-=dt;
  if(gm_state==S_GOAL){if(stateAge>.15f&&!gm_audio_playing(A_GOAL))enter(S_GAME);}
  else if(gm_state==S_MADE_GOAL){if(!gm_audio_playing(A_MADE)){madeDelay-=dt;if(madeDelay<=0)enter(S_SHOP);}}
  else if(gm_state==S_SHOP){
   if(itemCount<=0)finishShopping=1;
   if(finishShopping){shopSad=!bought;shoppingTimer-=dt;if(shoppingTimer<=0)enter(S_GOAL);}
  }else if(gm_state==S_GAME){
   hook_update(dt);for(unsigned i=0;i<entityCount;i++)entity_update(i,dt);
   timeLeft-=dt;if(timeLeft<=0){timeLeft=0;enter(p.money>=p.goal?S_MADE_GOAL:S_OVER);}
   if(bonusView>0)bonusView-=dt;if(strengthView>0)strengthView-=dt;
  }
 }
}
static void buy(void){
 if(finishShopping||selectIndex<0||selectIndex>=itemCount)return;int item=items[selectIndex];if(p.money<prices[item]){shopSad=2;return;}
 bought=1;shopSad=0;aster_printf("GoldMiner buy item=%u price=%u",item,prices[item]);p.money-=prices[item];p.viewMoney-=prices[item];gm_audio_stop(A_MONEY);gm_audio_play(A_MONEY);
 if(item==0){if(p.bombs<12)p.bombs++;}else p.flags|=1<<(item-1);
 for(int i=selectIndex;i<itemCount-1;i++)items[i]=items[i+1];itemCount--;selectIndex=0;
}
void gm_key(const char *key){
 if(!strcmp(key,"pause")){gm_paused=!gm_paused;gm_audio_pause(gm_paused);return;}
 if(gm_paused){if(!strcmp(key,"return")){gm_paused=0;gm_audio_pause(0);}else if(!strcmp(key,"menu")){gm_paused=0;gm_audio_pause(0);enter(S_MENU);}else if(!strcmp(key,"quit"))gm_exit=1;return;}
 int ok=!strcmp(key,"return")||!strcmp(key,"j")||!strcmp(key,"k");
 if(gm_state==S_MENU){if(!strcmp(key,"up"))menuIndex=0;else if(!strcmp(key,"down"))menuIndex=1;else if(ok){if(menuIndex)enter(S_HIGH);else start();}}
 else if(gm_state==S_HIGH||gm_state==S_NEW_HIGH)enter(S_MENU);
 else if(gm_state==S_OVER){if(p.money>highScore){highScore=p.money;highLevel=p.level;gm_save(highScore,highLevel);enter(S_NEW_HIGH);}else enter(S_MENU);}
 else if(gm_state==S_SHOP){if(!strcmp(key,"left")){if(selectIndex>0)selectIndex--;}else if(!strcmp(key,"right")){if(selectIndex+1<itemCount)selectIndex++;}else if(ok)buy();else if(!strcmp(key,"space"))finishShopping=1;}
 else if(gm_state==S_GAME){if((!strcmp(key,"down")||ok)&&!h.showBonus&&!h.firing){h.firing=1;aster_printf("GoldMiner release angle=%d",(int)h.angle);gm_audio_play(A_START);}else if(!strcmp(key,"up"))use_bomb();else if(!strcmp(key,"space")&&p.money>=p.goal)enter(S_MADE_GOAL);}
}
void gm_touch(int x,int y){
 if(gm_paused){if(y>=75&&y<112)gm_key("return");else if(y>=112&&y<147)gm_key("menu");else if(y>=147&&y<185)gm_key("quit");return;}
 if(y>=240){if(x<52){gm_key("pause");return;}
  if(gm_state==S_SHOP)gm_key(x<107?"left":x<163?"right":x<242?"return":"space");
  else if(gm_state==S_GAME)gm_key(x<145?"down":x<235?"up":"space");
  else gm_key(x<115?"up":x<199?"down":"return");return;
 }
 if(gm_state==S_MENU){if(y>=169&&y<205)menuIndex=1;else menuIndex=0;gm_key("return");}
 else if(gm_state==S_GAME)gm_key("down");
 else if(gm_state==S_SHOP){for(int i=0;i<itemCount;i++){int center=30+items[i]*42;if(abs(x-center)<23&&y>=130&&y<=195){selectIndex=i;buy();break;}}}
 else gm_key("return");
}
static void number(int font,int value,float x,float y,uint16_t color,const char *prefix){char s[80];snprintf(s,sizeof(s),"%s%d",prefix,value);gm_text(font,s,x,y,color);}
static void goal_page(void){plain("bg_goal.png",0,0);plain("text_goldminer.png",54,20);plain("panel.png",27,80);}
static const char *propImage[]={"dynamite.png","strength_drink.png","lucky_clover.png","rock_collectors_book.png","gem_polish.png"};
static const char *description[]={
 "After you grab something, press UP\nto throw dynamite and blow it up.",
 "Strength drink. Reel up objects faster\non the next level. Lasts one level.",
 "Lucky clover. Better chances of good\nthings from grab bags. One level.",
 "Rock collector's book. Rocks are worth\nthree times as much on the next level.",
 "Gem polish. Gems and diamonds are\nworth more on the next level."};
void gm_render(void){
 if(gm_state==S_MENU){plain("bg_start_menu.png",0,0);gm_text(2,"Start Game",30,150,YELLOW);gm_text(2,"High Score",30,170,YELLOW);plain("menu_arrow.png",5,menuIndex?172:152);gm_text(1,"GoldMiner-Rebirth / Aster C",62,225,YELLOW);}
 else if(gm_state==S_HIGH||gm_state==S_NEW_HIGH){goal_page();gm_text(2,gm_state==S_HIGH?"High Score:":"New High Score:",70,100,YELLOW);char s[80];snprintf(s,sizeof(s),"$%d",highScore);gm_text(2,s,70,142,GREEN);number(2,highLevel,70,168,YELLOW,"at Level ");}
 else if(gm_state==S_GOAL){goal_page();gm_text(2,p.level==1?"Your First Goal is":"Your Next Goal is",70,100,YELLOW);number(2,p.goal,70,142,GREEN,"$");}
 else if(gm_state==S_MADE_GOAL){goal_page();gm_text(2,"You made it to\nthe next Level!",90,110,YELLOW);}
 else if(gm_state==S_OVER){goal_page();gm_text(2,"You didn't reach the goal!",50,130,YELLOW);}
 else if(gm_state==S_SHOP){
  plain("bg_shop.png",0,0);plain("text_goldminer.png",54,5);plain("ui_dialogue_bubble.png",25,70);
  gm_text(4,shopSad==2?"You don't seem to have any money\n:(":finishShopping?(bought?"Thank you for your patronage!\nGood luck!":"  :("):"Left/Right to select. Enter to buy.\nPress NEXT when you are ready.",30,75,0);
  image("shopkeeper_sheet.png",shopSad==1,220,100,0,1,1,0,0);plain("shop_table.png",7,176);
  for(int i=0;i<itemCount;i++){int item=items[i],x=30+item*42;const char *file=propImage[item];image(file,0,x,176,0,1,1,gm_width(file)/2.f,gm_height(file));number(4,prices[item],x-gm_width(file)/2,179,GREEN,"$");}
  if(!finishShopping&&itemCount&&selectIndex<itemCount){int item=items[selectIndex];image("ui_selector.png",0,30+item*42,130,0,1,1,6,14);gm_text(1,description[item],25,195,YELLOW);}
 }else if(gm_state==S_GAME){
  plain("bg_top.png",0,0);char bg[32];snprintf(bg,sizeof(bg),"bg_level_%c.png",'A'+layout->bg);plain(bg,0,40);
  gm_text(4,"Money",5,5,0x6200);number(4,p.viewMoney,47,5,0x0340," $");gm_text(4,"Goal",11,15,0x6200);number(4,p.goal,47,15,0x0340," $");
  gm_text(4,"Time:",260,15,0x6200);number(4,(int)timeLeft,290,15,0x8200,"");gm_text(4,"Level:",250,25,0x6200);number(4,p.level,290,25,0x8200,"");
  if(p.money>=p.goal)gm_text(4,"Press NEXT to Skip",200,5,ORANGE);
  if(bonusView>0)number(5,p.currentBonus,90,18,GREEN,"$");
  if(strengthView>0)plain("text_strength.png",80,10);
  int frame=0;if(strengthView>0)frame=((int)(p.anim/.13f)%2)?7:6;else if(p.useTimer>0)frame=3+(int)(p.anim/.13f)%3;else if(h.firing)frame=h.backing?(int)(p.anim/.13f)%3:2;
  image("miner_sheet.png",frame,165,39,0,1,1,16,40);
  for(unsigned i=0;i<entityCount;i++){
   Entity *e=&entities[i];if(!e->active)continue;
   if(e->shine>0)image("gold_big_fx_sheet.png",(int)(e->anim/.2f)%9,e->x,e->y,h.angle,1,1,27,27);
   if(e->type==TNT&&e->exploded&&e->blast>0)image("bigger_explosive_fx_sheet.png",(int)((.5f-e->blast)/.2f)%12,e->bx,e->by,h.angle,1,1,50,50);
   if(e->type==TNT&&(e->exploded||e->grabbed)){image("tnt_destroyed.png",0,e->x,e->y,h.angle,1,1,e->w/3.f,e->h/10.f);continue;}
   if(e->type==MOLE||e->type==DIAMONDMOLE)image(images[e->type],e->moving?(int)(e->anim/.15f)%7:0,e->x,e->y,e->grabbed?h.angle:0,-e->dir,1,9,6.5f);
   else image(images[e->type],0,e->x,e->y,e->grabbed?h.angle:0,1,1,e->grabbed?e->w/2.f:0,e->grabbed?e->h/3.f:0);
  }
  for(int i=0;i<p.bombs;i++)plain("ui_dynamite.png",195+(i%6)*5,i<6?22:12);
  gm_line(158,30,h.x,h.y,0x4208,255);image("hook_sheet.png",h.frame,h.x,h.y,h.angle,1,1,6.5,0);
  if(h.explosion>0)image("explosive_fx_sheet.png",(int)((.5f-h.explosion)/.2f)%12,h.ex,h.ey,h.angle,1,1,32,32);
 }
 if(gm_paused){gm_rect(0,0,320,240,0,170);gm_rect(48,42,224,158,0x2104,255);gm_text(1,"PAUSED",120,52,0xffff);gm_text(1,"CONTINUE / ENTER",92,88,0xffff);gm_text(1,"RETURN TO MENU",92,123,YELLOW);gm_text(1,"QUIT GAME",92,158,0xf800);}
}
