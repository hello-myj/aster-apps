/* SPDX-License-Identifier: GPL-3.0-or-later
 * Binaryen turns its asyncify.* intrinsics into ordinary internal WASM code.
 * Nothing here requires a thread, custom Host import, or a changed Aster ABI. */
#include "tyrian_port.h"
#include <string.h>

__attribute__((import_module("asyncify"),import_name("start_unwind"))) void async_start_unwind(void *);
__attribute__((import_module("asyncify"),import_name("stop_unwind"))) void async_stop_unwind(void);
__attribute__((import_module("asyncify"),import_name("start_rewind"))) void async_start_rewind(void *);
__attribute__((import_module("asyncify"),import_name("stop_rewind"))) void async_stop_rewind(void);
/* Internal compiler-stack helpers are lowered by tools/build.py. */
__attribute__((import_module("tyrian"),import_name("stack_get"))) uint32_t engine_stack_get(void);
__attribute__((import_module("tyrian"),import_name("stack_set"))) void engine_stack_set(uint32_t);

static _Alignas(16) uint8_t coroutine_stack[65536];
static _Alignas(16) uint8_t engine_stack[262144];
static uint32_t engine_sp;
static struct { uint32_t current, end; } coroutine;
static uint32_t clock_ms, wake_ms, last_host, report_ms, presented;
static int suspended, started, done, exit_code, paused, cleaned, rewinding;
int tyrian_main(int argc,char **argv);
int tyrian_drop_cache(void) { return 0; }
uint32_t tyrian_ticks(void) { return clock_ms; }

__attribute__((noinline)) void tyrian_yield(uint32_t delay_ms) {
    if(rewinding) { rewinding=0; async_stop_rewind(); return; }
    wake_ms=clock_ms+delay_ms;
    suspended=1;
    coroutine.current=(uint32_t)(uintptr_t)coroutine_stack;
    coroutine.end=coroutine.current+sizeof(coroutine_stack);
    async_start_unwind(&coroutine);
}
__attribute__((noinline,noreturn)) void tyrian_exit(int code) {
    done=1;exit_code=code;
    /* The suspended call is never resumed after normal/fatal engine exit. */
    tyrian_yield(0);
    __builtin_trap();
}

ASTER_APP_EXPORT int aster_app_init(void) {
    /* Select upstream portable-save mode. O_CREAT preserves existing settings. */
    const char *config="/data/opentyrian.cfg";
    aster_handle_t config_file=0;
    int config_rc=aster_fs_open(config,strlen(config),ASTER_FS_O_CREAT|ASTER_FS_O_RDWR,&config_file);
    if(config_rc==0)aster_fs_close(config_file);
    else aster_errfmt("OpenTyrian config create %d",config_rc);
    int rc=tyrian_video_init();
    if(rc<0)return rc;
    last_host=aster_time_now_ms();report_ms=last_host;
    engine_sp=(uint32_t)(uintptr_t)(engine_stack+sizeof(engine_stack));
    aster_printf("OpenTyrian original engine ac5d0e7; cooperative C/WASM");
    return aster_app_set_framerate(60);
}
/* These runtime functions call Asyncify controls directly, and are deliberately
 * not themselves instrumented. At most one frame is presented per callback. */
ASTER_APP_EXPORT __attribute__((noinline)) void aster_app_event(const aster_event_t *ev) {
    if(!ev||cleaned||paused)return;
    if(ev->type==ASTER_EVENT_KEY||ev->type==ASTER_EVENT_TOUCH) { tyrian_input(ev);return; }
    if(ev->type!=ASTER_EVENT_FRAME)return;
    uint32_t now=aster_time_now_ms(), delta=now-last_host;last_host=now;
    if(delta>100)delta=100; /* freeze suspended time instead of runaway catch-up */
    uint32_t target=started?clock_ms+delta:0;
    static char *args[]={"opentyrian","--data=/data/resources",NULL};
    for(unsigned budget=0;budget<128&&!done;budget++) {
        if(suspended) {
            if((int32_t)(wake_ms-target)>0)break;
            clock_ms=wake_ms;
            rewinding=1; async_start_rewind(&coroutine);
        } else clock_ms=target;
        suspended=0;started=1;
        /* Keep the suspended engine's LLVM C stack separate from callbacks.
         * Asyncify preserves WASM locals, but does not relocate C stack data. */
        uint32_t host_sp=engine_stack_get();
        engine_stack_set(engine_sp);
        int rc=tyrian_main(2,args);
        engine_sp=engine_stack_get();
        engine_stack_set(host_sp);
        if(suspended)async_stop_unwind();
        if(!suspended&&!done){done=1;exit_code=rc;}
    }
    /* Advance idle time even if a pending wait has not yet elapsed. */
    if((int32_t)(target-clock_ms)>0)clock_ms=target;
    tyrian_audio_tick();
    if(done){aster_printf("OpenTyrian exited %d",exit_code);aster_app_exit(exit_code);return;}
    int rc=tyrian_video_present();
    if(rc<0){aster_errfmt("OpenTyrian present %d",rc);aster_app_exit(rc);return;}
    presented+=rc>0;
    if(now-report_ms>=5000){aster_printf("TYRIAN PERF frames=%u ms=%u heap=%u peak=%u",presented,now-report_ms,tyrian_heap_used(),tyrian_heap_peak());report_ms=now;presented=0;}
}
ASTER_APP_EXPORT void aster_app_pause(void) { paused=1;tyrian_input_clear();aster_audio_pause(); }
ASTER_APP_EXPORT void aster_app_resume(void) { last_host=aster_time_now_ms();paused=0;tyrian_input_clear();aster_audio_resume(); }
static void cleanup(void){if(cleaned)return;cleaned=1;tyrian_audio_destroy();tyrian_video_destroy();}
ASTER_APP_EXPORT void aster_app_stop(void){cleanup();}
ASTER_APP_EXPORT void aster_app_destroy(void){cleanup();}
