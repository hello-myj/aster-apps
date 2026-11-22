/* SPDX-License-Identifier: MIT */
#include <aster_imports.h>
#include <aster_gfx_engine.h>
#include "cover.h"
#include "flac_lyrics.h"
#include "lrc.h"
#include "paper.h"
#include "shake.h"

enum page { LYRICS, PLAYBACK, LIBRARY, PAPER };

enum control {
    NONE,
    CLOSE,
    VOLUME,
    SEEK,
    PREVIOUS,
    TOGGLE,
    NEXT,
    DEMO,
    PAGE_PREV,
    PAGE_NEXT,
    TRACK_FIRST
};

enum { DESIGN = 460, ROWS = 5, TRACK_LIMIT = 64, PROGRESS_X = 33, PROGRESS_W = 394 };

enum {
    HEADER_END = 59,
    CONTENT_END = 455,
    LYRIC_SEEK_TOP = 370,
    LYRIC_PROGRESS = 393,
    ROW_TOP = 108,
    ROW_H = 57,
    LIBRARY_PAGER = ROW_TOP + ROWS * ROW_H,
    VOLUME_X = 76,
    VOLUME_W = 161,
    VOLUME_Y = 316,
    PLAY_SEEK_X = 27,
    PLAY_SEEK_W = 209,
    PLAY_PROGRESS = 415,
    PANEL_X = 254,
    PANEL_Y = 301,
    PANEL_W = 184,
    PANEL_H = 140,
    STEP_X = PANEL_X + 8,
    STEP_W = 58,
    STEP_UP = PANEL_Y + 8,
    STEP_DOWN = PANEL_Y + 74,
    STEP_H = 58,
    TOGGLE_X = PANEL_X + 74,
    TOGGLE_W = 102
};

#define RGB(r, g, b) ((uint16_t)(((r) & 0xf8) << 8 | ((g) & 0xfc) << 3 | (b) >> 3))
static const uint16_t background = RGB(13, 17, 22), rim = RGB(16, 22, 29), chrome = RGB(26, 34, 44),
                      ink = RGB(243, 240, 232), muted = RGB(162, 170, 178),
                      border = RGB(53, 60, 71), gold = RGB(237, 186, 115), panel = RGB(34, 43, 54),
                      selected = RGB(48, 42, 36), lyric_accent = RGB(255, 157, 78),
                      rail = RGB(58, 68, 81);

static aster_handle_t surface, directory, lyric_file;
static aster_ge_t gfx;
static aster_gfx_cmd_t commands[160];
static uint8_t arena[4096];
static uint32_t width, height, content_w, content_h;
static int left, top;
static aster_player_info_t player;
static uint32_t last_poll, visible_second;
static int volume, library_page, track_count, scan_error;
static bool dirty, library_scanned;
static enum page page = PLAYBACK;
static char tracks[TRACK_LIMIT][248], current_file[256], queued_file[256];
static char playing_track[sizeof(current_file)];
static bool start_busy;
static uint32_t start_retry_until;
static char message[128];
static uint32_t message_until;

static lrc_t lyrics;
static unsigned char lyric_bytes[LRC_FILE_LIMIT];
static unsigned char gbk_table[LRC_GBK_BYTES];
static bool gbk_ready;
static size_t lyric_size;
static enum { SIDECAR, FLAC_SCAN, FLAC_TAG, FLAC_TEXT } lyric_source;
static flac_lyrics_t flac_scan;
static lyric_span_t lyric_value, plain_candidate;
static bool plain_lyrics;
static unsigned plain_cue, plain_byte;
static char lyric_track[256];
static bool lyric_error;
static int lyric_line = -1;
static uint32_t lyric_scroll;
static bool lyric_animating;
static int paper_line = -1;
static unsigned paper_previous, paper_style;
static uint32_t paper_began;
/* Predict when a lyric will be visible from the cost of presenting it.
 * All estimates start at the latest absolute audio position. */
enum { LYRIC_MAX_LEAD_MS = 100 };
static uint32_t player_sample_at, camera_cut_ms = 80, camera_draw_ms;
static uint32_t camera_prepare_ms, lyric_page_draw_ms;
static bool camera_new_frame;
static unsigned live_scene, rendered_live_scene;
static enum page motion_origin = PLAYBACK;
static enum lyric_effect motion_effect = LYRIC_EFFECT_COUNT;
static uint32_t motion_began;
static bool motion_leaving, motion_transition;
static bool has_accelerometer, imu_available;
static struct {
    bool armed;
    int x, y;
    uint32_t released;
} lyric_tap;
static shake_t shake;
static uint32_t shake_after, imu_sequence, imu_retry_at;

static struct {
    bool down, moved, rejected, page_swipe, tap_candidate;
    uint32_t pressed;
    int x, y, control;
} finger;

static enum { SEEK_IDLE, SEEK_DRAG, SEEK_WAIT } seeking;

static uint32_t seek_position, seek_deadline;

static int sx(int x)
{
    return x * (int)content_w / DESIGN;
}

static int sy(int y)
{
    return y * (int)content_h / DESIGN;
}

static int clamp(int n, int min, int max)
{
    return n < min ? min : n > max ? max : n;
}

static int abs_i(int n)
{
    return n < 0 ? -n : n;
}

static void box(int x, int y, int w, int h, int radius, uint16_t color, bool outline)
{
    int px = left + sx(x), py = top + sy(y);
    int pw = sx(x + w) - sx(x), ph = sy(y + h) - sy(y);
    if (pw > 0 && ph > 0)
        aster_ge_rect_r(&gfx, px, py, pw, ph, color, sx(radius), outline);
}

static void circle(int x, int y, int r, uint16_t color)
{
    aster_ge_ellipse(&gfx, left + sx(x), top + sy(y), sx(r), sy(r), color, 0);
}

static void line(int x, int y, int to_x, int to_y, uint16_t color)
{
    aster_ge_line_aa(&gfx, left + sx(x), top + sy(y), left + sx(to_x), top + sy(to_y), color);
}

static void triangle(int x, int y, int to_x, int to_y, int end_x, int end_y, uint16_t color)
{
    aster_ge_tri(&gfx, left + sx(x), top + sy(y), left + sx(to_x), top + sy(to_y), left + sx(end_x),
                 top + sy(end_y), color);
}

enum align { LEFT, CENTER, RIGHT };

static int font(int px)
{
    return aster_text_snap((int)((content_w * (uint32_t)px + DESIGN / 2) / DESIGN));
}

static void text_run(int x, int py, int w, int size, uint16_t color, const char *value,
                     enum align align)
{
    int span = sx(w);
    int offset = 0;
    if (align != LEFT) {
        int advance = aster_gfx_text_w(size, value, strlen(value));
        if (advance > 0 && advance < span)
            offset = (span - advance) / (align == CENTER ? 2 : 1);
    }
    aster_ge_text(&gfx, left + sx(x) + offset, top + py + aster_text_asc(size), size, color,
                  ASTER_GFX_NOBG, value);
}

static void text(int x, int y, int w, int px, uint16_t color, const char *value, enum align align)
{
    if (!value[0])
        return;
    char shown[224];
    int size = font(px);
    aster_text_elide(size, value, strlen(value), sx(w), shown, sizeof(shown));
    text_run(x, sy(y), w, size, color, shown, align);
}

static bool bluetooth_input(void)
{
    return player.mode == ASTER_PLAYER_BLUETOOTH;
}

static const char *basename_of(const char *path)
{
    const char *name = path;
    for (; *path; path++)
        if (*path == '/')
            name = path + 1;
    return name;
}

static const char *sandbox_track(const char *path)
{
    static const char example[] = "/assets/audio/demo.flac";
    static const char sd_app[] = "/SD:/aster/apps/aster.hifi/";
    static const char sd_alias[] = "/SD/aster/apps/aster.hifi/";
    if (!strncmp(path, "/SD:/music/", 11))
        return path + 4;
    if (!strncmp(path, "/SD/music/", 10))
        return path + 3;
    if (!strncmp(path, "/music/", 7) || !strcmp(path, example))
        return path;
    if (!strncmp(path, sd_app, sizeof(sd_app) - 1) ||
        !strncmp(path, sd_alias, sizeof(sd_alias) - 1)) {
        size_t length = strlen(path);
        if (length >= sizeof(example) - 1 &&
            !strcmp(path + length - (sizeof(example) - 1), example))
            return example;
    }
    return "";
}

