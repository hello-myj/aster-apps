/* GPL-3.0-or-later. Gravity steering for lckfb's landscape sensor Y axis. */
#include "engine.h"
#include "ui.h"
static bool available,enabled;
static float center,filtered,output;
static uint32_t previous,failures;
static int direction;
static bool gravity(float *value){
 float a[3];if(aster_accel_read(a,sizeof(a))<0)return false;
 float norm=a[0]*a[0]+a[1]*a[1]+a[2]*a[2];
 /* Includes finite validation; reject a shake/free fall as steering input. */
 if(!(norm>=64.f&&norm<=144.f))return false;
 // Normalize without libc math: bounded Newton reciprocal square root.
 float r=.1f;for(int i=0;i<3;i++)r*=1.5f-.5f*norm*r*r;
 *value=a[1]*r;return *value>=-1.f&&*value<=1.f;
}
bool outrun_motion_available(void){return available;}
bool outrun_motion_enabled(void){return enabled;}
float outrun_motion_steer(void){return enabled?output:0;}
void outrun_motion_center(void){
 float value;if(gravity(&value)){center=value;failures=0;}
 filtered=output=0;direction=0;previous=0;
}
void outrun_motion_init(void){
 available=enabled=false;float value;
 if(!ui_large()&&gravity(&value)){available=enabled=true;center=value;}
 filtered=output=0;direction=0;previous=failures=0;
 aster_logf(ASTER_LOG_INFO,"OUTRUN tilt available=%u enabled=%u axis=Y left=negative",available,enabled);
}
void outrun_motion_toggle(void){
 if(!available)return;enabled=!enabled;output=0;direction=0;
 if(enabled)outrun_motion_center();
}
void outrun_motion_tick(uint32_t now){
 if(!enabled)return;float value;
 if(!gravity(&value)){
  output=0;direction=0;
  if(++failures>=3){enabled=false;aster_logf(ASTER_LOG_WARN,"OUTRUN tilt unavailable; touch steering restored");}
  return;
 }
 failures=0;uint32_t elapsed=previous?now-previous:33;previous=now;
 float alpha=(float)MIN(elapsed,150)/(90.f+MIN(elapsed,150));
 filtered+=(value-center-filtered)*alpha;
 if(filtered>.075f)direction=1;else if(filtered<-.075f)direction=-1;
 else if(fabsf(filtered)<.045f)direction=0;
 float strength=CLAMP((fabsf(filtered)-.045f)/.28f,.12f,1.f);
 output=direction*strength;
}
