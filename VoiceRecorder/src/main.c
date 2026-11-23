/* SPDX-License-Identifier: MIT */
#include <aster_imports.h>
#include <aster_gfx_engine.h>

#define DIR "/sd/Recordings"
#define FOLDER_LEN (sizeof(DIR) - 1)
#define PAPER ASTER_RGB565(244, 239, 223)
#define INK ASTER_RGB565(31, 45, 41)
#define ACCENT ASTER_RGB565(192, 57, 42)

static aster_handle_t surface, file;
static aster_ge_t ge;
static aster_gfx_cmd_t commands[96];
static uint8_t arena[2048], pcm[8192];
static aster_audio_info_t format;
static uint32_t capture, length, selected, highest, seconds;
static int w, h, draining, playing, dirty, pressed;
static char notice[96];

static void set_notice(const char *s, int error)
{
    if (error)
        aster_snprintf(notice, sizeof(notice), "%s (%d)", s, error);
    else
        aster_snprintf(notice, sizeof(notice), "%s", s);
    dirty = 1;
}

static void filename(char *path, size_t n, uint32_t id)
{
    aster_snprintf(path, n, DIR "/REC-%06u.wav", (unsigned)id);
}

/* Select an existing take, including after files were removed on a computer. */
static int browse(int direction)
{
    aster_handle_t dir;
    int rc = aster_fs_opendir(DIR, FOLDER_LEN, &dir);
    if (rc < 0)
        return rc;
    char name[128];
    uint32_t is_dir, best = 0, last = 0;
    while ((rc = aster_fs_dir_next(dir, name, sizeof(name) - 1, &is_dir)) > 0) {
        if (is_dir || rc != 14)
            continue;
        name[rc] = 0;
        if (strncmp(name, "REC-", 4) || strcmp(name + 10, ".wav"))
            continue;
        uint32_t id = 0;
        for (int i = 4; i < 10; i++) {
            if (name[i] < '0' || name[i] > '9') {
                id = 0;
                break;
            }
            id = 10 * id + (uint32_t)(name[i] - '0');
        }
        if (id > last)
            last = id;
        if (direction < 0 && id < selected && id > best)
            best = id;
        if (direction > 0 && id > selected && (!best || id < best))
            best = id;
    }
    aster_fs_closedir(dir);
    if (rc < 0)
        return rc;
    highest = last;
    if (!direction || !last)
        selected = last;
    else if (best)
        selected = best;
    dirty = 1;
    return 0;
}

static void le16(uint8_t *p, uint32_t n)
{
    p[0] = (uint8_t)n;
    p[1] = (uint8_t)(n >> 8);
}

static void le32(uint8_t *p, uint32_t n)
{
    le16(p, n);
    le16(p + 2, n >> 16);
}

static int write_bytes(const uint8_t *p, uint32_t n, uint32_t *written)
{
    while (n) {
        int rc = aster_fs_write(file, p, n);
        if (rc <= 0)
            return rc ? rc : ASTER_EIO;
        p += rc;
        n -= rc;
        if (written)
            *written += rc;
    }
    return 0;
}

static int header(void)
{
    uint8_t wav[44] = "RIFF";
    uint32_t frame = format.channels * (format.bits / 8);
    memcpy(wav + 8, "WAVEfmt ", 8);
    le32(wav + 4, length + 36);
    le32(wav + 16, 16);
    le16(wav + 20, 1);
    le16(wav + 22, format.channels);
    le32(wav + 24, format.sample_rate);
    le32(wav + 28, format.sample_rate * frame);
    le16(wav + 32, frame);
    le16(wav + 34, format.bits);
    memcpy(wav + 36, "data", 4);
    le32(wav + 40, length);
    int rc = aster_fs_seek(file, 0, ASTER_FS_SEEK_SET);
    return rc < 0 ? rc : write_bytes(wav, sizeof(wav), NULL);
}

