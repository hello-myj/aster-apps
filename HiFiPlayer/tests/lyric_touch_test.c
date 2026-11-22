/* SPDX-License-Identifier: MIT */
/* Link with src/lrc.c, SDK aster_printf.c and --gc-sections, as for seek_test.c. */
#include "../src/main.c"
#include <assert.h>

static uint32_t now_ms;
static int seeks;

uint32_t aster_time_now_ms(void) { return now_ms; }
int32_t aster_audio_set_volume(int32_t percent) { return percent; }
int32_t aster_fs_closedir(aster_handle_t dir) { return 0; }
int32_t aster_fs_opendir(const char *path, uint32_t length, aster_handle_t *dir) { return ASTER_ENOENT; }
int32_t aster_gfx_text_w(uint32_t px, const char *text, uint32_t length) { return px * length; }
void aster_app_exit(int32_t code) { assert(0); }
void aster_log(uint32_t level, const char *msg, uint32_t len) {}
void paper_invalidate(void) {}
void paper_prepare(const lrc_t *lrc, int cue, uint32_t duration, unsigned scene) {}
bool paper_highlight(const lrc_t *lrc, int cue, uint32_t position) { return false; }

int32_t aster_player_control(const aster_player_request_t *request, uint32_t size)
{
    assert(size == sizeof(*request) && request->command == ASTER_PLAYER_SEEK);
    seeks++;
    return 0;
}

static void input(unsigned action, unsigned x, unsigned y, uint32_t time)
{
    now_ms = time;
    aster_event_t event = {.timestamp_ms = time, .u.touch = {x, y, action, 0}};
    touch(&event);
}

static void tap(unsigned x, unsigned y, uint32_t time)
{
    input(ASTER_TOUCH_DOWN, x, y, time);
    input(ASTER_TOUCH_UP, x, y, time + 45);
}

int main(void)
{
    content_w = content_h = 480;
    page = PLAYBACK;
    tap(194, 173, 10);
    tap(197, 173, 120);
    assert(page == PLAYBACK);

    input(ASTER_TOUCH_DOWN, 90, 172, 340);
    input(ASTER_TOUCH_UP, 210, 172, 418);
    assert(page == LYRICS);
    tap(200, 170, 600);
    tap(190, 169, 1200);
    assert(page == LYRICS); /* These two taps were too far apart in time. */
    tap(194, 170, 1300);
    assert(page == PAPER && motion_origin == LYRICS);
    tap(220, 205, 2000);
    tap(220, 205, 2150);
    assert(motion_leaving);
    paper_tick(3000);
    assert(page == LYRICS);

    tap(190, 169, 3200);
    tap(290, 169, 3310);
    assert(page == LYRICS); /* Separate locations. */
    input(ASTER_TOUCH_DOWN, 260, 165, 3450);
    input(ASTER_TOUCH_MOVE, 237, 170, 3480);
    input(ASTER_TOUCH_UP, 260, 165, 3505);
    tap(260, 165, 3600);
    assert(page == LYRICS); /* A drag can end where it started. */
    input(ASTER_TOUCH_DOWN, 260, 165, 3700);
    input(ASTER_TOUCH_UP, 260, 165, 4100);
    tap(260, 165, 4140);
    assert(page == LYRICS); /* A long press cannot start a pair. */

    player.mode = ASTER_PLAYER_LOCAL;
    player.duration_ms = 60000;
    tap(220, 411, 4250);
    tap(220, 411, 4370);
    assert(page == LYRICS && seeks == 2);

    has_accelerometer = true;
    tap(220, 200, 4600);
    tap(221, 201, 4720);
    assert(page == LYRICS);
    return 0;
}