static char lower(char c)
{
    return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
}

static const char *format(const char *path)
{
    const char *ext = NULL;
    for (const char *p = basename_of(path); *p; p++)
        if (*p == '.')
            ext = p + 1;
    if (!ext)
        return "";
    char kind[5] = {0};
    size_t n = strlen(ext);
    if (n >= sizeof(kind))
        return "";
    for (size_t i = 0; i < n; i++)
        kind[i] = lower(ext[i]);
    if (!strcmp(kind, "mp3"))
        return "MP3";
    if (!strcmp(kind, "wav"))
        return "WAV";
    if (!strcmp(kind, "flac"))
        return "FLAC";
    return "";
}

static void display_name(const char *path, char name[256])
{
    const char *base = basename_of(path);
    size_t length = strlen(base);
    if (*format(base))
        length -= strlen(format(base)) + 1;
    memcpy(name, base, length);
    name[length] = 0;
}

static bool numeric_title(const char *value)
{
    while (*value == ' ' || *value == '\t')
        value++;
    const char *start = value;
    while (*value >= '0' && *value <= '9')
        value++;
    if (value == start)
        return false;
    while (*value == ' ' || *value == '\t')
        value++;
    return !*value;
}

static const char *title(char name[256])
{
    if (bluetooth_input())
        return *player.title ? player.title : "等待手机播放";
    if (*lyrics.title && !numeric_title(lyrics.title) &&
        !strcmp(lyric_track, sandbox_track(player.path)))
        return lyrics.title;
    if (!strcmp(sandbox_track(player.path), "/assets/audio/demo.flac"))
        return "试听示例";
    if (*player.path) {
        display_name(player.path, name);
        return name;
    }
    return "选择一首音乐";
}

static const char *artist(void)
{
    if (!bluetooth_input() && *lyrics.artist && !strcmp(lyric_track, sandbox_track(player.path)))
        return lyrics.artist;
    return *player.artist ? player.artist : player.album;
}

static void notice(const char *value)
{
    aster_snprintf(message, sizeof(message), "%s", value);
    message_until = aster_time_now_ms() + 3500;
    dirty = true;
}

static int player_command(uint32_t command, uint32_t value, const char *path)
{
    aster_player_request_t request = {
        .size = sizeof(request), .version = 1, .command = command, .value = value};
    if (path)
        aster_snprintf(request.path, sizeof(request.path), "%s", path);
    return aster_player_control(&request, sizeof(request));
}

static int report_control(int result)
{
    if (result == ASTER_EBUSY)
        notice("音频正忙，请稍后再试");
    else if (result == ASTER_ESTATE && bluetooth_input())
        notice("请在手机上打开音乐应用");
    else if (result == ASTER_ENOTSUP)
        notice("当前设备或曲目不支持此操作");
    else if (result < 0) {
        char value_text[64];
        aster_snprintf(value_text, sizeof(value_text), "操作失败 (%d)", result);
        notice(value_text);
    }
    return result;
}

static int control(uint32_t command, uint32_t value, const char *path)
{
    return report_control(player_command(command, value, path));
}

static void scan_close(void)
{
    if (directory)
        aster_fs_closedir(directory);
    directory = 0;
}

static void scan_start(void)
{
    scan_close();
    track_count = library_page = 0;
    library_scanned = false;
    scan_error = aster_fs_opendir("/music", 6, &directory);
    if (scan_error < 0) {
        directory = 0;
        library_scanned = true;
    }
}

static void scan_tick(void)
{
    for (unsigned i = 0; directory && i < 4; i++) {
        char name[256];
        uint32_t is_dir = 0;
        int n = aster_fs_dir_next(directory, name, sizeof(name), &is_dir);
        if (n <= 0) {
            scan_error = n;
            scan_close();
            library_scanned = true;
        } else if (!is_dir && (size_t)n < sizeof(tracks[0])) {
            name[n] = 0;
            if (*format(name)) {
                memcpy(tracks[track_count++], name, n + 1);
                if (track_count == TRACK_LIMIT) {
                    scan_close();
                    library_scanned = true;
                }
            }
        }
        if (page == LIBRARY)
            dirty = true;
    }
}

static void lyrics_close(void)
{
    if (lyric_file)
        aster_fs_close(lyric_file);
    lyric_file = 0;
}

static void open_embedded_lyrics(void)
{
    if (strcmp(format(lyric_track), "FLAC"))
        return;
    aster_stat_t stat = {.size = sizeof(stat), .version = 1};
    size_t n = strlen(lyric_track);
    if (aster_fs_stat(lyric_track, n, &stat, sizeof(stat)) < 0 ||
        aster_fs_open(lyric_track, n, ASTER_FS_O_RDONLY, &lyric_file) < 0) {
        lyric_error = true;
        return;
    }
    if (!flac_lyrics_begin(&flac_scan, lyric_file,
                           stat.filesize > INT32_MAX ? INT32_MAX : (uint32_t)stat.filesize)) {
        lyrics_close();
        return;
    }
    lyric_source = FLAC_SCAN;
}

static void lyrics_track(const char *path)
{
    if (!strcmp(path, lyric_track))
        return;
    lyrics_close();
    aster_snprintf(lyric_track, sizeof(lyric_track), "%s", path);
    lrc_parse(&lyrics, NULL, 0, NULL);
    lyrics.text[0] = 0;
    lyric_size = 0;
    lyric_source = SIDECAR;
    plain_candidate = (lyric_span_t){0};
    plain_lyrics = false;
    plain_cue = plain_byte = 0;
    lyric_error = false;
    lyric_line = -1;
    paper_line = -1;
    lyric_animating = false;
    dirty = true;
    if (!*format(path))
        return;
    char sidecar[256];
    size_t prefix = strlen(path) - strlen(format(path));
    if (prefix + 4 > sizeof(sidecar))
        return;
    memcpy(sidecar, path, prefix);
    memcpy(sidecar + prefix, "lrc", 4);
    int r = aster_fs_open(sidecar, prefix + 3, ASTER_FS_O_RDONLY, &lyric_file);
    if (r < 0) {
        lyric_file = 0;
        lyric_error = r != ASTER_ENOENT;
        open_embedded_lyrics();
    }
}

static bool load_gbk(void)
{
    if (gbk_ready)
        return true;
    static const char path[] = "/assets/text/gbk.bin";
    aster_handle_t file;
    if (aster_fs_open(path, sizeof(path) - 1, ASTER_FS_O_RDONLY, &file) < 0)
        return false;
    size_t used = 0;
    while (used < sizeof(gbk_table)) {
        int n = aster_fs_read(file, gbk_table + used, sizeof(gbk_table) - used);
        if (n <= 0)
            break;
        used += n;
    }
    aster_fs_close(file);
    gbk_ready = used == sizeof(gbk_table);
    return gbk_ready;
}

