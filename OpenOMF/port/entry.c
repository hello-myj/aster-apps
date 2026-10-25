#include "omf_surface.h"
/* Aster event-loop adapter for the unchanged OpenOMF gameplay modules. */
#include <aster_imports.h>
#include "audio/audio.h"
#include "controller/joystick.h"
#include "game/common_defines.h"
#include "game/game_state.h"
#include "game/game_player.h"
#include "game/utils/settings.h"
#include "resources/resource_paths.h"
#include "resources/resource_files.h"
#include "resources/fonts.h"
#include "resources/languages.h"
#include "resources/script_cache.h"
#include "resources/sounds_loader.h"
#include "resources/modmanager.h"
#include "formats/altpal.h"
#include "game/gui/osd/osd.h"
#include "console/console.h"
#include "video/video.h"
#include "video/vga_state.h"
#include "utils/allocator.h"
#include "utils/log.h"
#include "utils/random.h"
#include <time.h>
#include <stdio.h>
int omf_scene_load_active(void);
int omf_scene_load_step(game_state *gs);
void omf_input_event(const aster_event_t *e);
void omf_input_cancel(void);
void omf_input_mode(int arena);
void omf_audio_tick(void);
void omf_audio_stats(void);
void omf_logic_stats(void);
void omf_renderer_stats(void);
void omf_audio_pause(void);
void omf_audio_resume(void);
uint32_t omf_heap_used(void);
uint32_t omf_heap_peak(void);
static game_state *gs;
static engine_init_flags flags;
/* settings_init retains this filename; it must outlive all callbacks. */
static path configFile;
static int stage,failed,stopped,systemPaused,previousPause,previousScene=-1,deviceDemo;
static uint32_t last,perfStart,perfFrames,perfWork;
static int dynamicWait,staticWait;
static uint32_t phaseAudio,phaseLogic,phaseDraw,phaseFinish;
static void adopt_state(void){
    if(!gs->new_state)return;
    game_state *old=gs;gs=gs->new_state;
    game_state_clone_free(old);omf_free(old);
}
static void dispatch(void){
    SDL_Event e;
    while(SDL_PollEvent(&e)){
        if(e.type==SDL_KEYDOWN&&(e.key.keysym.sym==SDLK_TAB||e.key.keysym.sym==SDLK_BACKQUOTE||
            (e.key.keysym.sym==SDLK_ESCAPE&&console_window_is_open()))){
            if(console_window_is_open())console_window_close();else console_window_open();
            continue;
        }
        if(console_window_is_open())console_event(gs,&e);else game_state_handle_event(gs,&e);
    }
}
ASTER_APP_EXPORT int aster_app_init(void){
    flags.speed=-1;aster_app_set_framerate(60);last=aster_time_now_ms();return 0;
}
static void initialize(void){
    int rc=0;
    switch(stage){
    case 0:
        log_init();log_set_level(LOG_INFO);
        rand_seed((uint32_t)time(NULL)^aster_time_now_ms());
        resource_path_init();resource_path_create_dirs();
        configFile=get_config_filename();rc=settings_init(path_c(&configFile));settings_load();
        {FILE *f=fopen("/data/state/device-demo.once","rb");if(f){deviceDemo=fgetc(f)=='2'?2:1;fclose(f);remove("/data/state/device-demo.once");}}
        break;
    case 1:
        vga_state_init();video_scan_renderers();
        rc=!video_init("",480,320,false,false,0,60,1,0);
        audio_scan_backends();rc|=!audio_init("",44100,false,1,
            settings_get()->sound.music_vol/10.0f,settings_get()->sound.sound_vol/10.0f);
        break;
    case 2:rc=!sounds_loader_init();break;
    case 3:rc=!lang_init();break;
    case 4:rc=!fonts_init();break;
    case 5:rc=altpals_init();console_init();osd_init();script_cache_init();modmanager_init();break;
    case 6:gs=omf_calloc(1,sizeof(*gs));rc=game_state_create(gs,&flags);if(!rc){joystick_init();if(deviceDemo){game_state_match_settings_reset(gs);game_state_init_demo(gs);if(deviceDemo==2){game_state_get_player(gs,0)->pilot->har_id=HAR_NOVA;game_state_get_player(gs,1)->pilot->har_id=HAR_NOVA;gs->arena=4;}game_state_set_next(gs,deviceDemo==2?SCENE_ARENA4:SCENE_VS);aster_printf("OMF one-shot device DEMO requested");}}break;
    }
    if(rc){aster_printf("OMF original engine init failed stage=%d rc=%d",stage,rc);failed=1;}
    else aster_printf("OMF original engine init stage=%d complete heap=%u peak=%u",stage++,omf_heap_used(),omf_heap_peak());
    last=aster_time_now_ms();
}
ASTER_APP_EXPORT void aster_app_event(const aster_event_t *e){
    if(!e||stopped||systemPaused)return;
    if(failed){aster_app_exit(1);return;}
    if(gs && omf_scene_load_active()){if(e->type==ASTER_EVENT_FRAME){omf_scene_load_step(gs);last=aster_time_now_ms();dynamicWait=staticWait=0;}return;}
    if(e->type!=ASTER_EVENT_FRAME){omf_input_event(e);if(gs)dispatch();return;}
    if(stage<7){initialize();return;}
    if(!game_state_is_running(gs)){aster_app_exit(0);return;}
    uint32_t start=aster_time_now_ms();omf_audio_tick();uint32_t logicStart=aster_time_now_ms();phaseAudio+=logicStart-start;int dt=start-last;last=start;
    dynamicWait+=dt;staticWait+=dt;
    if(dynamicWait>100)dynamicWait=100;if(staticWait>100)staticWait=100;
    int limit=10;
    while(limit--){
        int interval=game_state_ms_per_dyntick(gs),did=0;
        if(staticWait>STATIC_TICKS){game_state_static_tick(gs,false);if(omf_scene_load_active()){last=aster_time_now_ms();dynamicWait=staticWait=0;return;}adopt_state();console_tick(gs);osd_tick();staticWait-=STATIC_TICKS;did=1;}
        if(dynamicWait>interval){game_state_dynamic_tick(gs,false);dynamicWait-=interval;did=1;}
        if(!did)break;
        game_state_palette_transform(gs);vga_state_render();
    }
    omf_input_mode(gs->this_id>=SCENE_ARENA0&&gs->this_id<=SCENE_ARENA4&&!gs->paused&&!console_window_is_open());
    if((int)gs->this_id!=previousScene){previousScene=gs->this_id;aster_printf("OMF SCENE id=%u name=%s heap=%u peak=%u",gs->this_id,scene_get_name(gs->this_id),omf_heap_used(),omf_heap_peak());}
    uint32_t drawStart=aster_time_now_ms();phaseLogic+=drawStart-logicStart;
    video_render_prepare(game_state_get_framebuffer_options(gs));game_state_render(gs);osd_render();console_render();uint32_t finishStart=aster_time_now_ms();phaseDraw+=finishStart-drawStart;video_render_finish();phaseFinish+=aster_time_now_ms()-finishStart;
    uint32_t now=aster_time_now_ms();if(!perfFrames)perfStart=start;perfWork+=now-start;
    if(++perfFrames==120){omf_audio_stats();omf_logic_stats();omf_renderer_stats();aster_printf("OMF PERF frames=120 elapsed_ms=%u work_ms=%u scene=%u heap=%u peak=%u sprite_saved=%u sprite_cache=%u",now-perfStart,perfWork,gs->this_id,omf_heap_used(),omf_heap_peak(),omf_surface_saved(),omf_surface_resident());aster_printf("OMF PHASE n=120 audio_ms=%u logic_ms=%u draw_ms=%u finish_ms=%u",phaseAudio,phaseLogic,phaseDraw,phaseFinish);phaseAudio=phaseLogic=phaseDraw=phaseFinish=0;perfFrames=perfWork=0;}
}
ASTER_APP_EXPORT void aster_app_pause(void){
    if(systemPaused)return;systemPaused=1;omf_input_cancel();
    if(gs){previousPause=game_state_is_paused(gs);game_state_set_paused(gs,1);}omf_audio_pause();
}
ASTER_APP_EXPORT void aster_app_resume(void){
    if(systemPaused&&gs)game_state_set_paused(gs,previousPause);
    omf_input_cancel();systemPaused=0;last=aster_time_now_ms();dynamicWait=staticWait=0;omf_audio_resume();
}
static void release(void){
    if(stopped)return;stopped=1;omf_input_cancel();
    if(gs)game_state_free(&gs);if(stage>=7)joystick_close();
    if(stage>=6){script_cache_close();osd_close();console_close();altpals_close();modmanager_shutdown();}
    if(stage>=5)fonts_close();if(stage>=4)lang_close();if(stage>=3)sounds_loader_close();
    if(stage>=2){audio_close();video_close();vga_state_close();}
    if(stage>=1){settings_save();settings_free();}log_close();
}
ASTER_APP_EXPORT void aster_app_stop(void){release();}
ASTER_APP_EXPORT void aster_app_destroy(void){release();}
