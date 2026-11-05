/* GPL-3.0-or-later. One geometry definition for drawing and touch. */
#ifndef PIXELWHEELS_UI_H
#define PIXELWHEELS_UI_H
typedef struct {int x,y,w,h;} PWRect;
static int pw_large(void){return height>=400;}
static int pw_hit(PWRect r,int x,int y){return x>=r.x&&y>=r.y&&x<r.x+r.w&&y<r.y+r.h;}
static PWRect pw_control(int id){
 int big=pw_large();PWRect r={0};
 switch(id){
 case 0:r=big?(PWRect){14,height-96,74,80}:(PWRect){8,height-64,58,56};break;
 case 1:r=big?(PWRect){96,height-96,74,80}:(PWRect){80,height-64,58,56};break;
 case 2:r=big?(PWRect){width-160,height-88,68,74}:(PWRect){width-(auto_gas?68:132),height-64,60,56};break;
 case 3:r=big?(PWRect){width-82,height-106,68,92}:(PWRect){width-68,height-64,60,56};break;
 case 4:r=big?(PWRect){width/2-52,height-84,88,68}:(PWRect){width-68,height-124,60,48};break;
 }return r;
}
/* Larger, non-overlapping thumb zones extend to the physical screen edges.
 * Drawing stays compact; modal/header actions are resolved before these zones. */
static PWRect pw_control_touch(int id){
 if(pw_large())return pw_control(id);
 switch(id){
 case 0:return (PWRect){0,height-96,80,96};
 case 1:return (PWRect){80,height-96,80,96};
 case 2:return auto_gas?(PWRect){width-112,height-72,112,72}:(PWRect){width-160,height-72,80,72};
 case 3:return (PWRect){width-80,height-72,80,72};
 case 4:return (PWRect){width-80,height-144,80,72};
 default:return (PWRect){0};
 }
}
static PWRect pw_menu(int id){
 int big=pw_large(),bw=big?260:width-64;PWRect r={0};
 switch(id){
 case 1:r=big?(PWRect){width/2-108,height-72,216,48}:(PWRect){width/2-90,height-58,180,40};break;
 case 2:r=big?(PWRect){16,330,44,42}:(PWRect){8,138,36,32};break;
 case 3:r=big?(PWRect){224,330,44,42}:(PWRect){width-184,138,36,32};break;
 case 4:r=big?(PWRect){284,330,40,42}:(PWRect){width-136,138,32,32};break;
 case 5:r=big?(PWRect){424,330,40,42}:(PWRect){width-40,138,32,32};break;
 case 6:r=(PWRect){(width-bw)/2,height/2-8,bw,40};break;
 case 7:r=(PWRect){(width-bw)/2,height/2+40,bw,40};break;
 case 8:r=(PWRect){width-(big?72:66),3,big?64:58,26};break;
 case 9:r=(PWRect){8,42,big?92:76,28};break;
 case 10:r=big?(PWRect){width-112,122,96,28}:(PWRect){width-90,6,82,24};break;
 case 11:r=(PWRect){big?108:90,42,big?64:58,28};break;
 }return r;
}
#endif
