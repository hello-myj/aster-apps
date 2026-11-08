#include "audio.h"
#include "palcfg.h"
#include <aster_imports.h>
#include "audio_catalog.h"

AUDIODEVICE gAudioDevice;
static int available,current_music=-1,music_loop,music_started;
static int effect_id=-1,effect_part,effect_handle;
static uint32_t effect_deadline;
static int volume=70;

static void music_path(char *path,int capacity,int track)
{aster_snprintf(path,capacity,"/assets/audio/%c%c%c.mp3",'0'+track/100,'0'+track/10%10,'0'+track%10);}
static int start_music(void)
{
 if(!available||!gAudioDevice.fMusicEnabled||current_music<=0||current_music>=PAL_AUDIO_MUSIC_COUNT)return 0;
 char path[64];music_path(path,sizeof(path),current_music);
 int rc=aster_audio_start_file(path,(uint32_t)strlen(path));
 music_started=rc>=0;
 if(rc<0)aster_errfmt("PAL music %d failed: %d",current_music,rc);
 else aster_printf("PAL music %d",current_music);
 return music_started;
}
static void stop_effect(void)
{
 if(effect_handle>0){aster_audio_effect_unload((uint32_t)effect_handle);effect_handle=0;}
 effect_id=-1;
}
static int start_effect_part(void)
{
 if(effect_id<0||effect_id>=PAL_AUDIO_SFX_COUNT||effect_part>=pal_audio_sfx_parts[effect_id])return 0;
 char path[80];aster_snprintf(path,sizeof(path),"/assets/audio/sfx/%c%c%c_%d.wav",'0'+effect_id/100,'0'+effect_id/10%10,'0'+effect_id%10,effect_part);
 int rc=aster_audio_effect_load(path,(uint32_t)strlen(path));
 if(rc<=0){aster_errfmt("PAL effect %d/%d failed: %d",effect_id,effect_part,rc);return 0;}
 effect_handle=rc;
 rc=aster_audio_effect_play((uint32_t)effect_handle);
 if(rc<0){stop_effect();return 0;}
 uint32_t remaining=pal_audio_sfx_samples[effect_id]-(uint32_t)effect_part*PAL_AUDIO_CLIP_FRAMES;
 uint32_t count=remaining>PAL_AUDIO_CLIP_FRAMES?PAL_AUDIO_CLIP_FRAMES:remaining;
 effect_deadline=aster_time_now_ms()+(count*1000u+8191u)/8192u;
 return 1;
}
int AUDIO_OpenDevice(void)
{
 aster_audio_info_t info={.size=sizeof(info),.version=1};
 available=aster_audio_get_info(&info,sizeof(info))==ASTER_OK;
 memset(&gAudioDevice,0,sizeof(gAudioDevice));
 gAudioDevice.spec.freq=available?info.sample_rate:44100;
 gAudioDevice.spec.channels=available?info.channels:2;
 gAudioDevice.spec.format=AUDIO_S16SYS;
 gAudioDevice.spec.samples=1024;
 gAudioDevice.fOpened=available;
 gAudioDevice.fMusicEnabled=gAudioDevice.fSoundEnabled=available;
 gAudioDevice.iMusicVolume=gAudioDevice.iSoundVolume=volume*128/100;
 if(available)aster_audio_set_volume(volume);
 aster_printf("PAL audio %s",available?"enabled":"unavailable");
 return 0;
}
void AUDIO_CloseDevice(void)
{if(available)aster_audio_stop();stop_effect();available=0;music_started=0;gAudioDevice.fOpened=0;}
SDL_AudioSpec *AUDIO_GetDeviceSpec(void){return &gAudioDevice.spec;}
int AUDIO_CD_Available(void){return FALSE;}
int AUDIO_PlayCDTrack(int track){(void)track;return FALSE;}
void AUDIO_PlayMusic(int track,int loop,float fade)
{
 (void)fade; /* SDK file playback currently switches immediately. */
 if(track==current_music&&loop==music_loop&&music_started)return;
 current_music=track;music_loop=loop;music_started=0;
 if(available)aster_audio_stop();
 if(track>0)start_music();
}
void AUDIO_PlaySound(int sound)
{
 if(!available||!gAudioDevice.fSoundEnabled)return;
 stop_effect();
 if(sound<0||sound>=PAL_AUDIO_SFX_COUNT||!pal_audio_sfx_parts[sound])return;
 effect_id=sound;effect_part=0;
 if(!start_effect_part())stop_effect();
}
void AUDIO_EnableMusic(int enabled)
{
 gAudioDevice.fMusicEnabled=!!enabled;
 if(!available)return;
 if(enabled){if(music_started)aster_audio_resume();else start_music();}
 else if(music_started)aster_audio_pause();
}
int AUDIO_MusicEnabled(void){return gAudioDevice.fMusicEnabled;}
void AUDIO_EnableSound(int enabled){gAudioDevice.fSoundEnabled=!!enabled;if(!enabled)stop_effect();}
int AUDIO_SoundEnabled(void){return gAudioDevice.fSoundEnabled;}
static void set_volume(int next){volume=next<0?0:next>100?100:next;if(available)aster_audio_set_volume(volume);gAudioDevice.iMusicVolume=gAudioDevice.iSoundVolume=volume*128/100;}
void AUDIO_IncreaseVolume(void){set_volume(volume+5);}
void AUDIO_DecreaseVolume(void){set_volume(volume-5);}
void AUDIO_Lock(void){}void AUDIO_Unlock(void){}
void PalAsterAudioTick(void)
{
 if(!available)return;
 if(music_started&&music_loop&&gAudioDevice.fMusicEnabled){
  aster_audio_state_t state={.size=sizeof(state),.version=2};
  if(aster_audio_get_state(&state,sizeof(state))==ASTER_OK&&state.state==0)start_music();
 }
 if(effect_id>=0&&(int32_t)(aster_time_now_ms()-effect_deadline)>=0){
  if(effect_handle>0){aster_audio_effect_unload((uint32_t)effect_handle);effect_handle=0;}
  effect_part++;if(!start_effect_part())stop_effect();
 }
}
void PalAsterAudioPause(void){if(available&&music_started&&gAudioDevice.fMusicEnabled)aster_audio_pause();stop_effect();}
void PalAsterAudioResume(void){if(available&&music_started&&gAudioDevice.fMusicEnabled)aster_audio_resume();}
