/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "tyrian_port.h"
#include "loudness.h"
#include "nortsong.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int audioSampleRate=11025;
unsigned song_playing;
bool audio_disabled, music_disabled, samples_disabled, playing, songlooped;
static bool christmas, stopped=true;
static int loaded=-1;
static unsigned serial;
static struct {int id,handle; unsigned age;} cache[4];
static Uint8 sample_volume=255;
static void *wave_data[SOUND_COUNT];
static unsigned wave_bytes[SOUND_COUNT];
bool init_audio(void){aster_audio_set_volume(70);for(int i=0;i<4;i++)cache[i].id=-1;return true;}
void tyrian_audio_set_xmas(bool xmas){
 christmas=xmas;
 for(int i=0;i<4;i++){if(cache[i].handle>0)aster_audio_effect_unload(cache[i].handle);cache[i].handle=0;cache[i].id=-1;cache[i].age=0;}
 unsigned total=0;
 for(int i=0;i<SOUND_COUNT;i++){
  free(wave_data[i]);wave_data[i]=NULL;wave_bytes[i]=0;
  char path[96];snprintf(path,sizeof(path),"/data/audio/sfx/%03d.wav",i+(xmas?100:0));
  FILE *f=fopen(path,"rb");
  if(f){fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);
   if(n>=44&&n<=69632){void *data=malloc(n);if(data){if(fread(data,1,n,f)==(size_t)n){wave_data[i]=data;wave_bytes[i]=n;total+=n;}else free(data);}}
   fclose(f);
  }
  tyrian_yield(1);
 }
 aster_printf("TYRIAN audio preload %u bytes",total);
}
void play_song(unsigned n){
 if(audio_disabled||music_disabled)return;
 if(loaded==(int)n&&!stopped)return;
 char path[96];snprintf(path,sizeof(path),"/data/audio/music/%02u.mp3",n);
 int rc=aster_audio_start_file(path,strlen(path));
 if(rc<0){char msg[128];snprintf(msg,sizeof(msg),"music %u error %d",n,rc);aster_log(2,msg,strlen(msg));return;}
 loaded=n;song_playing=n;stopped=false;playing=true;songlooped=false;
}
void restart_song(void){unsigned n=song_playing;loaded=-1;play_song(n);}
void stop_song(void){aster_audio_pause();stopped=true;playing=false;}
void fade_song(void){stop_song();}
void set_volume(Uint8 music,Uint8 sample){(void)music;sample_volume=sample;}
void multiSamplePlay(const Sint16 *samples,size_t count,Uint8 chan,Uint8 vol){
 (void)count;(void)chan;
 if(audio_disabled||samples_disabled||!sample_volume)return;
 int id=-1;for(int i=0;i<SOUND_COUNT;i++)if(samples==soundSamples[i]){id=i;break;}
 if(id<0)return;
 int key=id+(christmas?100:0),slot=-1;
 for(int i=0;i<4;i++)if(cache[i].id==key)slot=i;
 if(slot<0){slot=0;for(int i=1;i<4;i++)if(cache[i].age<cache[slot].age)slot=i;
  if(cache[slot].handle>0){aster_audio_effect_stop(cache[slot].handle,0);aster_audio_effect_unload(cache[slot].handle);}
  cache[slot].handle=wave_data[id]?aster_audio_effect_load_memory(wave_data[id],wave_bytes[id]):0;cache[slot].id=key;
 }
 cache[slot].age=++serial;
 if(cache[slot].handle>0){aster_audio_effect_play_desc_t desc={.size=sizeof(desc),.version=1,.pitch_q16=65536,.gain_q16=(uint32_t)vol*sample_volume*65536/(7*255)};aster_audio_effect_play_ex(cache[slot].handle,&desc,sizeof(desc));}
}
void tyrian_audio_tick(void){
 if(stopped||loaded<0||music_disabled)return;
 aster_audio_state_t state={.size=sizeof(state),.version=2};
 if(aster_audio_get_state(&state,sizeof(state))==0&&state.state==0){
  if(song_playing==9||song_playing==10||song_playing==18||song_playing==24||song_playing==30||song_playing==33){playing=false;stopped=true;return;}
  songlooped=true;unsigned n=song_playing;loaded=-1;play_song(n);songlooped=true;
 }
}
void tyrian_audio_destroy(void){aster_audio_stop();for(int i=0;i<4;i++)if(cache[i].handle>0){aster_audio_effect_unload(cache[i].handle);cache[i].handle=0;}for(int i=0;i<SOUND_COUNT;i++){free(wave_data[i]);wave_data[i]=NULL;wave_bytes[i]=0;}stopped=true;}
void deinit_audio(void){tyrian_audio_destroy();}