static void lyrics_read(void)
{
    if (!lyric_file)
        return;
    if (lyric_source == FLAC_SCAN) {
        enum flac_result found = flac_lyrics_next(&flac_scan, &lyric_value);
        if (found == FLAC_MORE)
            return;
        if (found == FLAC_FOUND) {
            lyric_source = FLAC_TAG;
        } else if (plain_candidate.length) {
            lyric_source = FLAC_TEXT;
            lyric_value = plain_candidate;
        } else {
            lyric_error |= found == FLAC_INVALID;
            lyrics_close();
            dirty = true;
            return;
        }
        lyric_size = 0;
        if (aster_fs_seek(lyric_file, lyric_value.offset, ASTER_FS_SEEK_SET) < 0) {
            lyric_error = dirty = true;
            lyrics_close();
            return;
        }
    }
    bool embedded = lyric_source != SIDECAR;
    size_t remaining = (embedded ? lyric_value.length : sizeof(lyric_bytes)) - lyric_size;
    unsigned char extra;
    int n = 0;
    if (remaining)
        n = aster_fs_read(lyric_file, lyric_bytes + lyric_size, remaining < 4096 ? remaining : 4096);
    else if (!embedded)
        n = aster_fs_read(lyric_file, &extra, 1);
    if (n > 0 && remaining) {
        lyric_size += n;
        return;
    }
    lyric_error = n != 0 || (embedded && remaining);
    if (!lyric_error) {
        enum lrc_encoding encoding = lrc_detect(lyric_bytes, lyric_size);
        if (lyric_source == FLAC_TEXT) {
            lrc_parse_plain_utf8(&lyrics, lyric_bytes, lyric_size);
            plain_lyrics = lyrics.count != 0;
        } else if (embedded && encoding != LRC_UTF8) {
            lyric_error = true;
        } else if (encoding == LRC_GBK && !load_gbk()) {
            lyric_error = true;
        } else {
            lrc_parse(&lyrics, lyric_bytes, lyric_size, gbk_table);
            if (!lyrics.count && lyric_source == FLAC_TAG && !plain_candidate.length) {
                lrc_parse_plain_utf8(&lyrics, lyric_bytes, lyric_size);
                if (lyrics.count)
                    plain_candidate = lyric_value;
                lrc_parse(&lyrics, NULL, 0, NULL);
            }
        }
    }
    if (!lyrics.count && lyric_source == FLAC_TAG) {
        lyric_source = FLAC_SCAN;
        return;
    }
    lyrics_close();
    if (!lyrics.count && !embedded)
        open_embedded_lyrics();
    if (lyrics.count)
        aster_printf("HiFi lyrics: %s %s lines=%u bytes=%u\n",
                     embedded ? "FLAC" : "sidecar", plain_lyrics ? "plain" : "timed",
                     lyrics.count, (unsigned)lyric_size);
    dirty = true;
}

/* A car-lyrics player publishes one display line in AVRCP metadata. There is
 * no time-coded transcript or word timing. Reuse the local line layouts. */
static void sync_media(void)
{
    if (!bluetooth_input()) {
        const char *path = sandbox_track(player.path);
        lyrics_track(path);
        cover_select(path);
        live_scene = 0;
        return;
    }
    lyrics_track("@bluetooth");
    cover_select_remote(player.path, player.flags & ASTER_PLAYER_F_COVER ? player.reserved[2] : 0);
    const char *line = player.flags & ASTER_PLAYER_F_REMOTE ? player.title : "";
    if (!strcmp(line, lyrics.text))
        return;
    aster_snprintf(lyrics.text, sizeof(player.title), "%s", line);
    lyrics.count = *line ? 1 : 0;
    lyrics.cue[0] = (lrc_cue_t){0};
    lyric_line = *line ? 0 : -1;
    live_scene++;
    dirty = true;
}

static uint32_t position(void)
{
    if (seeking != SEEK_IDLE && !bluetooth_input())
        return seek_position;
    return player.position_ms;
}

static bool has_position(void)
{
    return bluetooth_input() ? (player.flags & ASTER_PLAYER_F_POSITION) != 0
                             : player.duration_ms != 0;
}

static bool can_seek(void)
{
    return player.mode == ASTER_PLAYER_LOCAL && player.duration_ms != 0;
}

static bool local_playing(void)
{
    return player.mode == ASTER_PLAYER_LOCAL && player.state == ASTER_PLAYER_PLAYING &&
           seeking == SEEK_IDLE;
}

static uint32_t lyric_position(uint32_t now, uint32_t rendering_ms)
{
    uint32_t at = position();
    if (local_playing() && at < player.duration_ms) {
        uint32_t elapsed = now - player_sample_at;
        uint32_t advance = (elapsed < 200 ? elapsed : 200) + rendering_ms;
        uint32_t left_ms = player.duration_ms - at;
        at += advance < left_ms ? advance : left_ms;
    }
    return at;
}

static int timed_cue(uint32_t now, uint32_t draw_ms)
{
    uint32_t at = lyric_position(now, draw_ms);
    int next = lrc_active(&lyrics, at);
    /* A cheaper cut can reduce the estimate while the first frame is already
     * visible. Keep that cue until the playhead catches up, except on seeks. */
    if (local_playing() && next < lyric_line && lyric_line >= 0 &&
        (int64_t)lyric_position(now, LYRIC_MAX_LEAD_MS) + lyrics.offset_ms >=
            lyrics.cue[lyric_line].time_ms)
        return lyric_line;
    return next;
}

static void lyrics_tick(uint32_t now)
{
    lyrics_read();
    int next;
    if (plain_lyrics)
        next = (int)plain_cue;
    else if (page == LYRICS)
        next = timed_cue(now, lyric_page_draw_ms);
    else if (page == PAPER)
        next = timed_cue(now, motion_effect == CAMERA_PUSH ? camera_cut_ms : 0);
    else
        next = lrc_active(&lyrics, position());
    if (lyric_line != next) {
        lyric_animating = page == LYRICS && seeking == SEEK_IDLE && lyric_line >= 0 &&
                          next > lyric_line &&
                          lyrics.cue[next - 1].time_ms == lyrics.cue[lyric_line].time_ms;
        lyric_scroll = now;
        lyric_line = next;
        if (page == LYRICS)
            dirty = true;
    }
    if (lyric_animating) {
        if (now - lyric_scroll >= 240)
            lyric_animating = false;
        if (page == LYRICS)
            dirty = true;
    }
}

static void set_page(enum page target)
{
    if (page == target)
        return;
    lyric_tap.armed = false;
    if (page == PAPER || target == PAPER) {
        uint32_t now = aster_time_now_ms();
        memset(&shake, 0, sizeof(shake));
        shake.wait_for_rest = true;
        shake.last_sample = now;
        shake_after = now + 800;
        paper_line = -1;
        paper_previous = paper_style = 0;
        paper_began = now;
        paper_invalidate();
        motion_transition = motion_leaving = false;
    }
    page = target;
    lyric_animating = false;
    if (target == LIBRARY && library_scanned && !track_count)
        scan_start();
    dirty = true;
}

static enum lyric_effect pick_lyric_effect(uint32_t now, uint32_t sequence)
{
    /* Mix the gesture timing and IMU sample counter before choosing a style. */
    uint32_t draw = now + sequence * 0x9e3779b9u;
    draw ^= draw >> 16;
    draw *= 0x85ebca6bu;
    draw ^= draw >> 13;
    if (motion_effect == LYRIC_EFFECT_COUNT)
        return (enum lyric_effect)(draw % LYRIC_EFFECT_COUNT);
    unsigned step = 1u + draw % (LYRIC_EFFECT_COUNT - 1u);
    return (enum lyric_effect)((motion_effect + step) % LYRIC_EFFECT_COUNT);
}

static void toggle_motion(uint32_t now, uint32_t sequence)
{
    if (page != PAPER) {
        motion_origin = page;
        motion_effect = pick_lyric_effect(now, sequence);
        set_page(PAPER);
        paper_line = -2; /* Prepare the opening frame even during a lyric gap. */
        paper_style = paper_previous = (unsigned)motion_effect * LYRIC_VARIANTS;
    } else {
        motion_leaving = true;
    }
    motion_began = now;
    motion_transition = true;
    shake_after = now + LYRIC_ENTER_MS;
    shake.wait_for_rest = true;
    shake.last_sample = now;
    shake.rest_ms = 0;
    dirty = true;
    aster_printf("HiFi lyric motion effect=%u return=%u", (unsigned)motion_effect, motion_leaving);
}

static void shake_poll(uint32_t now)
{
    if (motion_leaving || finger.down || (!imu_available && (int32_t)(now - imu_retry_at) < 0))
        return;
    aster_imu_sample_t sample = {.size = sizeof(sample), .version = 1};
    int result = aster_imu_read(&sample, sizeof(sample));
    if (result < 0 || !(sample.flags & ASTER_IMU_ACCEL_VALID)) {
        if (imu_available)
            aster_printf("HiFi accelerometer unavailable (%d), retrying", result);
        imu_available = false;
        imu_retry_at = now + 1000;
        return;
    }
    if (!imu_available) {
        imu_available = true;
        aster_printf("HiFi accelerometer ready seq=%u", sample.sequence);
    }
    if (sample.sequence == imu_sequence)
        return;
    imu_sequence = sample.sequence;
    if (!shake_update(&shake, sample.accel, now) || (int32_t)(now - shake_after) < 0)
        return;
    toggle_motion(now, sample.sequence);
}

static const char *source_label(void)
{
    if (bluetooth_input())
        return "蓝牙接收";
    if (player.flags & ASTER_PLAYER_F_BT_OUTPUT)
        return "蓝牙输出";
    return "有线";
}

