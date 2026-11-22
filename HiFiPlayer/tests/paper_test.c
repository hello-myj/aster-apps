/* SPDX-License-Identifier: MIT */
/* Run from HiFiPlayer so the test reads the same packed outlines as the APP. */
#include <aster/aster.h>
#include "../src/paper.h"
#include "../src/paper_font.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#ifndef TEST_SCREEN_SIZE
#define TEST_SCREEN_SIZE 460
#endif
enum { SIDE = TEST_SCREEN_SIZE };
static FILE *asset;
static uint16_t frame[SIDE * SIDE], expected[SIDE * SIDE];
static unsigned partial_frames, system_text_frames;
static lrc_t track;

int32_t aster_fs_open(const char *path, uint32_t length, uint32_t flags, aster_handle_t *out)
{
    assert(!asset && !strncmp(path, "/assets/", 8));
    assert(length == strlen(path) && flags == ASTER_FS_O_RDONLY);
    asset = fopen(path + 8, "rb");
    *out = 1;
    return asset ? 0 : -1;
}

int32_t aster_fs_seek(aster_handle_t handle, int32_t offset, uint32_t whence)
{
    assert(handle == 1 && whence == ASTER_FS_SEEK_SET);
    return fseek(asset, offset, SEEK_SET);
}

int32_t aster_fs_read(aster_handle_t handle, void *dst, uint32_t bytes)
{
    assert(handle == 1);
    return (int32_t)fread(dst, 1, bytes, asset);
}

int32_t aster_fs_close(aster_handle_t handle)
{
    assert(handle == 1);
    int result = fclose(asset);
    asset = NULL;
    return result;
}

int32_t aster_surface_acquire(aster_handle_t handle, aster_surface_buffer_t *buf, uint32_t size)
{
    assert(size == sizeof(*buf));
    buf->ptr = frame;
    buf->width = buf->height = SIDE;
    buf->pitch = SIDE * 2;
    buf->format = ASTER_PIXEL_RGB565;
    return 0;
}

int32_t aster_surface_present(aster_handle_t handle) { return 0; }

int32_t aster_surface_flush(aster_handle_t handle, const void *pixels, uint32_t x, uint32_t y,
                            uint32_t width, uint32_t height, uint32_t pitch)
{
    assert(x + width <= SIDE && y + height <= SIDE && pitch == SIDE);
    assert(pixels == frame + y * pitch + x);
    partial_frames++;
    return 0;
}

int32_t aster_gfx_exec(aster_handle_t handle, const aster_gfx_cmd_t *cmds, uint32_t count,
                       const uint8_t *arena, uint32_t bytes, uint32_t x, uint32_t y, uint32_t width,
                       uint32_t height)
{
    system_text_frames++;
    return count;
}

int32_t aster_gfx_text_w(uint32_t px, const char *text, uint32_t length) { return px * length; }

/* Inspect the actual layout and raster metrics with the shipped font. */
static void check_words(const char *text, unsigned scene)
{
    lyric_glyph_t glyphs[LYRIC_LINE_CAP] = {0};
    unsigned n = 0;
    for (const char *p = text; *p;) {
        unsigned bytes = aster_utf8_len(p);
        glyphs[n++].codepoint = aster_utf8_cp(p, bytes);
        p += bytes;
    }
    assert(paper_font_line(glyphs, n, scene >= LYRIC_VARIANTS || lyric_layout_vertical(scene)));
    lyric_layout_words(scene, glyphs, n);
    for (unsigned i = 0; i < n; i++) {
        const lyric_glyph_t *g = &glyphs[i];
        assert(g->word_end > i && g->word_end <= n);
        if (g->codepoint == ' ')
            continue;
        assert(g->pose.x >= 10 && g->pose.x <= SIDE - 10);
        assert(g->pose.y >= g->pose.em / 2 && g->pose.y <= SIDE - g->pose.em / 2);
        if (i && lyric_latin(g->codepoint) &&
            (lyric_latin(glyphs[i - 1].codepoint) || glyphs[i - 1].codepoint == 0x2019 ||
             glyphs[i - 1].codepoint == '\'' || glyphs[i - 1].codepoint == '-')) {
            assert(g->row == glyphs[i - 1].row && g->word_end == glyphs[i - 1].word_end);
            const lyric_glyph_t *before = &glyphs[i - 1];
            int ink_x = g->pose.x - g->center_offset * g->pose.em / PAPER_FONT_EM;
            int previous_x =
                before->pose.x - before->center_offset * before->pose.em / PAPER_FONT_EM;
            assert(ink_x > previous_x);
        }
    }
}