static void end_take(int error)
{
    if (capture)
        aster_audio_capture_close(capture);
    capture = 0;
    draining = 0;
    if (file) {
        length -= length % (format.channels * (format.bits / 8));
        int rc = header();
        int closed = aster_fs_close(file);
        if (!error)
            error = rc < 0 ? rc : closed;
        file = 0;
    }
    set_notice(error ? "录音中断" : "录音已保存", error);
}

static void start_take(void)
{
    /* Check the directory before opening the mic or creating any file. */
    int rc = browse(0);
    if (rc == ASTER_ENOENT) {
        rc = aster_fs_mkdir(DIR, FOLDER_LEN);
        if (rc >= 0 || rc == ASTER_EEXIST)
            rc = browse(0);
    }
    if (rc < 0 || highest >= 999999) {
        set_notice("无法创建录音", rc < 0 ? rc : ASTER_ENOSPC);
        return;
    }
    rc = aster_audio_capture_open(&format, sizeof(format));
    if (rc <= 0) {
        set_notice(rc == ASTER_EBUSY ? "音频占用中" : "麦克风无法打开", rc);
        return;
    }
    capture = (uint32_t)rc;
    length = seconds = 0;
    if (!format.sample_rate || !format.channels || format.channels > 2 || format.bits != 16) {
        end_take(ASTER_ENOTSUP);
        return;
    }
    char path[64];
    filename(path, sizeof(path), highest + 1);
    rc = aster_fs_open(path, strlen(path), ASTER_FS_O_CREAT | ASTER_FS_O_WRONLY, &file);
    if (rc >= 0) {
        selected = ++highest;
        rc = header();
    }
    if (rc < 0)
        end_take(rc);
    else
        set_notice("正在录音", 0);
}

static void pump(void)
{
    for (int block = 0; block < 8 && capture; block++) {
        int n = aster_audio_capture_read(capture, pcm, sizeof(pcm));
        if (n == ASTER_EAGAIN)
            break;
        if (n <= 0 || length > 0x7fffff00u - (uint32_t)n) {
            end_take(n <= 0 ? n : ASTER_ENOSPC);
            break;
        }
        int rc = write_bytes(pcm, (uint32_t)n, &length);
        if (rc < 0) {
            end_take(rc);
            break;
        }
        uint32_t elapsed = length / (format.channels * (format.bits / 8)) / format.sample_rate;
        if (elapsed != seconds) {
            seconds = elapsed;
            dirty = 1;
        }
    }
}

static void stop_audio(void)
{
    if (capture) {
        aster_audio_capture_finish(capture);
        pump();
        if (capture)
            end_take(0);
    }
    if (playing) {
        aster_audio_stop();
        playing = 0;
        dirty = 1;
    }
}

static int hit(int x, int y)
{
    if (x < 20 || x >= w - 20)
        return 0;
    if (y >= h/2 - 46 && y < h/2 + 30)
        return 1;
    if (y >= h - 104 && y < h - 36)
        return x < w/3 ? 2 : x >= 2*w/3 ? 4 : 3;
    return 0;
}

static void activate(int button)
{
    if (button == 1) {
        if (capture) {
            int rc = aster_audio_capture_finish(capture);
            if (rc < 0) end_take(rc);
            else { draining = 1; set_notice("正在保存", 0); }
        } else {
            stop_audio();
            start_take();
        }
    } else if (!capture && button >= 2) {
        if (button == 3 && playing) {
            stop_audio();
            set_notice("已停止回听", 0);
        } else if (button == 3 && selected) {
            char path[64];
            filename(path, sizeof(path), selected);
            int rc = aster_audio_start_file(path, strlen(path));
            playing = rc == 0;
            set_notice(playing ? "正在回听" : "播放失败", rc);
        } else if (button != 3) {
            stop_audio();
            int rc = browse(button == 2 ? -1 : 1);
            set_notice(rc < 0 ? "读取失败" : "", rc);
        }
    }
}

