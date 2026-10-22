/*
 * GPL-3.0-or-later. Road projection, layered geometry and sprite positioning
 * adapted from Ruben Rodriguez, OutRun Line.cpp / Map.cpp.
 * Aster backend rasterizes into the public RGB565 surface.
 */
#include "engine.h"
#include "catalog.h"
#include "ui.h"
uint16_t *pixels;uint32_t screen_w,screen_h,stride;View views[DRAW_DISTANCE];
/* Exact /255 blend: red and blue use independent 16-bit lanes, no rounding change. */
static uint16_t mix(uint16_t a,uint16_t b,int alpha){
 uint32_t inv=255-alpha,pa=(a&31u)|((uint32_t)(a&0xf800u)<<5),pb=(b&31u)|((uint32_t)(b&0xf800u)<<5);
 uint32_t rb=pa*inv+pb*alpha;
 rb=((rb+0x00010001u+((rb>>8)&0x00ff00ffu))>>8)&0x001f001fu;
 uint32_t g=((a>>5)&63u)*inv+((b>>5)&63u)*alpha;
 g=(g+1+(g>>8))>>8;
 return (rb&31u)|((rb>>5)&0xf800u)|(g<<5);
}
/* Aligned two-pixel stores reduce guest bounds checks and PSRAM write instructions. */
typedef uint32_t FillWord __attribute__((__may_alias__));
static void fill_pixels(uint16_t *p,int n,uint16_t color){
 if(n<=0)return;if(((uintptr_t)p&3u)&&n){*p++=color;n--;}
 FillWord *wide=(FillWord*)p;uint32_t v=color|((uint32_t)color<<16);
 while(n>=8){wide[0]=v;wide[1]=v;wide[2]=v;wide[3]=v;wide+=4;n-=8;}
 while(n>=2){*wide++=v;n-=2;}if(n)*(uint16_t*)wide=color;
}
void rect(int x,int y,int w,int h,uint16_t color){
 int x0=MAX(x,0),x1=MIN(x+w,(int)screen_w),y0=MAX(y,0),y1=MIN(y+h,(int)screen_h);
 for(int yy=y0;yy<y1;yy++)fill_pixels(pixels+yy*stride+x0,x1-x0,color);
}
void blend_rect(int x,int y,int w,int h,uint16_t c,int a){
 for(int yy=MAX(0,y);yy<MIN(y+h,(int)screen_h);yy++)for(int xx=MAX(0,x);xx<MIN(x+w,(int)screen_w);xx++){uint16_t *p=pixels+yy*stride+xx;*p=mix(*p,c,a);}
}
void blit(Image *im,int x,int y,int w,int h,int clip,int alpha){
 if(!im||w<1||h<1||w>50000||h>50000||alpha<=0)return;
 int x0=MAX(0,x),x1=MIN(x+w,(int)screen_w),y0=MAX(0,y),y1=MIN(MIN(y+h,clip),(int)screen_h);
 if(x0>=x1||y0>=y1)return;
 /* Sampling stays identical to the original 16.16 nearest-neighbour path. */
 int count=x1-x0;uint16_t columns[count];
 uint32_t step=((uint32_t)im->w<<16)/w,tx=(uint32_t)(x0-x)*step,tx0=tx;
 for(int i=0;i<count;i++,tx+=step)columns[i]=MIN(tx>>16,im->w-1);
 for(int yy=y0;yy<y1;yy++){
  uint32_t sy=(uint32_t)(yy-y)*im->h/h;const uint16_t *src=im->pixel+sy*im->w;uint16_t *dst=pixels+yy*stride+x0;
  int first=0,last=count;
  if(im->left){uint32_t l=im->left[sy],r=im->right[sy];if(l>=r)continue;
   if(l>columns[0])first=MIN((int)(((l<<16)-tx0+step-1)/step),count);
   if(r<=columns[count-1])last=MIN((int)(((r<<16)-tx0+step-1)/step),count);
   if(first>=last)continue;
  }
  if(alpha==255){
   if(!im->alpha){for(int i=first;i<last;i++){uint16_t col=src[columns[i]];if(col)dst[i]=col;}}
   else {const uint8_t *cover=im->alpha+sy*im->w;
    for(int i=first;i<last;i++){unsigned sx=columns[i],a=cover[sx];if(!a)continue;uint16_t col=src[sx];if(!col)continue;dst[i]=a==255?col:mix(dst[i],col,a);}
   }
  }else if(!im->alpha){for(int i=first;i<last;i++){uint16_t col=src[columns[i]];if(col)dst[i]=mix(dst[i],col,alpha);}}
  else {const uint8_t *cover=im->alpha+sy*im->w;
   for(int i=first;i<last;i++){unsigned sx=columns[i],a=alpha*cover[sx]/255;if(!a)continue;uint16_t col=src[sx];if(col)dst[i]=a==255?col:mix(dst[i],col,a);}
  }
 }
}
static uint16_t glyph_rows[96][16];static bool glyph_ready;
void text_at(int x,int y,const char *s,uint16_t color,int scale){
 if(!font.pixel||scale<1)return;
 if(!glyph_ready){
  for(int c=0;c<96;c++){for(int sy=0;sy<16;sy++){
  uint16_t bits=0;for(int sx=0;sx<12;sx++)if(font.pixel[((c/16)*16+sy)*font.w+(c%16)*16+sx])bits|=1u<<sx;
  glyph_rows[c][sy]=bits;
  }}
  glyph_ready=true;
 }
 for(;*s;s++,x+=8*scale){unsigned ch=(uint8_t)*s;if(ch<32||ch>127)continue;
  for(int sy=0;sy<16;sy++){int yy=y+sy*scale;uint32_t bits=glyph_rows[ch-32][sy];
   if(yy>=(int)screen_h||yy+scale<=0)continue;
   while(bits){unsigned sx=__builtin_ctz(bits);bits&=bits-1;int xx=x+sx*scale;
    if(scale==1){if(xx>=0&&xx<(int)screen_w&&yy>=0)pixels[yy*stride+xx]=color;}
    else if(scale==2&&xx>=0&&xx+1<(int)screen_w&&yy>=0&&yy+1<(int)screen_h){uint16_t *dst=pixels+yy*stride+xx;dst[0]=dst[1]=dst[stride]=dst[stride+1]=color;}
    else rect(xx,yy,scale,scale,color);
   }
  }
 }
}
static Projected project(float world_x,float world_y,float z,float cam_x,float cam_y){
 float s=CAMERA_D/MAX(.1f,z);Projected p={screen_w*.5f+screen_w*.5f*(world_x-cam_x)*s,
 screen_h*.5f-screen_h*.5f*(world_y-cam_y)*s, s*ROAD_W*screen_w*.5f,s};return p;
}
/* Paint visible road colour intervals once; nested higher layers exclude lower layers. */
static void span(uint16_t *row,int x0,int x1,uint16_t c){
 fill_pixels(row+x0,x1-x0,c);
}
static void except_spans(uint16_t *row,int x0,int x1,int a,int b,int c,int d,uint16_t color){
 if(a>c){int t=a;a=c;c=t;t=b;b=d;d=t;}
 if(b>x0&&a<x1){span(row,x0,MIN(a,x1),color);x0=MAX(x0,b);}
 if(d>x0&&c<x1){span(row,x0,MIN(c,x1),color);x0=MAX(x0,d);}
 span(row,x0,x1,color);
}
static void road_layers(View *v,int light,int off){
 float ya=v->a.y+off,yb=v->b.y+off;if(ya<=yb||ya<0||yb>=screen_h)return;
 int lo=MAX(0,(int)ceilf(yb)),hi=MIN((int)screen_h,(int)ceilf(ya));float inv=1.f/(ya-yb);
 const float limits[][2]={{-1.142857f,1.142857f},{-1,1},{-.944444f,.944444f},{-.351852f,.351852f},{-.296296f,.296296f}};
 uint16_t colors[]={map.colors[light?5:6],map.colors[light?9:10],map.colors[light?3:4],map.colors[light?7:8],map.colors[light?3:4]};
 for(int y=lo;y<hi;y++){
  float t=(y+.5f-yb)*inv,center0=v->b.x+(v->a.x-v->b.x)*t,center1=v->d.x+(v->c.x-v->d.x)*t,width=v->b.w+(v->a.w-v->b.w)*t;
  int l[5][2],r[5][2];uint16_t *row=pixels+y*stride;
  for(int i=0;i<5;i++){float left=width*limits[i][0],right=width*limits[i][1];
   l[i][0]=CLAMP((int)CLAMP(center0+left,-1e6f,1e6f),0,(int)screen_w);r[i][0]=CLAMP((int)CLAMP(center0+right,-1e6f,1e6f),0,(int)screen_w);
   l[i][1]=CLAMP((int)CLAMP(center1+left,-1e6f,1e6f),0,(int)screen_w);r[i][1]=CLAMP((int)CLAMP(center1+right,-1e6f,1e6f),0,(int)screen_w);
  }
  for(int i=0;i<4;i++)for(int side=0;side<2;side++)except_spans(row,l[i][side],r[i][side],l[i+1][0],r[i+1][0],l[i+1][1],r[i+1][1],colors[i]);
  span(row,l[4][0],r[4][0],colors[4]);span(row,l[4][1],r[4][1],colors[4]);
 }
}static void background(int horizon){
 int h=(int)(253.f*screen_h/691.f);
 for(int layer=0;layer<2;layer++){
  Image *im=get_image(game.map_id?bg_ids[game.map_id-1][layer]:goal_bg_ids[layer]);if(!im)continue;int w=im->ow*screen_w/921;int shift=(int)((1000+game.bg_offset*(layer?1.25f:1.f))*screen_w/921.f)%w;
  if(shift<0)shift+=w;blit(im,-shift,horizon-h,w,h,horizon,255);blit(im,w-shift,horizon-h,w,h,horizon,255);
 }
}
static void scenery(View *v,Placement *o,int off){
 ObjectMeta *meta=&map.meta[o->object];uint32_t aid=meta->image;
 if((game.map_id==1&&o->line==306&&o->object==0)||(game.map_id==0&&o->object==0)){uint32_t frame=game.state==1?(uint32_t)CLAMP((3-game.countdown)*4,0,13):MIN(13,game.steps/8);meta=&map.meta[frame];aid=meta->image;}
 /* Keep start semaphore animation's original frame IDs. */
 if(game.map_id==1&&o->line==311&&o->object>=38&&o->object<=41){
  uint32_t base=38,phase=game.state==1?(uint32_t)CLAMP(3-game.countdown,0,2):3;
  if(base+phase<map.h.metas)meta=&map.meta[base+phase];aid=meta->image;
 }
 Projected p=o->side?v->c:v->a;if(p.scale<=0)return;
 float x=p.x+o->x*p.scale*ROAD_W*screen_w*.5f;
 float y=p.y+o->y*p.scale*1000.f*screen_h*.5f;
 float basis=p.scale*screen_w*.5f*((.3f/170.f)*ROAD_W)*meta->scale;
 float dw=image_size[aid][0]*basis*3.2f,dh=image_size[aid][1]*basis*3.43f;
 if(dw<1||dh<1)return;
 float pivotx=o->x>=0?meta->prx:meta->plx,pivoty=o->x>=0?meta->pry:meta->ply;
 if(x-dw*pivotx>screen_w||x+dw*(1-pivotx)<0||y+off-dh*pivoty>=v->clip+off||y+off+dh*(1-pivoty)<0)return;
 Image *im=get_image(aid);if(!im)return;
 blit(im,(int)(x-dw*pivotx),(int)(y+off-dh*pivoty),(int)dw,(int)dh,(int)v->clip+off,255);
}
static void traffic_draw(View *v,Traffic *car,int off){
 Projected p=car->side?v->c:v->a,q=car->side?v->d:v->b;float t=fmodf(car->z,SEGMENT)/SEGMENT;
 float scale=p.scale+(q.scale-p.scale)*t,x=p.x+(q.x-p.x)*t,y=p.y+(q.y-p.y)*t;
 x+=car->offset*scale*ROAD_W*screen_w*.5f;
 int direction=game.x-(car->offset+(car->side?map.line[v->line].distance/ROAD_W:0))<0?-1:1;
 int frame=(game.steps/3)%4+(direction<0?4:8);
 uint32_t id=traffic_ids[car->model%TRAFFIC_MODELS][frame];
 float basis=scale*screen_w*.5f*((.3f/170.f)*ROAD_W)*traffic_scale[car->model%TRAFFIC_MODELS];
 int dw=image_size[id][0]*basis*3.2f,dh=image_size[id][1]*basis*3.43f;
 int bx=(int)x-dw/2,by=(int)y+off-dh;
 if(dw<1||dh<1||bx>=(int)screen_w||bx+dw<=0||by>=MIN((int)v->clip+off,(int)screen_h)||by+dh<=0)return;
 Image *im=get_image(id);if(!im)return;
 blit(im,bx,by,dw,dh,(int)v->clip+off,255);
}
static void player_draw(void){
 if(game.crash>0&&game.crash_phase==2&&game.steps%12<6)return;int frame=game.body_frame;Image *im=get_image(player_ids[CLAMP(frame-1,0,147)]);if(!im)return;
 int w=im->ow*3.2f*screen_w/921.f,h=im->oh*3.2f*screen_h/691.f;
 int x=(screen_w-w)/2,y=screen_h*.84f-h*.25f;
 if(game.offroad&&game.speed>0)y+=(game.steps/5)%2?-1:0;
 blit(im,x,y,w,h,screen_h,255);
 if((game.brake||game.offroad||map.h.road_terrain>0||game.crash>0||game.speed<45)&&game.speed>0){
  int code=(game.offroad||map.h.road_terrain>0)?4+(CLAMP(game.offroad?map.h.terrain:map.h.road_terrain,1,4)-1)*6+(game.steps/3)%6:(game.steps/3)%4;
  Image *smoke=get_image(wheel_ids[code]);if(smoke){
   int sw=smoke->ow*2.5f*screen_w/921.f,sh=smoke->oh*2.5f*screen_w/921.f;
   blit(smoke,screen_w/2-(int)(sw*1.2f),y+h-sh,sw,sh,screen_h,255);
   blit(smoke,screen_w/1.9f,y+h-sh,sw,sh,screen_h,255);
  }
 }
}
static void hud_image(int id,int x,int y,int w){
 Image *im=get_image(id);if(im)blit(im,x,y,w,(int)((float)w*im->oh/im->ow),screen_h,255);
}
static void ui_button(int id,const char *label,bool active){
 UiRect r=ui_rect(id);blend_rect(r.x,r.y,r.w,r.h,active?0x3186:0x0841,active?155:90);
 rect(r.x,r.y,r.w,1,0x8410);rect(r.x,r.y+r.h-1,r.w,1,0x8410);
 if(id==UI_LEFT||id==UI_RIGHT)text_at(r.x+(r.w-16)/2,r.y+r.h-32,label,0xffff,2);
 else {int ty=(id==UI_BRAKE||id==UI_GAS)?r.y+r.h-20:r.y+(r.h-16)/2;
  text_at(r.x+(r.w-(int)strlen(label)*8)/2,ty,label,0xffff,1);
 }
}
static void hud(void){
 char b[80];int sx=screen_w,sy=screen_h;
 hud_image(HUD_TIME_INDICATOR,12,10,40);aster_snprintf(b,sizeof(b),"%03u",(unsigned)MAX(0,game.remaining));text_at(58,8,b,0xffe0,1);
 hud_image(HUD_SCORE_INDICATOR,sx*31/100,10,42);aster_snprintf(b,sizeof(b),"%08u",(unsigned)game.score);text_at(sx*42/100,8,b,0xffff,1);
 hud_image(HUD_LAP_INDICATOR,sx*70/100,10,34);aster_snprintf(b,sizeof(b),"%02u'%02u",(unsigned)game.lap/60,(unsigned)game.lap%60);text_at(sx*80/100,8,b,0xffff,1);
 hud_image(HUD_SPEED_INDICATOR,12,sy-46,34);aster_snprintf(b,sizeof(b),"%03u",(unsigned)(game.speed*1.65f));text_at(51,sy-52,b,0xffe0,2);
 hud_image(HUD_GEAR_INDICATOR,sx-104,sy-58,44);text_at(sx-45,sy-62,game.gear?"H":"L",0xffe0,1);
 aster_snprintf(b,sizeof(b),"STAGE %u",game.stage);text_at(sx-103,sy-82,b,0xffff,1);
 if(game.state!=0){ui_button(UI_MENU,"MENU",game.menu_open);
  if(game.state==1||game.state==2){
   ui_button(UI_LEFT,"<",game.steer<0);ui_button(UI_RIGHT,">",game.steer>0);
   ui_button(UI_BRAKE,"BRAKE",game.brake);ui_button(UI_AUTO,game.auto_gas?"AUTO GAS":"MANUAL",false);
   if(!game.auto_gas)ui_button(UI_GAS,"GAS",game.gas||game.touch_gas);
  }
 }
 if(game.state==0){
  text_at(sx/2-56,62,"OUT RUN",0xffe0,2);
  ui_button(UI_PREV,"<",false);ui_button(UI_NEXT,">",false);
  aster_snprintf(b,sizeof(b),"CIRCUIT %02u",game.map_id);text_at(sx/2-40,128,b,0xffff,1);
  ui_button(UI_START,"START RACE",false);
 }else if(game.state==1){
  aster_snprintf(b,sizeof(b),"READY  %u",(unsigned)ceilf(game.countdown));text_at(sx/2-32,sy/2-24,b,0xffe0,1);
 }else if(game.state==3||game.state==4){
  blend_rect(sx/2-148,112,296,132,0x0001,170);
  text_at(sx/2-36,132,game.state==3?"TIME OVER":"GOAL!",0xffe0,1);
  ui_button(UI_START,"RETRY RACE",false);
 }else if(game.crash>0)text_at(sx/2-20,sy/2-40,"CRASH",0xf800,1);
 if(game.menu_open){
  blend_rect(0,0,sx,sy,0,145);rect(sx/2-148,56,296,232,0x1082);
  text_at(sx/2-32,65,"MENU",0xffe0,2);
  ui_button(UI_RESUME,"RESUME",game.menu_choice==0);
  ui_button(UI_RESTART,"RESTART",game.menu_choice==1);
  ui_button(UI_MAPS,"SELECT MAP",game.menu_choice==2);
 }else if(game.paused){blend_rect(0,0,sx,sy,0,120);text_at(sx/2-24,sy/2,"PAUSED",0xffff,1);}

}
void draw_frame(void){
 if(!pixels)return;
 if(!map.line){rect(0,0,screen_w,screen_h,0x0841);text_at(16,60,"OUTRUN RESOURCES MISSING",0xffff,1);text_at(16,90,"COPY SDCARD DIRECTORY FIRST",0xffe0,1);return;}
 uint32_t measured=aster_time_now_ms();
 rect(0,0,screen_w,screen_h,map.colors[0]);
 int base=(int)(game.position/SEGMENT)%map.h.count,off=(int)(130.f*screen_h/691.f);
 float playerz=game.position+PLAYER_Z;uint32_t pi=(uint32_t)(playerz/SEGMENT)%map.h.count;RoadLine *pl=&map.line[pi];
 float frac=fmodf(playerz,SEGMENT)/SEGMENT,cy=800.f+pl->y0+(pl->y1-pl->y0)*frac;
 float percent=fmodf(game.position,SEGMENT)/SEGMENT,dx=-map.line[base].curve*percent,sum=0,maxY=screen_h;
 for(int n=0;n<DRAW_DISTANCE;n++){
  uint32_t index=(base+n)%map.h.count;RoadLine *l=&map.line[index];View *v=&views[n];v->line=index;v->clip=maxY;v->valid=false;
  float z=(base+n)*SEGMENT-game.position;
  float cx=game.x*ROAD_W;
  v->a=project(0,l->y0,z,cx-sum,cy);v->b=project(0,l->y1,z+SEGMENT,cx-sum-dx,cy);
  v->c=project(0,l->y0,z,cx+(l->mirror?sum:-sum)-l->distance,cy);
  v->d=project(0,l->y1,z+SEGMENT,cx+(l->mirror?sum+dx:-sum-dx)-l->distance,cy);
  sum+=dx;dx+=l->curve;
  if(z<=CAMERA_D||v->b.y>=maxY)continue;
  v->valid=true;int light=(index/3)%2;
  int y0=MAX(0,(int)(v->b.y+off)),y1=MIN((int)screen_h,(int)(v->a.y+off));
  if(y1>y0)rect(0,y0,screen_w,y1-y0,map.colors[light?1:2]);
  road_layers(v,light,off);maxY=v->b.y;
 }
 profile.road+=(uint32_t)(aster_time_now_ms()-measured);measured=aster_time_now_ms();
 background(CLAMP((int)maxY+off,0,(int)screen_h));
 profile.bg+=(uint32_t)(aster_time_now_ms()-measured);measured=aster_time_now_ms();
 for(int n=DRAW_DISTANCE-1;n>=0;n--){
  View *v=&views[n];float ahead=(base+n)*SEGMENT-game.position;if(ahead<PLAYER_Z-SEGMENT)continue;
  for(uint32_t j=map.first[v->line];j<map.first[v->line+1];j++)scenery(v,&map.obj[j],off);
  for(int c=0;game.map_id&&c<20;c++)if((uint32_t)(cars[c].z/SEGMENT)%map.h.count==v->line&&cars[c].z>playerz)traffic_draw(v,&cars[c],off);
 }
 player_draw();profile.sprites+=(uint32_t)(aster_time_now_ms()-measured);measured=aster_time_now_ms();hud();profile.hud+=(uint32_t)(aster_time_now_ms()-measured);
}