static const char *playback_state(void)
{
    if (player.state == ASTER_PLAYER_PLAYING)
        return "正在播放";
    if (player.state == ASTER_PLAYER_PAUSED)
        return "已暂停";
    if (player.state == ASTER_PLAYER_OPENING)
        return "正在读取";
    if (player.state == ASTER_PLAYER_STOPPING)
        return "正在切换";
    if (player.state == ASTER_PLAYER_ENDED)
        return "已播放完毕";
    if (player.state == ASTER_PLAYER_ERROR)
        return "无法播放";
    return bluetooth_input() ? "等待手机连接" : "本地播放";
}

static void frame_chrome(void)
{
    if (page != PLAYBACK)
        aster_ge_clear(&gfx, background);
    if (page == LYRICS)
        return;
    if (page == LIBRARY) {
        text(22, 17, 173, 24, ink, "本地曲库", LEFT);
        text(260, 20, 109, 18, muted, source_label(), RIGHT);
        box(386, 9, 55, 46, 12, finger.control == CLOSE ? panel : chrome, false);
    }
    line(405, 23, 423, 41, muted);
    line(423, 23, 405, 41, muted);
}

static void clock_text(char out[24], uint32_t ms)
{
    uint32_t secs = ms / 1000;
    aster_snprintf(out, 24, "%02u:%02u", secs / 60, secs % 60);
}

static void progress(int y, bool compact)
{
    int bar_height = compact ? 6 : 9;
    int time_y = y + (compact ? 24 : 16);
    int time_size = compact ? 16 : 18;
    box(PROGRESS_X, y, PROGRESS_W, bar_height, bar_height / 2, rail, false);
    if (player.duration_ms && has_position()) {
        uint32_t now = position();
        if (now > player.duration_ms)
            now = player.duration_ms;
        int fill = (int)((uint64_t)now * PROGRESS_W / player.duration_ms);
        if (fill)
            box(PROGRESS_X, y, fill, bar_height, bar_height / 2, gold, false);
        if (can_seek())
            circle(PROGRESS_X + fill, y + bar_height / 2, compact ? 7 : 11, gold);
    }
    char time[24];
    clock_text(time, position());
    text(32, time_y, 145, time_size, seeking != SEEK_IDLE ? gold : ink,
         has_position() ? time : "--:--", LEFT);
    clock_text(time, player.duration_ms);
    text(283, time_y, 145, time_size, muted, player.duration_ms ? time : "--:--", RIGHT);
}

static void fallback_art(uint16_t base)
{
    aster_ge_clear(&gfx, base);
    int cx = left + sx(217), cy = top + sy(109);
    for (int i = 11; i >= 0; i--) {
        uint16_t glow = cover_mix(base, RGB(43, 110, 150), 170 - i * 13);
        aster_ge_ellipse(&gfx, cx + sx(i * 3), cy, sx(27 + i * 12), sy(15 + i * 5), glow, 0);
    }
    for (int i = 0; i < 11; i++) {
        uint16_t edge = cover_mix(RGB(160, 211, 239), base, 35 + i * 13);
        aster_ge_ellipse(&gfx, left + sx(185 + i * 6), cy, sx(26 + i * 12), sy(13 + i * 5), edge,
                         1);
    }
    circle(174, 105, 8, RGB(195, 231, 250));
}

static void playback_slider(int x, int y, int w, int fill, uint16_t track, uint16_t accent,
                            bool knob)
{
    box(x, y, w, 4, 2, track, false);
    if (fill)
        box(x, y, fill, 4, 2, accent, false);
    if (knob)
        circle(x + fill, y + 2, 6, accent);
}

static void playback(void)
{
    char name[256], parameters[48];
    uint16_t base = RGB(14, 26, 40), accent = RGB(157, 218, 248);
    uint16_t tray = RGB(24, 43, 63);
    uint16_t key = cover_mix(base, accent, 34), track = cover_mix(base, accent, 67);
    uint16_t subtitle = cover_mix(ink, base, 58);
    if (cover_paint(surface, left, top, content_w, base, accent))
        aster_ge_touch(&gfx, 0, 0, width, height);
    else
        fallback_art(base);
    text(25, 224, 235, 32, ink, title(name), LEFT);
    const char *by = artist();
    text(27, 266, 270, 16, subtitle, *by ? by : source_label(), LEFT);

    if (player.rate) {
        const char *codec = bluetooth_input() ? "SBC" : format(player.path);
        if (*codec)
            aster_snprintf(parameters, sizeof(parameters), "%s/%uHz/%ubit", codec, player.rate,
                           player.bits);
        else
            aster_snprintf(parameters, sizeof(parameters), "%uHz/%ubit", player.rate, player.bits);
        text(264, 234, 171, 14, subtitle, parameters, RIGHT);
    } else
        text(264, 233, 171, 16, subtitle, playback_state(), RIGHT);
    line(26, 291, 435, 291, track);
    text(27, VOLUME_Y - 8, 47, 14, subtitle, "音量", LEFT);
    playback_slider(VOLUME_X, VOLUME_Y, VOLUME_W, volume * VOLUME_W / 100, track, accent, true);

    char stamp[24];
    uint32_t pos = position();
    if (player.duration_ms && pos > player.duration_ms)
        pos = player.duration_ms;
    clock_text(stamp, pos);
    text(25, 348, 153, 48, seeking != SEEK_IDLE ? accent : ink,
         has_position() ? stamp : "--:--", LEFT);
    if (player.duration_ms)
        clock_text(stamp, player.duration_ms);
    text(177, 376, 60, 14, subtitle, player.duration_ms ? stamp : "--:--", RIGHT);
    int fill = player.duration_ms ? (uint64_t)pos * PLAY_SEEK_W / player.duration_ms : 0;
    playback_slider(PLAY_SEEK_X, PLAY_PROGRESS, PLAY_SEEK_W, fill, track, accent, can_seek());

    box(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, 19, tray, false);
    box(STEP_X, STEP_UP, STEP_W, STEP_H, 12, finger.control == PREVIOUS ? track : key, false);
    box(STEP_X, STEP_DOWN, STEP_W, STEP_H, 12, finger.control == NEXT ? track : key, false);
    box(TOGGLE_X, STEP_UP, TOGGLE_W, STEP_DOWN + STEP_H - STEP_UP, 15,
        finger.control == TOGGLE ? ink : accent, false);
    int cx = STEP_X + STEP_W / 2, prev_y = STEP_UP + STEP_H / 2, next_y = STEP_DOWN + STEP_H / 2;
    box(cx - 12, prev_y - 10, 3, 20, 1, ink, false);
    triangle(cx - 6, prev_y, cx + 11, prev_y - 10, cx + 11, prev_y + 10, ink);
    triangle(cx - 11, next_y - 10, cx + 6, next_y, cx - 11, next_y + 10, ink);
    box(cx + 9, next_y - 10, 3, 20, 1, ink, false);
    cx = TOGGLE_X + TOGGLE_W / 2;
    int cy = PANEL_Y + PANEL_H / 2;
    if (player.state == ASTER_PLAYER_PLAYING) {
        box(cx - 10, cy - 15, 7, 30, 2, base, false);
        box(cx + 3, cy - 15, 7, 30, 2, base, false);
    } else
        triangle(cx - 9, cy - 16, cx + 15, cy, cx - 9, cy + 16, base);
}

static int next_lyric_group(int first)
{
    int next = first + 1;
    if (plain_lyrics)
        return next;
    while (next < (int)lyrics.count && lyrics.cue[next].time_ms == lyrics.cue[first].time_ms)
        next++;
    return next;
}

static int previous_lyric_group(int first)
{
    int prior = first - 1;
    while (prior > 0 && lyrics.cue[prior].time_ms == lyrics.cue[prior - 1].time_ms)
        prior--;
    return prior;
}

struct lyric_row {
    const char *start;
    uint32_t length;
};

static int wrap_lyric(int first, int end, int size, struct lyric_row *rows, int limit)
{
    int count = 0;
    for (int cue = first; cue < end; cue++) {
        const char *body = lyrics.text + lyrics.cue[cue].text_offset;
        uint32_t length = strlen(body);
        while (length) {
            if (count == limit)
                return count + 1;
            uint32_t shown;
            uint32_t consumed = aster_text_line(size, body, length, sx(400), &shown);
            if (!consumed)
                return 0;
            rows[count++] = (struct lyric_row){body, shown};
            length -= consumed;
            body += consumed;
        }
    }
    return count;
}