int main(void)
{
    const char *lrc = "[00:00.00]<00:00.10>沿着<00:00.90>夜色<00:01.80>慢慢走<00:03.00>\n"
                      "[00:03.15]听见天亮的回声\n[00:06.25]我们就快到家了";
    lrc_parse(&track, (const unsigned char *)lrc, strlen(lrc), NULL);
    assert(track.count == 3);
    aster_gfx_cmd_t commands[160];
    uint8_t arena[4096];
    aster_ge_t gfx;
    aster_ge_init(&gfx, 1, SIDE, SIDE, commands, 160, arena, sizeof(arena));
    for (unsigned scene = 0; scene < LYRIC_EFFECT_COUNT * LYRIC_VARIANTS; scene++) {
        unsigned i = scene % track.count;
        unsigned previous = scene % LYRIC_VARIANTS ? scene - 1 : scene;
        const char *text = track.text + track.cue[i].text_offset;
        paper_prepare(&track, (int)i, 9500, scene);
        for (unsigned local = 0; local < 3100; local += 60) {
            uint32_t position = track.cue[i].time_ms + local;
            paper_highlight(&track, (int)i, position);
            assert(!paper_draw(
                &gfx, 0, 0, SIDE, scene, previous, local, text, NULL,
                (lyric_transition_t){scene % LYRIC_VARIANTS ? UINT32_MAX : local, false}));
            memcpy(expected, frame, sizeof(frame));
            paper_invalidate();
            assert(!paper_draw(
                &gfx, 0, 0, SIDE, scene, previous, local, text, NULL,
                (lyric_transition_t){scene % LYRIC_VARIANTS ? UINT32_MAX : local, false}));
            assert(!memcmp(expected, frame, sizeof(frame)));
            assert(!paper_highlight(&track, (int)i, position)); /* paused playhead */
        }
    }
    assert(!asset && !system_text_frames && partial_frames > 50);

    lrc = "[00:00.00]<00:00.00>When the <00:01.00>city lights <00:03.00>go quiet\n"
          "[00:05.00]We’ll follow the street-lights all the way home until the sky turns blue\n"
          "[00:10.00]陪我 stay a little longer 再走 don't leave yet\n"
          "[00:15.00]abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ\n"
          "[00:20.00]音乐♫𠮷";
    lrc_parse(&track, (const unsigned char *)lrc, strlen(lrc), NULL);
    assert(track.count == 5);
    for (unsigned scene = 0; scene < LYRIC_EFFECT_COUNT * LYRIC_VARIANTS; scene++) {
        for (unsigned i = 0; i < track.count - 1; i++) {
            unsigned start = track.cue[i].time_ms;
            const char *text = track.text + track.cue[i].text_offset;
            check_words(text, scene);
            paper_prepare(&track, (int)i, 25000, scene);
            unsigned clocks[] = {0, 1380, 2750, 1100, UINT32_MAX};
            for (unsigned k = 0; k < sizeof(clocks) / sizeof(clocks[0]); k++) {
                paper_highlight(&track, (int)i,
                                clocks[k] == UINT32_MAX ? UINT32_MAX : start + clocks[k]);
                lyric_transition_t steady = {UINT32_MAX, false};
                assert(paper_draw(&gfx, 0, 0, SIDE, scene, scene, 650, text, NULL, steady) == 0);
                memcpy(expected, frame, sizeof(frame));
                paper_invalidate();
                assert(paper_draw(&gfx, 0, 0, SIDE, scene, scene, 650, text, NULL, steady) == 0);
                assert(!memcmp(frame, expected, sizeof(frame)));
            }
        }
    }
    assert(system_text_frames == 0);
    paper_prepare(&track, 4, 25000, 0);
    paper_highlight(&track, 4, 22000);
    assert(paper_draw(&gfx, 0, 0, SIDE, 0, 0, 650, track.text + track.cue[4].text_offset, NULL,
                      (lyric_transition_t){UINT32_MAX, false}) >= 0);
    assert(system_text_frames == 1);
    return 0;
}
