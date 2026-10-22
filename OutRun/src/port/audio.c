/* GPL-3.0-or-later. ABI 4.1 background-driven loops; no FRAME retriggering. */
#include "engine.h"

typedef struct {
 int32_t handle;
 bool active;
 uint32_t pitch,gain,retry_at,attempts;
} LoopVoice;
static LoopVoice engine_voice,brake_voice;
static int32_t crash_effect,beep_effect;
static bool prepared,available,params_clock_valid,music_requested;
static uint32_t last_params,last_music_check,audio_errors;

static void report_error(const char *operation,int32_t rc){
 if(rc>=0)return;
 audio_errors++;
 if(audio_errors<=8)aster_logf(ASTER_LOG_WARN,"AUDIO41 %s error=%d",operation,rc);
}
static int32_t load_effect(const char *name){
 char path[128];aster_snprintf(path,sizeof(path),"/data/resources/audio/%s",name);
 int32_t handle=aster_audio_effect_load(path,strlen(path));
 if(handle<=0)report_error(name,handle<0?handle:ASTER_EIO);
 return handle;
}
static void stop_loop(LoopVoice *voice,uint32_t fade_ms){
 if(voice->handle<=0||!voice->active)return;
 int32_t rc=aster_audio_effect_stop((uint32_t)voice->handle,fade_ms);
 report_error("stop",rc);
 if(rc>=0)voice->active=false;
}
static void update_loop(LoopVoice *voice,uint32_t pitch,uint32_t gain,uint32_t crossfade,uint32_t now){
 if(voice->handle<=0)return;
 if(!voice->active){
  if(voice->attempts>=3||(int32_t)(now-voice->retry_at)<0)return;
  aster_audio_effect_play_desc_t desc={
   .size=sizeof(desc),.version=1,.flags=ASTER_AUDIO_EFFECT_F_LOOP,
   .crossfade_frames=crossfade,.pitch_q16=pitch,.gain_q16=gain,.fade_in_ms=80
  };
  int32_t rc=aster_audio_effect_play_ex((uint32_t)voice->handle,&desc,sizeof(desc));
  report_error("loop start",rc);
  if(rc<0){voice->attempts++;voice->retry_at=now+1000;return;}
  voice->active=true;voice->pitch=pitch;voice->gain=gain;voice->attempts=0;
  aster_logf(ASTER_LOG_INFO,"AUDIO41 loop handle=%d pitch=%u gain=%u",voice->handle,pitch,gain);
  return;
 }
 uint32_t dp=pitch>voice->pitch?pitch-voice->pitch:voice->pitch-pitch;
 uint32_t dg=gain>voice->gain?gain-voice->gain:voice->gain-gain;
 if(dp<512&&dg<512)return;
 aster_audio_effect_params_t params={
  .size=sizeof(params),.version=1,.pitch_q16=pitch,.gain_q16=gain,.ramp_ms=120
 };
 int32_t rc=aster_audio_effect_set_params((uint32_t)voice->handle,&params,sizeof(params));
 report_error("set params",rc);
 if(rc>=0){voice->pitch=pitch;voice->gain=gain;}
 else {voice->active=false;voice->retry_at=now+250;}
}
static void play_once(int32_t handle,uint32_t gain){
 if(handle<=0)return;
 aster_audio_effect_play_desc_t desc={
  .size=sizeof(desc),.version=1,.pitch_q16=65536,.gain_q16=gain,.fade_in_ms=5
 };
 report_error("one shot",aster_audio_effect_play_ex((uint32_t)handle,&desc,sizeof(desc)));
}
void audio_prepare(void){
 if(prepared)return;prepared=true;
 aster_audio_info_t info={.size=sizeof(info),.version=1};
 int32_t rc=aster_audio_get_info(&info,sizeof(info));
 report_error("get info",rc);available=rc>=0;
 if(!available)return;
 engine_voice.handle=load_effect("engine-loop.wav");
 brake_voice.handle=load_effect("brake-loop.wav");
 crash_effect=load_effect("crash-once.wav");
 beep_effect=load_effect("start-once.wav");
 aster_logf(ASTER_LOG_INFO,"AUDIO41 ready native=%u engine=%d brake=%d crash=%d beep=%d",info.sample_rate,engine_voice.handle,brake_voice.handle,crash_effect,beep_effect);
}
void audio_beep(void){play_once(beep_effect,42598);}
void audio_crash(void){play_once(crash_effect,36045);}
void audio_begin(void){
 audio_prepare();if(!available)return;
 /* RC1 start_file still replaces the playback session. Start BGM before loops. */
 stop_loop(&engine_voice,0);stop_loop(&brake_voice,0);
 const char *path="/data/resources/audio/music.mp3";
 int32_t rc=aster_audio_start_file(path,strlen(path));report_error("music start",rc);
 music_requested=rc>=0;
 engine_voice.active=brake_voice.active=false;
 engine_voice.attempts=brake_voice.attempts=0;
 engine_voice.retry_at=brake_voice.retry_at=0;
 params_clock_valid=false;last_music_check=aster_time_now_ms();
 aster_logf(ASTER_LOG_INFO,"AUDIO41 music rc=%d; loop voices armed after BGM",rc);
}
void audio_tick(void){
 if(!available||game.state!=2||game.paused)return;
 uint32_t now=aster_time_now_ms();
 if(params_clock_valid&&(uint32_t)(now-last_params)<50)return;
 params_clock_valid=true;last_params=now;
 if(music_requested&&(uint32_t)(now-last_music_check)>=1000){
  last_music_check=now;
  aster_audio_state_t state={.size=sizeof(state),.version=2};
  int32_t rc=aster_audio_get_state(&state,sizeof(state));
  if(rc>=0&&state.state==ASTER_AUDIO_IDLE)audio_begin();
 }
 float speed=CLAMP(game.speed,0.f,200.f);
 float rpm=game.gear?.35f+.65f*CLAMP((speed-100.f)/100.f,0.f,1.f):CLAMP(speed/100.f,0.f,1.f);
 bool throttle=game.gas||game.touch_gas||game.auto_gas;
 float pitch=.65f+1.05f*rpm+(throttle&&!game.brake?.05f:0.f);
 float gain=.18f+.22f*rpm+(throttle&&!game.brake?.08f:0.f);
 if(game.brake)gain=.14f;
 if(game.offroad)gain*=.8f;
 if(game.crash>0){pitch=.65f;gain=.06f;}
 update_loop(&engine_voice,(uint32_t)(pitch*65536.f),(uint32_t)(gain*65536.f),160,now);
 if(game.brake&&speed>5&&game.crash<=0){
  update_loop(&brake_voice,(uint32_t)((1.f+speed/800.f)*65536.f),16384,220,now);
 }else stop_loop(&brake_voice,80);
}
static void unload_effect(int32_t *handle){
 if(*handle>0){
  report_error("final stop",aster_audio_effect_stop((uint32_t)*handle,0));
  report_error("unload",aster_audio_effect_unload((uint32_t)*handle));
 }
 *handle=0;
}
void audio_end(void){
 unload_effect(&engine_voice.handle);unload_effect(&brake_voice.handle);
 unload_effect(&crash_effect);unload_effect(&beep_effect);
 if(available)report_error("session stop",aster_audio_stop());
 if(prepared)aster_logf(ASTER_LOG_INFO,"AUDIO41 end errors=%u",audio_errors);
 memset(&engine_voice,0,sizeof(engine_voice));memset(&brake_voice,0,sizeof(brake_voice));
 prepared=available=params_clock_valid=music_requested=false;
 last_params=last_music_check=audio_errors=0;
}