struct active_lyric {
    struct lyric_row rows[6];
    int count, size, step;
    bool clipped;
};

static struct active_lyric measure_active_lyric(int focus)
{
    static const int roles[] = {32, 28, 24, 20, 18};
    struct active_lyric block = {0};
    int next = next_lyric_group(focus);
    for (unsigned i = 0; i < sizeof(roles) / sizeof(roles[0]); i++) {
        block.size = font(roles[i]);
        block.step = aster_text_line_h(block.size);
        int limit = clamp(sy(128) / block.step, 1, sizeof(block.rows) / sizeof(block.rows[0]));
        int count = wrap_lyric(focus, next, block.size, block.rows, limit);
        block.count = clamp(count, 0, limit);
        block.clipped = count > limit;
        if (!block.clipped)
            break;
    }
    return block;
}

static void draw_active_lyric(const struct active_lyric *block, int y)
{
    for (int i = 0; i < block->count; i++, y += block->step) {
        char value[768];
        const struct lyric_row *row = &block->rows[i];
        if (block->clipped && i == block->count - 1) {
            aster_text_elide(block->size, row->start, strlen(row->start), sx(400), value,
                             sizeof(value));
        } else {
            memcpy(value, row->start, row->length);
            value[row->length] = 0;
        }
        text_run(30, y, 400, block->size, lyric_accent, value, LEFT);
    }
}

static const char *lyric_preview(int first)
{
    int end = next_lyric_group(first);
    for (int i = first; i < end; i++) {
        const char *line = lyrics.text + lyrics.cue[i].text_offset;
        if (*line)
            return line;
    }
    return NULL;
}

static void draw_context_lyric(const char *value, int y, int size, uint16_t color)
{
    char shown[224];
    aster_text_elide(size, value, strlen(value), sx(400), shown, sizeof(shown));
    text_run(30, y, 400, size, color, shown, LEFT);
}

static void lyric_scroll_view(int top_y, uint32_t now)
{
    enum { CONTEXT_LIMIT = 12 };

    const char *prior[CONTEXT_LIMIT], *after[CONTEXT_LIMIT];
    int prior_count = 0, after_count = 0;
    int top_px = sy(top_y), bottom = sy(LYRIC_SEEK_TOP - 8);
    int size = font(20), line_h = aster_text_line_h(size), gap = sy(6);
    int step = line_h + gap;
    for (int i = lyric_line < 0 ? -1 : previous_lyric_group(lyric_line);
         i >= 0 && prior_count < CONTEXT_LIMIT; i = previous_lyric_group(i)) {
        const char *value = lyric_preview(i);
        if (value)
            prior[prior_count++] = value;
    }
    for (int i = lyric_line < 0 ? 0 : next_lyric_group(lyric_line);
         i < (int)lyrics.count && after_count < CONTEXT_LIMIT; i = next_lyric_group(i)) {
        const char *value = lyric_preview(i);
        if (value)
            after[after_count++] = value;
    }

    struct active_lyric active = {0};
    if (lyric_line >= 0)
        active = measure_active_lyric(lyric_line);
    int height = active.count * active.step;
    int anchor = top_px;
    if (lyric_line >= 0) {
        int upper = clamp(top_px + prior_count * step, top_px, bottom - height);
        int lower = clamp(bottom - height - after_count * step - gap, top_px, upper);
        anchor = clamp((top_px + bottom - height) / 2, lower, upper);
        if (lyric_animating) {
            int remaining = 240 - (int)(now - lyric_scroll);
            if (remaining > 0) {
                int delta = (step + (height - line_h) / 2) * remaining * remaining / (240 * 240);
                anchor = clamp(anchor + delta, top_px, bottom - height);
            }
        }
        draw_active_lyric(&active, anchor);
    }
    for (int i = 0; i < prior_count; i++) {
        int y = anchor - (i + 1) * step;
        if (y < top_px)
            break;
        draw_context_lyric(prior[i], y, size, i < 2 ? muted : RGB(116, 124, 135));
    }
    int y = anchor + height + (height ? gap : 0);
    for (int i = 0; i < after_count && y + line_h <= bottom; i++, y += step)
        draw_context_lyric(after[i], y, size, i < 2 ? muted : RGB(116, 124, 135));
}

static unsigned plain_row(unsigned cue, unsigned offset, uint32_t *shown)
{
    const char *body = lyrics.text + lyrics.cue[cue].text_offset + offset;
    return aster_text_line(font(24), body, strlen(body), sx(400), shown);
}

static void scroll_plain(int steps)
{
    int direction = steps > 0 ? 1 : -1;
    for (; steps; steps -= direction) {
        const char *body = lyrics.text + lyrics.cue[plain_cue].text_offset;
        uint32_t shown;
        if (direction > 0) {
            unsigned n = plain_row(plain_cue, plain_byte, &shown);
            if (n && body[plain_byte + n])
                plain_byte += n;
            else if (plain_cue + 1 < lyrics.count) {
                plain_cue++;
                plain_byte = 0;
            } else
                return;
        } else {
            unsigned limit = plain_byte;
            if (!limit) {
                if (!plain_cue)
                    return;
                plain_cue--;
                limit = strlen(lyrics.text + lyrics.cue[plain_cue].text_offset);
            }
            plain_byte = 0;
            unsigned n;
            while ((n = plain_row(plain_cue, plain_byte, &shown)) && n < limit - plain_byte)
                plain_byte += n;
        }
    }
}

static void plain_view(int y)
{
    text(30, y, 400, 16, muted, "内嵌歌词 · 上下滑动翻阅", LEFT);
    unsigned cue = plain_cue, offset = plain_byte;
    int size = font(24), step = aster_text_line_h(size) + sy(8);
    for (int py = sy(y + 30); cue < lyrics.count && py + step <= sy(LYRIC_PROGRESS - 24);
         py += step) {
        const char *body = lyrics.text + lyrics.cue[cue].text_offset + offset;
        uint32_t shown;
        unsigned n = plain_row(cue, offset, &shown);
        if (!n)
            break;
        char row[768];
        memcpy(row, body, shown);
        row[shown] = 0;
        text_run(30, py, 400, size, ink, row, LEFT);
        offset += n;
        if (!body[n]) {
            cue++;
            offset = 0;
        }
    }
}

static void lyric_page(uint32_t now)
{
    char name[256];
    const char *by = artist();
    text(30, 18, 400, 24, ink, bluetooth_input() ? "蓝牙实时歌词" : title(name), LEFT);
    text(30, 49, 400, 16, muted, by, LEFT);
    if (!lyrics.count) {
        const char *heading = bluetooth_input() ? "等待手机发送歌词"
                              : lyric_file      ? "正在读取歌词"
                              : lyric_error     ? "无法读取歌词文件"
                                                : "暂无同步歌词";
        text(30, 207, 400, 24, ink, heading, LEFT);
        if (!bluetooth_input() && !lyric_file)
            text(30, 249, 400, 20, muted, "将同名 LRC 放在歌曲旁边", LEFT);
    } else if (plain_lyrics) {
        plain_view(*by ? 84 : 65);
    } else {
        lyric_scroll_view(*by ? 84 : 65, now);
    }
    if (bluetooth_input() && !player.duration_ms && !has_position())
        text(30, LYRIC_PROGRESS, 400, 16, muted, "手机音乐设置 · 外接设备蓝牙歌词", LEFT);
    else
        progress(LYRIC_PROGRESS, true);
}

static unsigned paper_layout(int cue)
{
    if (bluetooth_input())
        return (unsigned)motion_effect * LYRIC_VARIANTS + live_scene % LYRIC_VARIANTS;
    unsigned scene = 0;
    for (int i = 0; i < cue;) {
        int end = next_lyric_group(i);
        for (; i < end; i++) {
            if (lyrics.text[lyrics.cue[i].text_offset]) {
                scene++;
                break;
            }
        }
        i = end;
    }
    return (unsigned)motion_effect * LYRIC_VARIANTS + scene % LYRIC_VARIANTS;
}

