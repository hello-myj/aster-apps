/* GPL-3.0-or-later. Pixel Wheels original audio behaviour, Aster ABI 4.1. */
#pragma once
#include <stdint.h>
enum {PW_COUNT,PW_GO,PW_IMPACT,PW_PICKUP,PW_TURBO,PW_SPLASH,PW_LAP,PW_FINISH};
void pw_audio_begin(const char *theme);
void pw_audio_event(int cue);
void pw_audio_tick(uint32_t now,float speed,float turbo,float drift,int material,int active,int paused);
void pw_audio_finish(void);
void pw_audio_stop(void);
void pw_audio_pause(void);
void pw_audio_resume(void);
void pw_audio_release(void);