static void label(int x, int y, int px, int color, const char *s)
{
    aster_ge_text(&ge, x, y + aster_text_asc(px), px, color, ASTER_GFX_NOBG, s);
}

static void render(void)
{
    char s[72];
    (void)aster_ge_begin_frame(&ge);
    aster_ge_clear(&ge, PAPER);
    label(24, 18, 32, INK, "录音机");
    label(24, 63, 20, INK, notice);
    aster_snprintf(s, sizeof(s), "%02u:%02u", (unsigned)seconds / 60, (unsigned)seconds % 60);
    label(24, h/2 - 116, 48, INK, s);
    aster_ge_rect_r(&ge, 20, h/2 - 46, w - 40, 76, capture ? INK : ACCENT, 12, 0);
    label(44, h/2 - 26, 32, PAPER, capture ? (draining ? "保存中" : "停止并保存") : "开始录音");
    if (selected) aster_snprintf(s, sizeof(s), "REC-%06u.wav", (unsigned)selected);
    else aster_snprintf(s, sizeof(s), "暂无录音");
    label(24, h - 154, 24, INK, s);
    const char *actions[] = {"上一条", playing ? "停止" : "回听", "下一条"};
    for (int i = 0; i < 3; i++) {
        int left = i == 0 ? 20 : w*i/3;
        int right = i == 2 ? w - 20 : w*(i+1)/3;
        aster_ge_rect_r(&ge, left, h-104, right-left-4, 68, capture || !selected ? 0x8430 : INK, 10, 0);
        label(left + 10, h - 82, 24, PAPER, actions[i]);
    }
    aster_ge_present(&ge);
    dirty = 0;
}

ASTER_APP_EXPORT int aster_app_init(void)
{
    aster_display_info_t d = {.size = sizeof(d), .version = 1};
    int rc = aster_display_get_info(0, &d, sizeof(d));
    if (rc < 0) return rc;
    w = d.app_width; h = d.app_height;
    aster_surface_desc_t desc = {.size = sizeof(desc), .version = 1, .width = w,
                                 .height = h, .format = ASTER_PIXEL_RGB565};
    rc = aster_surface_create(&desc, sizeof(desc), &surface);
    if (rc < 0) return rc;
    aster_ge_init(&ge, surface, w, h, commands, 96, arena, sizeof(arena));
    rc = browse(0);
    set_notice(rc < 0 && rc != ASTER_ENOENT ? "读取录音目录失败" : "", rc == ASTER_ENOENT ? 0 : rc);
    return aster_app_set_framerate(30);
}

ASTER_APP_EXPORT void aster_app_event(const aster_event_t *e)
{
    if (e->type == ASTER_EVENT_FRAME) {
        if (capture) pump();
        if (playing) {
            aster_audio_state_t st = {.size = sizeof(st), .version = 2};
            if (aster_audio_get_state(&st, sizeof(st)) == 0 && st.state == ASTER_AUDIO_IDLE) {
                playing = 0;
                set_notice("回听结束", 0);
            }
        }
        if (dirty || aster_gfx_fonts_pending()) render();
    } else if (e->type == ASTER_EVENT_TOUCH && e->u.touch.slot == 0) {
        int button = hit(e->u.touch.x, e->u.touch.y);
        if (e->u.touch.action == ASTER_TOUCH_DOWN) pressed = button;
        if (e->u.touch.action == ASTER_TOUCH_MOVE && pressed != button) pressed = 0;
        if (e->u.touch.action == ASTER_TOUCH_UP) {
            if (pressed && pressed == button) activate(button);
            pressed = 0;
        }
    }
}

ASTER_APP_EXPORT void aster_app_pause(void) { pressed = 0; stop_audio(); }
ASTER_APP_EXPORT void aster_app_resume(void) { dirty = 1; }
ASTER_APP_EXPORT void aster_app_stop(void) { stop_audio(); }
ASTER_APP_EXPORT void aster_app_destroy(void)
{
    stop_audio();
    if (surface) aster_surface_destroy(surface);
    surface = 0;
}