static void paper_tick(uint32_t now)
{
    if (page != PAPER)
        return;
    if (motion_transition) {
        dirty = true;
        if (motion_leaving && now - motion_began >= LYRIC_EXIT_MS) {
            set_page(motion_origin);
            return;
        }
        if (!motion_leaving && now - motion_began >= LYRIC_ENTER_MS)
            motion_transition = false;
    }
    int next = lyric_line;
    while (next >= 0 && !lyrics.text[lyrics.cue[next].text_offset]) {
        int following = next + 1;
        if (following >= (int)lyrics.count ||
            lyrics.cue[following].time_ms != lyrics.cue[next].time_ms) {
            next = -1;
            break;
        }
        next = following;
    }
    if (next != paper_line || (bluetooth_input() && rendered_live_scene != live_scene)) {
        rendered_live_scene = live_scene;
        paper_previous = paper_style;
        if (next >= 0)
            paper_style = paper_layout(lyric_line);
        if (paper_line < 0)
            paper_previous = paper_style;
        paper_prepare(&lyrics, next, player.duration_ms, paper_style);
        if (motion_effect == CAMERA_PUSH) {
            camera_new_frame = next >= 0;
            camera_prepare_ms = aster_time_now_ms() - now;
        }
        paper_began = now;
        paper_line = next;
        dirty = true;
    }
    uint32_t position_ms = motion_effect == CAMERA_PUSH
                               ? lyric_position(aster_time_now_ms(), camera_draw_ms)
                               : lyric_position(now, 0);
    if (paper_highlight(&lyrics, next, bluetooth_input() || plain_lyrics ? UINT32_MAX : position_ms) ||
        now - paper_began <= 650)
        dirty = true;
}

static uint32_t camera_age(uint32_t now)
{
    if (player.mode != ASTER_PLAYER_LOCAL || plain_lyrics || paper_line < 0)
        return now - paper_began;
    /* A late frame or a seek should show the camera where the lyric already
     * is in the song, including the file's own LRC offset. */
    int64_t age = (int64_t)lyric_position(now, camera_draw_ms) + lyrics.offset_ms -
                  lyrics.cue[paper_line].time_ms;
    return age <= 0 ? 0 : age >= CAMERA_MOVE_MS ? CAMERA_MOVE_MS : (uint32_t)age;
}

static int draw_paper(uint32_t now)
{
    uint32_t age = now - paper_began;
    if (motion_effect == CAMERA_PUSH) {
        now = aster_time_now_ms();
        age = camera_age(now);
    }
    const char *body = paper_line < 0 ? NULL : lyrics.text + lyrics.cue[paper_line].text_offset;
    const char *status = lyrics.count        ? NULL
                         : bluetooth_input() ? "等待手机发送歌词"
                         : lyric_file        ? "正在读取歌词"
                         : lyric_error       ? "无法读取歌词文件"
                                             : "暂无同步歌词";
    lyric_transition_t transition = {motion_transition ? now - motion_began : UINT32_MAX,
                                     motion_leaving};
    int result = paper_draw(&gfx, left, top, content_w, paper_style, paper_previous, age, body,
                            status, transition);
    if (motion_effect == CAMERA_PUSH && result >= 0 && body && *body) {
        uint32_t cost = aster_time_now_ms() - now;
        if (camera_new_frame) {
            uint32_t cut = cost + camera_prepare_ms;
            if (cut > LYRIC_MAX_LEAD_MS)
                cut = LYRIC_MAX_LEAD_MS;
            camera_cut_ms = (3 * camera_cut_ms + cut) / 4;
            camera_new_frame = false;
        }
        if (cost > LYRIC_MAX_LEAD_MS)
            cost = LYRIC_MAX_LEAD_MS;
        camera_draw_ms = (camera_draw_ms + cost) / 2;
    }
    return result;
}

static int current_index(void)
{
    if (bluetooth_input())
        return -1;
    const char *path = sandbox_track(player.path[0] ? player.path : current_file);
    if (strncmp(path, "/music/", 7))
        return -1;
    for (int i = 0; i < track_count; i++)
        if (!strcmp(path + 7, tracks[i]))
            return i;
    return -1;
}

static int page_count(void)
{
    return track_count ? (track_count + ROWS - 1) / ROWS : 1;
}

static void library(void)
{
    char temp[256];
    if (!library_scanned)
        aster_snprintf(temp, sizeof(temp), "正在读取 SD / music");
    else
        aster_snprintf(temp, sizeof(temp), "SD / music · %d 首", track_count);
    text(25, 71, 252, 18, muted, temp, LEFT);
    box(305, 59, 130, 44, 10, finger.control == DEMO ? rim : panel, false);
    text(305, 68, 130, 20, gold, "试听示例", CENTER);
    int active = current_index();
    for (int row = 0; row < ROWS; row++) {
        int index = library_page * ROWS + row, y = ROW_TOP + row * ROW_H;
        if (index >= track_count)
            break;
        if (active == index || finger.control == TRACK_FIRST + row)
            box(19, y + 2, 422, ROW_H - 5, 10, selected, false);
        else
            line(22, y + ROW_H - 2, 438, y + ROW_H - 2, rail);
        aster_snprintf(temp, sizeof(temp), "%02d", index + 1);
        text(33, y + 18, 45, 18, gold, temp, LEFT);
        display_name(tracks[index], temp);
        text(77, y + 13, 260, 24, ink, temp, LEFT);
        text(351, y + 19, 69, 18, muted, format(tracks[index]), RIGHT);
    }
    if (!track_count && library_scanned) {
        text(26, 200, 408, 19, ink,
             scan_error < 0 && scan_error != ASTER_ENOENT ? "无法打开存储卡曲库" : "曲库暂无音乐",
             CENTER);
        text(26, 236, 408, 16, muted, "将 MP3、FLAC 或 WAV 放入 music 目录", CENTER);
    }
    uint16_t prev = library_page > 0 ? gold : border;
    uint16_t next = library_page + 1 < page_count() ? gold : border;
    box(18, LIBRARY_PAGER + 6, 126, 47, 10, chrome, false);
    box(316, LIBRARY_PAGER + 6, 126, 47, 10, chrome, false);
    text(18, LIBRARY_PAGER + 17, 126, 20, prev, "上一页", CENTER);
    text(316, LIBRARY_PAGER + 17, 126, 20, next, "下一页", CENTER);
    aster_snprintf(temp, sizeof(temp), "%d / %d", library_page + 1, page_count());
    text(158, LIBRARY_PAGER + 17, 144, 20, ink, temp, CENTER);
}

static void draw(uint32_t now)
{
    if (page == PAPER) {
        int result = draw_paper(now);
        if (result < 0)
            aster_errfmt("HiFi paper render %d", result);
        return;
    }
    switch (page) {
    case LYRICS:
        frame_chrome();
        lyric_page(now);
        break;
    case PLAYBACK:
        playback();
        frame_chrome();
        break;
    case LIBRARY:
        frame_chrome();
        library();
        break;
    case PAPER:
        break;
    }
    if (message[0]) {
        box(32, 271, 396, 47, 11, panel, false);
        box(32, 271, 396, 47, 11, border, true);
        text(44, 283, 372, 18, ink, message, CENTER);
    }
    int result = aster_ge_present(&gfx);
    if (result < 0) {
        aster_errfmt("HiFi render %d", result);
    } else if (page == LYRICS && lyrics.count && !plain_lyrics && local_playing()) {
        /* Estimate the time to lay out and present the next highlighted line.
         * Each frame is anchored to the media snapshot, even late in a song. */
        uint32_t cost = aster_time_now_ms() - now;
        if (cost > LYRIC_MAX_LEAD_MS)
            cost = LYRIC_MAX_LEAD_MS;
        lyric_page_draw_ms = (lyric_page_draw_ms + cost) / 2;
    }
}

