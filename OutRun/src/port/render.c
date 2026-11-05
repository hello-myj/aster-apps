/*
 * GPL-3.0-or-later. Road projection, layered geometry and sprite positioning
 * adapted from Ruben Rodriguez, OutRun Line.cpp / Map.cpp.
 * Aster backend rasterizes into the public RGB565 surface.
 */
#include "engine.h"
#include "catalog.h"
#include "ui.h"
#include "aster_gfx.h"
uint16_t *pixels;uint32_t screen_w,screen_h,stride;View views[DRAW_DISTANCE];static uint32_t view_h;
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
 view_h*.5f-view_h*.5f*(world_y-cam_y)*s, s*ROAD_W*screen_w*.5f,s};return p;
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
 float ya=v->a.y+off,yb=v->b.y+off;if(ya<=yb||ya<0||yb>=view_h)return;
 int lo=MAX(0,(int)ceilf(yb)),hi=MIN((int)view_h,(int)ceilf(ya));float inv=1.f/(ya-yb);
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
 int h=(int)(253.f*view_h/691.f);
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
 float y=p.y+o->y*p.scale*1000.f*view_h*.5f;
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
 if(dw<1||dh<1||bx>=(int)screen_w||bx+dw<=0||by>=MIN((int)v->clip+off,(int)view_h)||by+dh<=0)return;
 Image *im=get_image(id);if(!im)return;
 blit(im,bx,by,dw,dh,(int)v->clip+off,255);
}
static void player_draw(void){
 if(game.crash>0&&game.crash_phase==2&&game.steps%12<6)return;int frame=game.body_frame;Image *im=get_image(player_ids[CLAMP(frame-1,0,147)]);if(!im)return;
 int w=im->ow*3.2f*screen_w/921.f,h=im->oh*3.2f*view_h/691.f;
 int x=(screen_w-w)/2,y=view_h*.84f-h*.25f;
 if(game.offroad&&game.speed>0)y+=(game.steps/5)%2?-1:0;
 blit(im,x,y,w,h,view_h,255);
 if((game.brake||game.offroad||map.h.road_terrain>0||game.crash>0||game.speed<45)&&game.speed>0){
  int code=(game.offroad||map.h.road_terrain>0)?4+(CLAMP(game.offroad?map.h.terrain:map.h.road_terrain,1,4)-1)*6+(game.steps/3)%6:(game.steps/3)%4;
  Image *smoke=get_image(wheel_ids[code]);if(smoke){
   int sw=smoke->ow*2.5f*screen_w/921.f,sh=smoke->oh*2.5f*screen_w/921.f;
   blit(smoke,screen_w/2-(int)(sw*1.2f),y+h-sh,sw,sh,view_h,255);
   blit(smoke,screen_w/1.9f,y+h-sh,sw,sh,view_h,255);
  }
 }
}
static void tiny_text(aster_gfx_t *g,int x,int y,const char *text,unsigned scale,uint16_t color){
 for(;*text;text++,x+=6*(int)scale){unsigned c=(unsigned char)*text;if(c<32||c>126)c='?';const uint8_t *glyph=aster_font5x7[c-32];
  for(unsigned yy=0;yy<7;yy++)for(unsigned xx=0;xx<5;xx++)if(glyph[yy]&(1u<<(4-xx)))aster_gfx_fill_rect(g,x+xx*scale,y+yy*scale,scale,scale,color);}
}
static void rounded(aster_gfx_t *g,int x,int y,int w,int h,int r,uint16_t color){
 aster_gfx_fill_rect(g,x+r,y,w-2*r,h,color);aster_gfx_fill_rect(g,x,y+r,w,h-2*r,color);
 aster_gfx_disk(g,x+r,y+r,r,color);aster_gfx_disk(g,x+w-r-1,y+r,r,color);aster_gfx_disk(g,x+r,y+h-r-1,r,color);aster_gfx_disk(g,x+w-r-1,y+h-r-1,r,color);
}
static void ui_button(int id,const char *label,bool active){
 UiRect r=ui_rect(id);aster_gfx_t g;aster_gfx_init(&g,pixels,screen_w,screen_h,stride*2);
 bool pressed=outrun_control_held(id);uint16_t accent=id==UI_AUTO?0x3d5f:0x349f;
 rounded(&g,r.x,r.y,r.w,r.h,6,pressed?0xffff:active?accent:0x526f);
 rounded(&g,r.x+2,r.y+2,r.w-4,r.h-4,4,pressed?accent:0x2127);
 unsigned scale=ui_large()&&id!=UI_MENU&&id!=UI_AUTO?2:1;
 tiny_text(&g,r.x+(r.w-(int)strlen(label)*6*(int)scale)/2,r.y+(r.h-7*(int)scale)/2,label,scale,0xffff);
}
static void pedal(aster_gfx_t *g,int id,const char *name,bool down,uint16_t accent){
 UiRect r=ui_rect(id);
 if(ui_large()){rounded(g,r.x,r.y,r.w,r.h,7,down?0xffff:0x9cd3);rounded(g,r.x+2,r.y+2,r.w-4,r.h-4,5,down?accent:0x2127);}
 else {blend_rect(r.x,r.y,r.w,r.h,down?accent:0x10c4,down?210:125);
  uint16_t edge=down?0xffff:0x9cd3;aster_gfx_line(g,r.x+3,r.y,r.x+r.w-4,r.y,edge);aster_gfx_line(g,r.x+3,r.y+r.h-1,r.x+r.w-4,r.y+r.h-1,edge);
  aster_gfx_line(g,r.x,r.y+3,r.x,r.y+r.h-4,edge);aster_gfx_line(g,r.x+r.w-1,r.y+3,r.x+r.w-1,r.y+r.h-4,edge);}
 for(int y=r.y+10;y<r.y+r.h-21;y+=8)aster_gfx_fill_rect(g,r.x+8,y,r.w-16,3,down?0xffff:0x738e);
 tiny_text(g,r.x+(r.w-(int)strlen(name)*6)/2,r.y+r.h-13,name,1,0xffff);
}
static void wheel(aster_gfx_t *g){
 int x=ui_wheel_x(),y=ui_wheel_y(),r=ui_wheel_radius(),delta=outrun_wheel_offset();
 if(!delta&&game.steer)delta=game.steer*r*3/4;
 int step=CLAMP((delta<0?-delta:delta)*3/(r*3/4),0,3);const int sine[]={0,265,512,724},cosine[]={1024,989,887,724};
 int sn=sine[step]*(delta<0?-1:1),cs=cosine[step];bool down=game.steer||outrun_control_held(UI_WHEEL);
 aster_gfx_disk(g,x,y,r+2,down?0x5d9e:0x526f);aster_gfx_disk(g,x,y,r-2,0x10c4);
 aster_gfx_disk(g,x,y,r-8,0x2948);aster_gfx_disk(g,x,y,r-12,0x10c4);
 const int vx[]={0,-7,7},vy[]={-9,6,6};
 for(int i=0;i<3;i++){int px=r*vx[i]/12,py=r*vy[i]/12;int xx=x+(px*cs-py*sn)/1024,yy=y+(px*sn+py*cs)/1024;
  for(int d=-1;d<=1;d++)aster_gfx_line(g,x+d,y,xx+d,yy,down?0xdfff:0x9cd3);}
 aster_gfx_disk(g,x,y,r/5,down?0x5d9e:0x526f);tiny_text(g,x-9,y-3,"RUN",1,0xffff);
 aster_gfx_disk(g,x+(r-5)*sn/1024,y-(r-5)*cs/1024,3,0xfd20);
}
static void hud(void){
 char b[80];int sx=screen_w,sy=screen_h,cy=ui_play_height()/2;aster_gfx_t g;aster_gfx_init(&g,pixels,screen_w,screen_h,stride*2);
 if(ui_large())rect(0,view_h,sx,sy-view_h,0x10c4);
 blend_rect(0,0,sx,39,0x0841,210);
 tiny_text(&g,8,6,"TIME",1,0x9cd3);aster_snprintf(b,sizeof(b),"%03u",(unsigned)MAX(0,game.remaining));tiny_text(&g,8,20,b,2,0xffe0);
 tiny_text(&g,64,6,"SCORE",1,0x9cd3);aster_snprintf(b,sizeof(b),"%08u",(unsigned)game.score);tiny_text(&g,64,24,b,1,0xffff);
 if(ui_large()||!outrun_motion_available()){tiny_text(&g,128,6,"STAGE",1,0x9cd3);aster_snprintf(b,sizeof(b),"%u",game.stage);tiny_text(&g,128,24,b,1,0xffff);}
 if(game.state==1||game.state==2){
  if(!outrun_motion_enabled())wheel(&g);
  if(ui_large()){pedal(&g,UI_BRAKE,"BRAKE",game.brake,0xc946);pedal(&g,UI_GAS,"GAS",game.gas||game.auto_gas,0x254a);}
  int x=ui_large()?156:sx/2-38,y=ui_large()?sy-83:48;
  if(!ui_large())blend_rect(x-4,y-4,88,26,0x10c4,155);
  if(ui_large())tiny_text(&g,x,y,"SPEED",1,0x9cd3);
  aster_snprintf(b,sizeof(b),"%03u",(unsigned)(game.speed*1.65f));tiny_text(&g,x,y+(ui_large()?13:0),b,2,0xffff);tiny_text(&g,x+42,y+(ui_large()?21:8),"KM/H",1,0x9cd3);
  if(ui_large())tiny_text(&g,x,y+36,game.gear?"GEAR H":"GEAR L",1,0x9cd3);
  if(!ui_large()&&outrun_motion_available())ui_button(UI_MODE,outrun_motion_enabled()?"TILT":"TOUCH",outrun_motion_enabled());
  if(outrun_motion_enabled())ui_button(UI_CENTER,"CENTER",false);
 }
 if(game.state!=0)ui_button(UI_MENU,"MENU",game.menu_open);
 if(game.state==1||game.state==2)ui_button(UI_AUTO,game.auto_gas?"AUTO ON":"AUTO OFF",game.auto_gas);
 if(game.state==0){
  blend_rect(sx/2-(ui_large()?156:124),cy-82,ui_large()?312:248,164,0x0841,190);
  tiny_text(&g,sx/2-42,cy-70,"OUT RUN",2,0xffe0);ui_button(UI_PREV,"<",false);ui_button(UI_NEXT,">",false);
  aster_snprintf(b,sizeof(b),"CIRCUIT %02u",game.map_id);tiny_text(&g,sx/2-30,cy-20,b,1,0xffff);ui_button(UI_START,"START RACE",false);
  if(ui_large())tiny_text(&g,sx/2-48,ui_play_height()-20,"HOLD GAS + STEER",1,0x9cd3);
  else {blend_rect(0,sy-38,sx,38,0x0841,180);tiny_text(&g,sx/2-42,sy-34,outrun_motion_enabled()?"TILT TO STEER":"DRAG THE WHEEL",1,0x9cd3);tiny_text(&g,sx/2-66,sy-18,"LEFT BRAKE / RIGHT GAS",1,0x9cd3);}
 }else if(game.state==1){aster_snprintf(b,sizeof(b),"READY %u",(unsigned)ceilf(game.countdown));tiny_text(&g,sx/2-42,cy-14,b,2,0xffe0);}
 else if(game.state==3||game.state==4){blend_rect(sx/2-124,cy-60,248,140,0x0841,210);tiny_text(&g,sx/2-54,cy-38,game.state==3?"TIME OVER":"GOAL!",2,0xffe0);ui_button(UI_START,"RETRY RACE",false);}
 else if(game.crash>0)tiny_text(&g,sx/2-30,cy-36,"CRASH",2,0xf800);
 if(game.menu_open){
  blend_rect(0,0,sx,sy,0,170);rounded(&g,sx/2-(ui_large()?152:124),sy/2-112,ui_large()?304:248,216,10,0x10c4);
  tiny_text(&g,sx/2-36,sy/2-96,"PAUSED",2,0xffe0);
  ui_button(UI_RESUME,"RESUME",game.menu_choice==0);ui_button(UI_RESTART,"RESTART",game.menu_choice==1);ui_button(UI_MAPS,"SELECT MAP",game.menu_choice==2);
 }else if(game.paused){blend_rect(0,0,sx,sy,0,120);tiny_text(&g,sx/2-36,sy/2,"PAUSED",2,0xffff);}
}
void draw_frame(void){
 if(!pixels)return;view_h=(uint32_t)ui_play_height();
 if(!map.line){rect(0,0,screen_w,view_h,0x0841);text_at(16,60,"OUTRUN RESOURCES MISSING",0xffff,1);text_at(16,90,"COPY SDCARD DIRECTORY FIRST",0xffe0,1);return;}
 uint32_t measured=aster_time_now_ms();
 rect(0,0,screen_w,view_h,map.colors[0]);
 int base=(int)(game.position/SEGMENT)%map.h.count,off=(int)(130.f*view_h/691.f);
 float playerz=game.position+PLAYER_Z;uint32_t pi=(uint32_t)(playerz/SEGMENT)%map.h.count;RoadLine *pl=&map.line[pi];
 float frac=fmodf(playerz,SEGMENT)/SEGMENT,cy=800.f+pl->y0+(pl->y1-pl->y0)*frac;
 float percent=fmodf(game.position,SEGMENT)/SEGMENT,dx=-map.line[base].curve*percent,sum=0,maxY=view_h;
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
  int y0=MAX(0,(int)(v->b.y+off)),y1=MIN((int)view_h,(int)(v->a.y+off));
  if(y1>y0)rect(0,y0,screen_w,y1-y0,map.colors[light?1:2]);
  road_layers(v,light,off);maxY=v->b.y;
 }
 profile.road+=(uint32_t)(aster_time_now_ms()-measured);measured=aster_time_now_ms();
 background(CLAMP((int)maxY+off,0,(int)view_h));
 profile.bg+=(uint32_t)(aster_time_now_ms()-measured);measured=aster_time_now_ms();
 for(int n=DRAW_DISTANCE-1;n>=0;n--){
  View *v=&views[n];float ahead=(base+n)*SEGMENT-game.position;if(ahead<PLAYER_Z-SEGMENT)continue;
  for(uint32_t j=map.first[v->line];j<map.first[v->line+1];j++)scenery(v,&map.obj[j],off);
  for(int c=0;game.map_id&&c<20;c++)if((uint32_t)(cars[c].z/SEGMENT)%map.h.count==v->line&&cars[c].z>playerz)traffic_draw(v,&cars[c],off);
 }
 player_draw();profile.sprites+=(uint32_t)(aster_time_now_ms()-measured);measured=aster_time_now_ms();hud();profile.hud+=(uint32_t)(aster_time_now_ms()-measured);
}









