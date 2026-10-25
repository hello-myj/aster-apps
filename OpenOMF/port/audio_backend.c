/* Aster backend for the upstream three-channel audio scheduler. */
#include <aster_imports.h>
#include "audio/backends/audio_backend.h"
#include "audio/music_sources/psm_source.h"
#include "audio/music_sources/opus_source.h"
#include "utils/allocator.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
typedef struct {int handle,volume,pan;uint64_t end;} Channel;
typedef struct {int handle,id,freq,pan;unsigned len;const char *buf;} Cached;
static Cached cache[4];static uint32_t musicPoll;static int musicStarted;
static Channel channels[3];static float masterSound=.5f,masterMusic=.5f;static music_source current;static uint64_t suspended;
typedef struct {char path[512];} Music;
static bool available(void){return true;}static const char *name(void){return "AsterAudio";}static const char *desc(void){return "Aster three-channel original audio";}
static unsigned rates(const audio_sample_rate **r){static const audio_sample_rate v[]={{44100,true,"44100 Hz"}};*r=v;return 1;}
static void info(void *ctx,unsigned *rate,unsigned *ch,unsigned *res){if(rate)*rate=44100;if(ch)*ch=2;if(res)*res=1;}
static void sound_volume(void *ctx,float v){masterSound=v;for(int i=0;i<3;i++)if(channels[i].handle>0){aster_audio_effect_params_t p={.size=sizeof(p),.version=1,.pitch_q16=65536,.gain_q16=(uint32_t)(v*channels[i].volume*65536/127),.ramp_ms=0};aster_audio_effect_set_params(channels[i].handle,&p,sizeof(p));}}
static void music_volume(void *ctx,float v){masterMusic=v;aster_audio_set_volume((int)(v*100));}
static void create(audio_backend *b){b->ctx=channels;}
static void stop_channel(void *ctx,int ch){if(channels[ch].handle>0){aster_audio_effect_stop(channels[ch].handle,0);}channels[ch]=(Channel){0};}
static bool playing(void *ctx,int ch){return channels[ch].handle>0&&aster_time_now_ms()<channels[ch].end;}
static void le16(unsigned char *b,unsigned v){b[0]=v;b[1]=v>>8;}static void le32(unsigned char *b,unsigned v){le16(b,v);le16(b+2,v>>16);}
static int cached_busy(int handle){for(int i=0;i<3;i++)if(channels[i].handle==handle&&playing(NULL,i))return 1;return 0;}
static uint32_t pcmCalls,pcmElapsed,pcmMiss;
static bool pcm_impl(void *ctx,int ch,const sound_source *src,int volume,int pan,int fade){
 stop_channel(ctx,ch);if(!src->buf||!src->len||src->freq<=0)return false;
 Cached *item=NULL;int handle=0;
 for(int i=0;i<4;i++)if(cache[i].handle>0&&!cached_busy(cache[i].handle)&&cache[i].buf==src->buf&&cache[i].id==src->sound_id&&cache[i].freq==src->freq&&cache[i].len==src->len&&cache[i].pan==pan){item=&cache[i];handle=item->handle;goto start_play;}
 for(int i=0;i<4;i++)if(!cache[i].handle){item=&cache[i];break;}
 if(!item)for(int i=0;i<4;i++)if(!cached_busy(cache[i].handle)){item=&cache[i];break;}
 if(!item)return false;if(item->handle>0){for(int i=0;i<3;i++)if(channels[i].handle==item->handle)channels[i]=(Channel){0};aster_audio_effect_unload(item->handle);}*item=(Cached){0};
 pcmMiss++;
 unsigned rate=src->freq<8000?8000:src->freq>48000?48000:src->freq;unsigned frames=(uint64_t)src->len*rate/src->freq;
 if(frames>32768){rate=8000;frames=(uint64_t)src->len*rate/src->freq;}if(!frames||frames>32768){aster_printf("OMF audio sample exceeds effect budget id=%d",src->sound_id);return false;}
 unsigned count=frames<=16384?2:1;unsigned bytes=frames*2*count;
 unsigned char *wav=calloc(1,44+bytes);if(!wav)return false;unsigned char *h=wav;
 memcpy(h,"RIFF",4);le32(h+4,36+bytes);memcpy(h+8,"WAVEfmt ",8);le32(h+16,16);le16(h+20,1);le16(h+22,count);le32(h+24,rate);le32(h+28,rate*2*count);le16(h+32,2*count);le16(h+34,16);memcpy(h+36,"data",4);le32(h+40,bytes);
 int16_t *samples=(int16_t *)(wav+44);
 for(unsigned i=0;i<frames;i++){uint64_t pos=(uint64_t)i*src->freq*65536/rate;unsigned at=pos>>16;if(at>=src->len)at=src->len-1;unsigned next=at+1<src->len?at+1:at;int v0=((unsigned char)src->buf[at]-128)*256,v1=((unsigned char)src->buf[next]-128)*256;int v=v0+((int64_t)(v1-v0)*(pos&65535))/65536;if(count==2){samples[i*2]=v*(pan>0?100-pan:100)/100;samples[i*2+1]=v*(pan<0?100+pan:100)/100;}else samples[i]=v;}
 /* ABI 4.2 copies the complete PCM WAV before returning. No SD round trip. */
 handle=aster_audio_effect_load_memory(wav,44+bytes);free(wav);if(handle<=0)return false;*item=(Cached){handle,src->sound_id,src->freq,pan,src->len,src->buf};
 start_play:;
 for(int i=0;i<3;i++)if(channels[i].handle==handle)channels[i]=(Channel){0};
 aster_audio_effect_play_desc_t play={.size=sizeof(play),.version=1,.pitch_q16=65536,.gain_q16=(uint32_t)(masterSound*volume*65536/127),.fade_in_ms=fade<0?0:fade>1000?1000:fade};int rc=aster_audio_effect_play_ex(handle,&play,sizeof(play));if(rc<0){aster_audio_effect_unload(handle);*item=(Cached){0};return false;}channels[ch]=(Channel){.handle=handle,.volume=volume,.pan=pan,.end=aster_time_now_ms()+(uint64_t)src->len*1000/src->freq};return true;
}
static bool pcm(void *ctx,int ch,const sound_source *src,int volume,int pan,int fade){uint32_t t=aster_time_now_ms();bool ok=pcm_impl(ctx,ch,src,volume,pan,fade);pcmCalls++;pcmElapsed+=aster_time_now_ms()-t;return ok;}
void omf_audio_stats(void){aster_printf("OMF PCM calls=%u elapsed_ms=%u misses=%u",pcmCalls,pcmElapsed,pcmMiss);pcmCalls=pcmElapsed=pcmMiss=0;}
static void fade_channel(void *ctx,int ch,int ms){if(channels[ch].handle>0)aster_audio_effect_stop(channels[ch].handle,ms>1000?1000:ms<0?0:ms);channels[ch].end=aster_time_now_ms()+(ms>0?ms:0);}
static void pan_channel(void *ctx,int ch,int pan){/* Initial pan is baked into PCM. Live per-voice pan needs a host API. */channels[ch].pan=pan;}
static void stop_music(void *ctx){musicStarted=0;aster_audio_stop();music_source_close(&current);memset(&current,0,sizeof(current));for(int i=0;i<3;i++)stop_channel(ctx,i);}
static void play_music(void *ctx,const music_source *src){stop_music(ctx);current=*src;Music *m=current.context;if(m)musicStarted=aster_audio_start_file(m->path,strlen(m->path))>=0;musicPoll=aster_time_now_ms();aster_audio_set_volume((int)(masterMusic*100));}
static bool setup(void *ctx,unsigned r,bool mono,int res,float mv,float sv){masterSound=sv;masterMusic=mv;return true;}
static void close_context(void *ctx){stop_music(ctx);for(int i=0;i<4;i++){if(cache[i].handle>0)aster_audio_effect_unload(cache[i].handle);cache[i]=(Cached){0};}}static void destroy(audio_backend *b){}
void sdl_audio_backend_set_callbacks(audio_backend *b){*b=(audio_backend){.is_available=available,.get_description=desc,.get_name=name,.get_sample_rates=rates,.get_info=info,.set_sound_volume=sound_volume,.set_music_volume=music_volume,.create=create,.destroy=destroy,.setup_context=setup,.close_context=close_context,.play_pcm_sound=pcm,.is_channel_playing=playing,.stop_channel=stop_channel,.fade_out_channel=fade_channel,.set_channel_panning=pan_channel,.play_music=play_music,.stop_music=stop_music};}
static void omf_music_close(void *ctx){free(ctx);}static void omf_music_render(void *ctx,char *buf,int n){memset(buf,0,n);}static void music_gain(void *ctx,float v){}
bool psm_load(music_source *s,int ch,int rate,int res,const char *file){Music *m=calloc(1,sizeof(*m));if(!m)return false;snprintf(m->path,sizeof(m->path),"%s",file);char *dot=strrchr(m->path,'.');if(dot)snprintf(dot,sizeof(m->path)-(dot-m->path),".mp3");*s=(music_source){.context=m,.close=omf_music_close,.render=omf_music_render,.set_volume=music_gain};return true;}
unsigned psm_get_resamplers(const music_resampler **r){static const music_resampler a[]={{1,true,"Converted original tracker"}};*r=a;return 1;}
bool opus_load_memory(music_source *s,int ch,int rate,const unsigned char *b,size_t n){free((void*)b);return false;}
void omf_audio_pause(void){suspended=aster_time_now_ms();aster_audio_pause();}
void omf_audio_resume(void){if(suspended){uint64_t dt=aster_time_now_ms()-suspended;for(int i=0;i<3;i++)if(channels[i].handle>0)channels[i].end+=dt;suspended=0;}aster_audio_resume();}

/* The public ABI has no file-loop flag. Restart a complete track at EOF.
 * This preserves repetition, but MP3 restart is not guaranteed gapless. */
void omf_audio_tick(void){
 uint32_t now=aster_time_now_ms();if(!musicStarted||suspended||now-musicPoll<250)return;musicPoll=now;
 aster_audio_state_t st={.size=sizeof(st),.version=2};
 if(aster_audio_get_state(&st,sizeof(st))>=0&&st.state==ASTER_AUDIO_IDLE){Music *m=current.context;if(m)musicStarted=aster_audio_start_file(m->path,strlen(m->path))>=0;}
}