static int hit(int x, int y)
{
    if (page == PAPER)
        return NONE;
    if (page != LYRICS && y >= 4 && y < HEADER_END && x >= 378 && x < DESIGN)
        return CLOSE;
    if (page == LIBRARY) {
        if (y >= HEADER_END && y < ROW_TOP && x >= 288 && x < DESIGN)
            return DEMO;
        if (y >= ROW_TOP && y < LIBRARY_PAGER && x >= 8 && x < DESIGN - 8)
            return TRACK_FIRST + (y - ROW_TOP) / ROW_H;
        if (y >= LIBRARY_PAGER && y < CONTENT_END && x >= 0 && x < 150)
            return PAGE_PREV;
        if (y >= LIBRARY_PAGER && y < CONTENT_END && x >= 310 && x < DESIGN)
            return PAGE_NEXT;
        return NONE;
    }
    if (page == LYRICS && can_seek() && y >= LYRIC_SEEK_TOP && y < CONTENT_END)
        return SEEK;
    if (page == PLAYBACK) {
        if (x >= 19 && x < PANEL_X - 7) {
            if (y >= VOLUME_Y - 20 && y < VOLUME_Y + 20)
                return VOLUME;
            if (can_seek() && y >= PLAY_PROGRESS - 25 && y < PLAY_PROGRESS + 25)
                return SEEK;
        }
        if (x >= STEP_X && x < STEP_X + STEP_W) {
            if (y >= STEP_UP && y < STEP_UP + STEP_H)
                return PREVIOUS;
            if (y >= STEP_DOWN && y < STEP_DOWN + STEP_H)
                return NEXT;
        }
        if (x >= TOGGLE_X && x < TOGGLE_X + TOGGLE_W && y >= STEP_UP && y < STEP_DOWN + STEP_H)
            return TOGGLE;
    }
    return NONE;
}

static bool page_swipe_area(int y)
{
    if (page == PAPER)
        return y >= 24 && y < DESIGN - 24;
    if (page == PLAYBACK)
        return y >= 24 && y < PANEL_Y - 5;
    return y >= HEADER_END && y < (page == LYRICS ? LYRIC_SEEK_TOP : LIBRARY_PAGER);
}

static uint32_t output(void)
{
    if (player.capabilities & ASTER_PLAYER_CAP_SYSTEM_OUTPUT)
        return ASTER_PLAYER_OUTPUT_SYSTEM;
    aster_player_bt_list_t list = {.size = sizeof(list), .version = 1};
    if (!aster_player_bt_list(&list, sizeof(list)) && list.state == ASTER_PLAYER_BT_CONNECTED &&
        list.output_rate)
        return ASTER_PLAYER_OUTPUT_BLUETOOTH;
    return ASTER_PLAYER_OUTPUT_WIRED;
}

static bool media_in_use(void)
{
    return player.state == ASTER_PLAYER_PLAYING || player.state == ASTER_PLAYER_PAUSED ||
           player.state == ASTER_PLAYER_OPENING || player.state == ASTER_PLAYER_STOPPING;
}

/* ENDED is sticky until the next command. Only consume it for the local
 * library track we actually saw running, or successfully started ourselves. */
static int automatic_next(void)
{
    const char *path = sandbox_track(player.path);
    if (player.mode != ASTER_PLAYER_LOCAL || queued_file[0] || strncmp(path, "/music/", 7)) {
        playing_track[0] = 0;
        return -1;
    }
    if (player.state != ASTER_PLAYER_ENDED) {
        if (player.state == ASTER_PLAYER_OPENING || player.state == ASTER_PLAYER_PLAYING ||
            player.state == ASTER_PLAYER_PAUSED) {
            if (strcmp(playing_track, path))
                aster_snprintf(playing_track, sizeof(playing_track), "%s", path);
        } else {
            playing_track[0] = 0;
        }
        return -1;
    }
    if (!library_scanned || seeking != SEEK_IDLE || !playing_track[0] || strcmp(playing_track, path))
        return -1;
    playing_track[0] = 0;
    int index = current_index();
    return index >= 0 && track_count > 1 && scan_error >= 0 ? (index + 1) % track_count : -1;
}

static void start_queued(uint32_t now)
{
    int result = player_command(ASTER_PLAYER_PLAY_FILE, output(), queued_file);
    /* The old audio worker may briefly be releasing the output after EOF. */
    if (result == ASTER_EBUSY) {
        if (!start_busy) {
            start_retry_until = now + 1500;
            start_busy = true;
        }
        if ((int32_t)(now - start_retry_until) < 0)
            return;
    }
    if (result >= 0) {
        aster_snprintf(current_file, sizeof(current_file), "%s", queued_file);
        aster_snprintf(playing_track, sizeof(playing_track), "%s", queued_file);
    } else {
        report_control(result);
    }
    queued_file[0] = 0;
    start_busy = false;
}

static void queue_track(const char *path)
{
    if (bluetooth_input()) {
        notice("请先在系统蓝牙中断开手机");
        return;
    }
    seeking = SEEK_IDLE;
    playing_track[0] = 0;
    start_busy = false;
    aster_snprintf(queued_file, sizeof(queued_file), "%s", path);
    if (media_in_use() && player.state != ASTER_PLAYER_STOPPING &&
        control(ASTER_PLAYER_STOP, 0, NULL) < 0)
        queued_file[0] = 0;
    set_page(PLAYBACK);
    dirty = true;
}

static void play_index(int index)
{
    if (index < 0 || index >= track_count)
        return;
    char path[256];
    aster_snprintf(path, sizeof(path), "/music/%s", tracks[index]);
    queue_track(path);
}

static void change_library_page(int step)
{
    library_page = clamp(library_page + step, 0, page_count() - 1);
    dirty = true;
}

static void activate(int id)
{
    switch (id) {
    case CLOSE:
        aster_app_exit(0);
        break;
    case DEMO:
        queue_track("/assets/audio/demo.flac");
        break;
    case PAGE_PREV:
        change_library_page(-1);
        break;
    case PAGE_NEXT:
        change_library_page(1);
        break;
    case TOGGLE:
        if (!bluetooth_input() && !media_in_use()) {
            const char *track = sandbox_track(player.path);
            if (*track)
                queue_track(track);
            else if (current_file[0])
                queue_track(current_file);
            else
                set_page(LIBRARY);
        } else if (player.state == ASTER_PLAYER_PLAYING)
            control(ASTER_PLAYER_PAUSE, 0, NULL);
        else
            control(ASTER_PLAYER_RESUME, 0, NULL);
        break;
    case PREVIOUS:
    case NEXT:
        if (bluetooth_input()) {
            control(id == PREVIOUS ? ASTER_PLAYER_PREVIOUS : ASTER_PLAYER_NEXT, 0, NULL);
        } else if (track_count) {
            int index = current_index();
            play_index(index < 0 ? 0 : (index + (id == NEXT ? 1 : track_count - 1)) % track_count);
        } else
            set_page(LIBRARY);
        break;
    default:
        if (id >= TRACK_FIRST && id < TRACK_FIRST + ROWS)
            play_index(library_page * ROWS + id - TRACK_FIRST);
        break;
    }
    dirty = true;
}

static void slider(int id, int x)
{
    if (id == VOLUME) {
        int next = clamp((x - VOLUME_X) * 100 / VOLUME_W, 0, 100);
        if (volume != next && aster_audio_set_volume(next) >= 0)
            volume = next;
    } else if (id == SEEK && can_seek()) {
        int start = page == PLAYBACK ? PLAY_SEEK_X : PROGRESS_X;
        int span = page == PLAYBACK ? PLAY_SEEK_W : PROGRESS_W;
        seek_position = (uint32_t)((uint64_t)player.duration_ms * clamp(x - start, 0, span) / span);
        seeking = SEEK_DRAG;
    }
    dirty = true;
}

