/* GPL-3.0-or-later. Original EngineSoundPlayer adjacent-layer blend + pow2Out
 * pitch, and AudioComponent drift ramp. Four handles: two engine layers,
 * one road loop, one reusable priority cue. No FRAME-based loop retriggering.
 */
#include <aster_imports.h>
#include "audio.h"
typedef struct {const char *file;uint32_t duration_ms;} AudioCueDef;
#include "audio_catalog.h"
typedef struct {int32_t handle;int index,active;uint32_t pitch,gain,retry_at;} Voice;
static Voice engines[2]={{.index=-1},{.index=-1}},road={.index=-1};
static int32_t cue_handle;
static int cue_index=-1,cue_priority,prepared,available,running,suspended,errors;
static uint32_t cue_until,last_event[8],clock_at,loops_after,last_tick;
static int queue[8],queue_count;
static const int priorities[]={3,9,6,5,7,6,8,10};
static float clamp(float x,float lo,float hi){return x<lo?lo:x>hi?hi:x;}
static float absolute(float x){return x<0?-x:x;}
static int due(uint32_t now,uint32_t at){return (int32_t)(now-at)>=0;}
static void result(const char *op,int rc){if(rc<0&&errors++<12)aster_logf(ASTER_LOG_WARN,"PW AUDIO %s rc=%d",op,rc);}
static int32_t load(const char *name){char path[128];aster_snprintf(path,sizeof(path),"/data/resources/%s",name);int32_t h=aster_audio_effect_load(path,strlen(path));if(h<=0)result(name,h<0?h:ASTER_EIO);return h;}
static void drop(Voice *v){if(v->handle>0){aster_audio_effect_stop(v->handle,0);aster_audio_effect_unload(v->handle);}v->handle=0;v->active=0;v->index=-1;v->pitch=v->gain=0;}
static void loop(Voice *v,uint32_t pitch,uint32_t gain,uint32_t crossfade,uint32_t now,const char *label){
    if(v->handle<=0||!due(now,v->retry_at))return;
    int rc;
    if(!v->active){
        aster_audio_effect_play_desc_t d={.size=sizeof(d),.version=1,.flags=ASTER_AUDIO_EFFECT_F_LOOP,.crossfade_frames=crossfade,.pitch_q16=pitch,.gain_q16=gain,.fade_in_ms=80};
        rc=aster_audio_effect_play_ex(v->handle,&d,sizeof(d));
        result(label,rc);if(rc<0){v->retry_at=now+1000;return;}
        v->active=1;v->pitch=pitch;v->gain=gain;
        aster_printf("PW AUDIO %s start layer=%d pitch=%u gain=%u",label,v->index,pitch,gain);
    }else{
        uint32_t dp=pitch>v->pitch?pitch-v->pitch:v->pitch-pitch,dg=gain>v->gain?gain-v->gain:v->gain-gain;
        if(dp<512&&dg<512)return;
        aster_audio_effect_params_t p={.size=sizeof(p),.version=1,.pitch_q16=pitch,.gain_q16=gain,.ramp_ms=100};
        rc=aster_audio_effect_set_params(v->handle,&p,sizeof(p));result("set params",rc);
        if(rc>=0){v->pitch=pitch;v->gain=gain;}else{v->active=0;v->retry_at=now+1000;}
    }
}
void pw_audio_stop(void){
    if(available)result("stop",aster_audio_stop());
    for(int i=0;i<2;i++)engines[i].active=0;road.active=0;
    running=suspended=queue_count=0;cue_until=0;cue_priority=0;
}
void pw_audio_begin(const char *theme){
    if(!prepared){available=aster_system_has_feature(ASTER_FEATURE_AUDIO_OUTPUT)>0;prepared=1;}
    pw_audio_stop();if(!available)return;
    memset(last_event,0,sizeof(last_event));
    char path[128];aster_snprintf(path,sizeof(path),"/data/resources/%s.mp3",theme);
    int rc=aster_audio_start_file(path,strlen(path));result("music",rc);
    // Let the RC1 session handoff settle before arming continuous effects.
    clock_at=aster_time_now_ms();loops_after=clock_at+250;last_tick=clock_at;running=1;
    aster_printf("PW AUDIO begin music=%d engine=5-layer blend road=drift/water",rc);
}
void pw_audio_event(int cue){
    if(!running||cue<0||cue>7)return;
    for(int i=0;i<queue_count;i++)if(queue[i]==cue)return;
    if(last_event[cue]&&!due(clock_at,last_event[cue]+(cue==PW_IMPACT?250:120)))return;
    if(queue_count<8)queue[queue_count++]=cue;
}
static void play_cue(uint32_t now){
    if(!queue_count)return;
    int q=0;for(int i=1;i<queue_count;i++)if(priorities[queue[i]]>priorities[queue[q]])q=i;
    int cue=queue[q];int busy=cue_until&&!due(now,cue_until);
    if(busy&&priorities[cue]<cue_priority)return;
    if(cue_handle>0&&cue_index!=cue){aster_audio_effect_stop(cue_handle,0);aster_audio_effect_unload(cue_handle);cue_handle=0;}
    if(cue_handle<=0){cue_handle=load(cue_defs[cue].file);cue_index=cue;}
    queue[q]=queue[--queue_count];
    if(cue_handle<=0)return;
    uint32_t gain=cue==PW_TURBO?30000:cue==PW_IMPACT?46000:cue==PW_LAP||cue==PW_FINISH?24000:44000;
    aster_audio_effect_play_desc_t d={.size=sizeof(d),.version=1,.pitch_q16=65536,.gain_q16=gain,.fade_in_ms=5};
    int rc=aster_audio_effect_play_ex(cue_handle,&d,sizeof(d));result(cue_defs[cue].file,rc);
    if(rc>=0){cue_until=now+cue_defs[cue].duration_ms;cue_priority=priorities[cue];last_event[cue]=now;aster_printf("PW AUDIO cue %s gain=%u",cue_defs[cue].file,gain);}
}
void pw_audio_tick(uint32_t now,float speed,float turbo,float drift,int material,int active,int paused){
    clock_at=now;if(!running||!available||suspended||paused)return;
    if(now-last_tick<60||!due(now,loops_after))return;last_tick=now;
    play_cue(now);
    if(!active){for(int i=0;i<2;i++)if(engines[i].active){aster_audio_effect_stop(engines[i].handle,120);engines[i].active=0;}if(road.active){aster_audio_effect_stop(road.handle,120);road.active=0;}return;}
    // Upstream normSpeed = body speed / 50m/s. Adapted world is 10px/m.
    float norm=clamp(speed/500,0,1),idx=norm*4;int low=(int)idx,high=low<4?low+1:low;
    float duck=cue_until&&!due(now,cue_until)?.75f:1;
    float pitch=.5f+1.5f*(1-(1-norm)*(1-norm));
    for(int i=0;i<2;i++)if(engines[i].index!=low&&engines[i].index!=high)drop(&engines[i]);
    int targets[2]={low,high};
    for(int t=0;t<2;t++){
        int band=targets[t];if(t&&band==targets[0])continue;
        Voice *v=0;for(int i=0;i<2;i++)if(engines[i].index==band)v=&engines[i];
        if(!v){for(int i=0;i<2;i++)if(engines[i].handle<=0){v=&engines[i];break;}}
        if(!v)continue;
        if(v->handle<=0){char file[24];aster_snprintf(file,sizeof(file),"engine%d.wav",band);v->handle=load(file);v->index=band;v->retry_at=0;}
        float weight=clamp(1-absolute(band-idx),0,1);
        // Keep zero-volume neighbours in phase, so a speed crossing needs no replay.
        uint32_t gain=(uint32_t)(weight*.68f*duck*65536);
        loop(v,(uint32_t)(pitch*65536),gain,160,now,"engine");
    }
    int road_type=drift>0?0:(material==4||material==5)&&speed>8?1:-1;
    if(road_type<0){if(road.active){result("road stop",aster_audio_effect_stop(road.handle,120));road.active=0;}return;}
    if(road.index!=road_type){drop(&road);road.index=road_type;road.handle=load(road_type?"splash.wav":"skid.wav");}
    float gain=road_type?clamp(speed/500,0,.55f):clamp(drift/.6f,0,1)*.6f;
    gain*=.68f*duck;
    uint32_t road_pitch=material==7?32768:65536;
    loop(&road,road_pitch,(uint32_t)(gain*65536),80,now,road_type?"water":"drift");
    (void)turbo;
}
void pw_audio_finish(void){
    for(int i=0;i<2;i++)if(engines[i].active){aster_audio_effect_stop(engines[i].handle,120);engines[i].active=0;}
    if(road.active){aster_audio_effect_stop(road.handle,120);road.active=0;}
    pw_audio_event(PW_FINISH);
}
void pw_audio_pause(void){if(!running||suspended)return;suspended=1;if(available)result("pause",aster_audio_pause());}
void pw_audio_resume(void){if(!running||!suspended)return;suspended=0;if(available)result("resume",aster_audio_resume());last_tick=aster_time_now_ms();}
void pw_audio_release(void){
    pw_audio_stop();for(int i=0;i<2;i++)drop(&engines[i]);drop(&road);
    if(cue_handle>0)aster_audio_effect_unload(cue_handle);cue_handle=0;cue_index=-1;
    available=prepared=errors=0;
}