static void touch(const aster_event_t *event)
{
    int x = ((int)event->u.touch.x - left) * DESIGN / (int)content_w;
    int y = ((int)event->u.touch.y - top) * DESIGN / (int)content_h;
    uint32_t action = event->u.touch.action;
    uint32_t at = (uint32_t)event->timestamp_ms;
    if (action == ASTER_TOUCH_DOWN) {
        finger.down = true;
        finger.moved = finger.rejected = false;
        finger.x = x;
        finger.y = y;
        finger.control = hit(x, y);
        finger.page_swipe =
            page_swipe_area(y) && finger.control != VOLUME && finger.control != DEMO;
        finger.pressed = at;
        finger.tap_candidate = !has_accelerometer && (page == LYRICS || page == PAPER) &&
                               !motion_leaving && finger.control == NONE && finger.page_swipe;
        if (!finger.tap_candidate)
            lyric_tap.armed = false;
        if (finger.control == SEEK)
            slider(SEEK, x);
        dirty = true;
        return;
    }
    if (!finger.down)
        return;
    int dx = x - finger.x, dy = y - finger.y;
    int ax = abs_i(dx), ay = abs_i(dy);
    int id = finger.control;
    bool is_slider = id == VOLUME || id == SEEK;
    if (!page_swipe_area(y))
        finger.page_swipe = false;
    if (ax > 12 || ay > 12) {
        if (!finger.moved && is_slider && ay > ax * 2) {
            finger.rejected = true;
            if (id == SEEK && seeking == SEEK_DRAG)
                seeking = SEEK_IDLE;
            dirty = true;
        }
        finger.moved = true;
        lyric_tap.armed = false;
    }
    if (action == ASTER_TOUCH_MOVE) {
        if (is_slider && !finger.rejected && finger.moved)
            slider(id, x);
        return;
    }
    if (action != ASTER_TOUCH_UP)
        return;
    finger.down = false;
    finger.control = NONE;
    if (finger.tap_candidate && finger.page_swipe && !finger.moved && at - finger.pressed <= 260) {
        if (lyric_tap.armed && finger.pressed - lyric_tap.released <= 350 &&
            abs_i(x - lyric_tap.x) <= 24 && abs_i(y - lyric_tap.y) <= 24) {
            lyric_tap.armed = false;
            toggle_motion(aster_time_now_ms(), at);
        } else {
            lyric_tap.armed = true;
            lyric_tap.x = x;
            lyric_tap.y = y;
            lyric_tap.released = at;
        }
        dirty = true;
        return;
    }
    lyric_tap.armed = false;
    if (is_slider && !finger.rejected) {
        slider(id, x);
        if (id == SEEK && seeking == SEEK_DRAG) {
            if (control(ASTER_PLAYER_SEEK, seek_position, NULL) >= 0) {
                seeking = SEEK_WAIT;
                seek_deadline = aster_time_now_ms() + 2000;
            } else
                seeking = SEEK_IDLE;
        }
    } else if (!is_slider && finger.page_swipe && ax >= 46 && ax * 2 > ay * 3) {
        if (page == PAPER) {
            if (dx < 0)
                set_page(PLAYBACK);
        } else
            set_page((enum page)clamp((int)page + (dx < 0 ? 1 : -1), LYRICS, LIBRARY));
    } else if (page == LIBRARY && ay >= 42 && ay * 2 > ax * 3 && finger.y >= 110) {
        change_library_page(dy < 0 ? 1 : -1);
    } else if (plain_lyrics && (page == LYRICS || page == PAPER) && !is_slider &&
               ay >= 28 && ay * 2 > ax * 3) {
        if (page == PAPER) {
            plain_cue = clamp((int)plain_cue + (dy < 0 ? 1 : -1), 0, (int)lyrics.count - 1);
            plain_byte = 0;
        } else {
            scroll_plain((dy < 0 ? 1 : -1) * clamp((ay + 18) / 36, 1, 12));
        }
    } else if (!finger.moved && id != NONE && hit(x, y) == id)
        activate(id);
    dirty = true;
}

static void poll(uint32_t now)
{
    if (now - last_poll < (page == PAPER ? 50u : 150u))
        return;
    last_poll = now;
    aster_player_info_t next = {.size = sizeof(next), .version = 1};
    if (!aster_player_get_info(&next, sizeof(next))) {
        player_sample_at = now;
        player.position_ms = next.position_ms;
        memcpy(player.reserved, next.reserved, sizeof(player.reserved));
        if (memcmp(&next, &player, sizeof(next)))
            dirty = true;
        if (player.mode != next.mode) {
            queued_file[0] = 0;
            start_busy = false;
            seeking = SEEK_IDLE;
            lyric_tap.armed = false;
            finger.down = false;
            finger.control = NONE;
        }
        player = next;
        if (seeking == SEEK_WAIT) {
            uint32_t delta = player.position_ms > seek_position
                                 ? player.position_ms - seek_position
                                 : seek_position - player.position_ms;
            if (delta <= 450 || (int32_t)(now - seek_deadline) >= 0 ||
                player.state == ASTER_PLAYER_ERROR) {
                seeking = SEEK_IDLE;
                dirty = true;
            }
        }
        int index = automatic_next();
        if (index >= 0)
            aster_snprintf(queued_file, sizeof(queued_file), "/music/%s", tracks[index]);
        if (queued_file[0] && player.mode == ASTER_PLAYER_LOCAL && !media_in_use())
            start_queued(now);
    }
    sync_media();
    int next_volume = aster_audio_get_volume();
    if (next_volume >= 0 && next_volume != volume) {
        volume = clamp(next_volume, 0, 100);
        dirty = true;
    }
}

ASTER_APP_EXPORT int aster_app_init(void)
{
    aster_display_info_t display = {.size = sizeof(display), .version = 1};
    int result = aster_display_get_info(0, &display, sizeof(display));
    if (result < 0)
        return result;
    width = display.app_width;
    height = display.app_height;
    if (width < 240 || height < 240)
        return ASTER_ENOTSUP;
    content_w = content_h = width < height ? width : height;
    left = (width - content_w) / 2;
    top = (height - content_h) / 2;
    player = (aster_player_info_t){.size = sizeof(player), .version = 1};
    result = aster_player_get_info(&player, sizeof(player));
    if (result < 0)
        return result;
    player_sample_at = aster_time_now_ms();
    int initial_volume = aster_audio_get_volume();
    volume = initial_volume < 0 ? 40 : clamp(initial_volume, 0, 100);
    aster_surface_desc_t desc = {.size = sizeof(desc),
                                 .version = 1,
                                 .width = width,
                                 .height = height,
                                 .format = ASTER_PIXEL_RGB565};
    result = aster_surface_create(&desc, sizeof(desc), &surface);
    if (result < 0)
        return result;
    aster_ge_init(&gfx, surface, width, height, commands, 160, arena, sizeof(arena));
    scan_start();
    automatic_next();
    sync_media();
    has_accelerometer = aster_system_has_feature(ASTER_FEATURE_ACCELEROMETER) > 0;
    imu_retry_at = aster_time_now_ms();
    aster_app_set_framerate(30);
    dirty = true;
    aster_printf("HiFi init %ux%u caps=%u", width, height, player.capabilities);
    return 0;
}

ASTER_APP_EXPORT void aster_app_event(const aster_event_t *event)
{
    if (!event || !surface)
        return;
    if (event->type == ASTER_EVENT_FRAME) {
        uint32_t now = aster_time_now_ms();
        poll(now);
        if (has_accelerometer)
            shake_poll(now);
        scan_tick();
        lyrics_tick(now);
        paper_tick(now);
        if (cover_tick(sx(208)) && page == PLAYBACK)
            dirty = true;
        uint32_t second = position() / 1000;
        if (visible_second != second) {
            visible_second = second;
            if (page != LIBRARY && page != PAPER)
                dirty = true;
        }
        if (message[0] && (int32_t)(now - message_until) >= 0) {
            message[0] = 0;
            dirty = true;
        }
        if (aster_gfx_fonts_pending())
            dirty = true;
        if (dirty) {
            draw(now);
            dirty = false;
        }
    } else if (event->type == ASTER_EVENT_TOUCH) {
        if (event->u.touch.slot == 0)
            touch(event);
        else if (event->u.touch.action == ASTER_TOUCH_DOWN) {
            lyric_tap.armed = false;
            finger.tap_candidate = false;
        }
    } else if (event->type == ASTER_EVENT_KEY && event->u.key.pressed && event->u.key.code == 1)
        aster_app_exit(0);
}

ASTER_APP_EXPORT void aster_app_pause(void)
{
    lyric_tap.armed = false;
    memset(&finger, 0, sizeof(finger));
    memset(&shake, 0, sizeof(shake));
    seeking = SEEK_IDLE;
}

ASTER_APP_EXPORT void aster_app_resume(void)
{
    aster_app_pause();
    paper_invalidate();
    last_poll = 0;
    dirty = true;
}

static void cleanup(void)
{
    scan_close();
    lyrics_close();
    cover_close();
    if (surface) {
        aster_surface_destroy(surface);
        surface = 0;
    }
}

ASTER_APP_EXPORT void aster_app_stop(void)
{
    cleanup();
}

ASTER_APP_EXPORT void aster_app_destroy(void)
{
    cleanup();
}
