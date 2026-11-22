/* SPDX-License-Identifier: MIT */
#include <aster_imports.h>
#include "paper.h"
#include "paper_font.h"
#include "lyric_layout.h"

enum { DESIGN = 460, CHINESE_LAYOUT_LIMIT = 36, FP = 1024 };

typedef struct {
    uint32_t base, panel, pending, ink;
} palette_t;

static const palette_t palettes[] = {
    {0xf1e1ca, 0xae4250, 0x885758, 0x691e33}, {0x24203f, 0x514182, 0xaa9ac4, 0xffe5b2},
    {0xefe5d0, 0x7c99bd, 0x556a8c, 0x21365d}, {0xf4d8b5, 0xc46349, 0x955e48, 0x562631},
    {0x233e41, 0x4f8175, 0xb3c4aa, 0xffedb6}, {0xdbe7da, 0x889b84, 0x637a72, 0x223e45},
    {0x112947, 0x2e599c, 0x88aadc, 0xc8f8ef}, {0x172d40, 0x375262, 0x85afc4, 0xc7f7ea},
    {0x254284, 0x456ac6, 0xaac2f1, 0xf2fbda}, {0x362648, 0x75548a, 0xb59abb, 0xffe4a9},
    {0x143b3c, 0x376963, 0x8cb7ad, 0xe3ffdc}, {0x333052, 0x655da4, 0xb4add3, 0xffedcc},
    {0x161733, 0x594479, 0x9d8ebb, 0xebdbff}, {0x112931, 0x336b64, 0x91b5aa, 0xdbf9c6},
    {0x252238, 0x614867, 0xb9a1b5, 0xffe3eb}, {0x291c30, 0x824956, 0xc69799, 0xffead3},
    {0x0f2939, 0x365c83, 0x94b4ce, 0xe1fbff}, {0x292434, 0x74624b, 0xbca990, 0xfff3cc},
};

typedef struct {
    int x, y;
} point_t;

static const point_t pink_sheet[] = {{0, 250},   {349, 63},  {460, 229},
                                     {460, 348}, {227, 460}, {0, 460}};
static const point_t pink_band[] = {{0, 376}, {460, 91}, {460, 122}, {0, 423}};
static const point_t pink_corner[] = {{355, 0}, {460, 0}, {460, 164}, {415, 125}};
static const point_t violet_sheet[] = {{83, 0}, {407, 0}, {350, 460}, {9, 460}};
static const point_t violet_bottom[] = {{0, 362}, {276, 362}, {238, 460}, {0, 460}};
static const point_t violet_side[] = {{432, 98}, {460, 86}, {460, 460}, {391, 460}};
static const point_t blue_sheet[] = {{0, 296}, {460, 68}, {460, 277}, {75, 460}, {0, 460}};
static const point_t blue_corner[] = {{0, 0}, {122, 0}, {0, 134}};

/* Positions from the 460×460 motion study for four characters followed by
 * three. Other lengths retain the same reading direction and hierarchy. */
static const pose_t reference[][7] = {
    {{70, 145, 77, -5},
     {176, 152, 77, 0},
     {282, 145, 77, -5},
     {388, 152, 77, 0},
     {112, 294, 101, 5},
     {230, 280, 101, 1},
     {348, 266, 101, -3}},
    {{331, 84, 66, 3},
     {328, 170, 66, 0},
     {325, 256, 66, -3},
     {322, 342, 66, -6},
     {154, 119, 84, -8},
     {147, 228, 84, -4},
     {140, 337, 84, 0}},
    {{96, 138, 68, 6},
     {188, 130, 68, 2},
     {280, 122, 68, -2},
     {372, 114, 68, -6},
     {106, 300, 100, -3},
     {219, 288, 100, 3},
     {332, 276, 100, 9}},
};

static lyric_glyph_t glyphs[LYRIC_LINE_CAP];
static unsigned glyph_count;
static bool settled_frame, word_layout;
static uint16_t displayed[LYRIC_LINE_CAP];

void paper_invalidate(void) { settled_frame = false; }
/* Timing covers the full LRC line, including lines painted with the system font. */
static struct letter {
    uint32_t begin, end;
    unsigned byte_offset, progress;
} letters[LYRIC_LINE_CAP];
static unsigned letter_count, sung_bytes;
enum { LIGHT_MAP = 115 };
static struct light_map {
    int scene;
    uint16_t pixels[LIGHT_MAP][LIGHT_MAP];
} stage_lights[2] = {{.scene = -1}, {.scene = -1}};
static unsigned light_slot;

static int min(int a, int b) { return a < b ? a : b; }

static int max(int a, int b) { return a > b ? a : b; }

static float unit(float value) { return value < 0.f ? 0.f : value > 1.f ? 1.f : value; }

static float smooth(float value)
{
    float x = unit(value);
    return x * x * (3.f - 2.f * x);
}

static float sine(float x)
{
    float xx = x * x;
    return x * (1.f - xx * (1.f / 6.f - xx / 120.f));
}

static float cosine(float x)
{
    float xx = x * x;
    return 1.f - xx * (.5f - xx / 24.f);
}

static float radians(float degrees) { return degrees * .0174532925f; }

static uint16_t rgb565(uint32_t rgb)
{
    return ((rgb >> 8) & 0xf800) | ((rgb >> 5) & 0x07e0) | ((rgb >> 3) & 0x001f);
}

static uint16_t mix(uint16_t base, uint16_t front, unsigned alpha)
{
    const uint32_t mask = 0x07e0f81fu;
    uint32_t a = (base | (uint32_t)base << 16) & mask;
    uint32_t b = (front | (uint32_t)front << 16) & mask;
    uint32_t out = (a + ((b - a) * alpha >> 5)) & mask;
    return (uint16_t)(out | out >> 16);
}

/* Mix in RGB888 before quantizing the reference palette to the panel. */
static uint16_t tint(uint32_t a, uint32_t b, unsigned percent)
{
    unsigned r = ((a >> 16) * (100 - percent) + (b >> 16) * percent + 50) / 100;
    unsigned g = (((a >> 8) & 255) * (100 - percent) + ((b >> 8) & 255) * percent + 50) / 100;
    unsigned bl = ((a & 255) * (100 - percent) + (b & 255) * percent + 50) / 100;
    return rgb565(r << 16 | g << 8 | bl);
}

/* Bake the spotlight and the outline of the first sung character at a quarter
 * of design resolution. Crossfades then cost one blend per four pixels. */
static void build_lights(unsigned scene, const uint8_t *mask, int cell)
{
    static const point_t centers[] = {{295, 225}, {179, 253}, {212, 170},
                                      {166, 156}, {285, 275}, {235, 170}};
    const palette_t *p = &palettes[scene];
    point_t center = centers[scene % LYRIC_VARIANTS];
    struct light_map *map = &stage_lights[light_slot];
    map->scene = scene;
    uint16_t halo = tint(p->panel, p->pending, 36);
    uint16_t shades[101];
    float radius_limit[101], x_squared[LIGHT_MAP];
    /* Invert the light curve once per shade. Along a scanline the intensity
     * rises towards the centre and falls again, usually by one shade per cell. */
    for (int i = 0; i <= 100; i++) {
        float radius = (7.f - __builtin_sqrtf(1.f + .48f * i)) / 6.f;
        radius_limit[i] = radius * radius;
        shades[i] = tint(p->base, p->panel, i);
    }
    for (int x = 0; x < LIGHT_MAP; x++) {
        int dx = 4 * x - center.x;
        x_squared[x] = (float)(dx * dx) / (368 * 368);
    }
    for (int y = 0; y < LIGHT_MAP; y++) {
        int dy = 4 * y - center.y, power = 0;
        float y_squared = (float)(dy * dy) / (285 * 285);
        for (int x = 0; x < LIGHT_MAP; x++) {
            float distance_squared = x_squared[x] + y_squared;
            while (power < 100 && distance_squared <= radius_limit[power + 1])
                power++;
            while (power && distance_squared > radius_limit[power])
                power--;
            uint16_t color = shades[power];
            /* The ghost is tilted independently of the foreground lettering. */
            int u = 40 + ((4 * x - 276) * 72 - (4 * y - 222) * 23) / 275;
            int v = 40 + ((4 * x - 276) * 23 + (4 * y - 222) * 72) / 275;
            u = u * cell / PAPER_FONT_CELL;
            v = v * cell / PAPER_FONT_CELL;
            if (mask && u > 0 && u < cell - 1 && v > 0 && v < cell - 1) {
                const uint8_t *a = mask + v * cell + u;
                int edge = max(max(a[-1], a[1]), max(a[-cell], a[cell])) - *a;
                if (edge > 0)
                    color = mix(color, halo, (unsigned)edge / 15);
            }
            map->pixels[y][x] = color;
        }
    }
}

static uint32_t end_of_line(const lrc_t *lyrics, unsigned line, uint32_t duration)
{
    uint32_t start = lyrics->cue[line].time_ms;
    for (unsigned i = line + 1; i < lyrics->count; i++) {
        if (lyrics->cue[i].time_ms > start)
            return lyrics->cue[i].time_ms - start;
    }
    int64_t end = (int64_t)duration + lyrics->offset_ms;
    return end > start ? (uint32_t)(end - start) : letter_count * 380;
}

static void time_glyphs(const lrc_t *lyrics, unsigned line, uint32_t duration)
{
    const lrc_cue_t *cue = &lyrics->cue[line];
    uint32_t window = end_of_line(lyrics, line, duration);
    unsigned first = 0;
    uint32_t start = 0;
    for (unsigned i = 0; i <= cue->word_count && first < letter_count; i++) {
        bool last = i == cue->word_count;
        const lrc_word_t *tag = last ? NULL : &lyrics->words[cue->first_word + i];
        unsigned end = first;
        while (end < letter_count && (last || letters[end].byte_offset < tag->byte_offset))
            end++;
        uint32_t remaining = window > start ? window - start : 0;
        uint32_t estimate = (end - first) * 550;
        uint32_t finish =
            last ? start + (remaining < estimate ? remaining : estimate) : tag->delta_ms;
        if (last && !cue->word_count) {
            start = min(140, finish / 6);
            finish -= min(110, finish / 6);
        }
        uint32_t span = finish > start ? finish - start : 0;
        unsigned total = 0, prefix = 0;
        if (word_layout)
            for (unsigned j = first; j < end; j++)
                total += glyphs[j].codepoint == ' ' ? 0 : glyphs[j].advance;
        bool weighted = total > 0;
        if (!weighted)
            total = end - first;
        for (unsigned j = first; j < end; j++) {
            letters[j].begin = start + (uint64_t)span * prefix / total;
            prefix += weighted ? (glyphs[j].codepoint == ' ' ? 0 : glyphs[j].advance) : 1;
            letters[j].end = start + (uint64_t)span * prefix / total;
            letters[j].progress = 0;
        }
        first = end;
        start = finish;
    }
}

static void place_glyphs(unsigned style)
{
    unsigned count = glyph_count;
    if (style >= 3) {
        for (unsigned i = 0; i < count; i++)
            glyphs[i].pose = lyric_layout_pose(style, count, i);
        return;
    }
    if (count == 7) {
        for (unsigned i = 0; i < count; i++)
            glyphs[i].pose = reference[style][i];
        return;
    }
    unsigned rows = count <= 3 ? 1 : count <= 12 ? 2 : (count + 8) / 9;
    if (style == 1 && count <= 12) {
        unsigned first = (count + 1) / 2;
        for (unsigned i = 0; i < count; i++) {
            bool head = i < first;
            unsigned slot = head ? i : i - first;
            unsigned column = count <= 3 ? count : head ? first : count - first;
            if (count <= 3)
                slot = i;
            glyphs[i].pose = (pose_t){count <= 3 ? 232
                                      : head     ? 329
                                                 : 148,
                                      40 + (int)(380 * (2 * slot + 1) / (2 * column)),
                                      min(head ? 72 : 92, 320 / column),
                                      head ? 3 - (int)slot * 3 : -8 + (int)slot * 4};
        }
        return;
    }
    unsigned at = 0;
    for (unsigned row = 0; row < rows; row++) {
        unsigned slots = (count + rows - 1 - row) / rows;
        int y = rows == 1 ? 230 : rows == 2 ? (row ? 290 : 150) : 64 + 332 * row / (rows - 1);
        int spacing = min(134, 412 / slots);
        int em = min(row == 0 ? 94 : 101, spacing - 11);
        for (unsigned i = 0; i < slots; i++) {
            int x = 230 + (2 * (int)i + 1 - (int)slots) * spacing / 2;
            int slope = style == 2 || row == 1 ? 3 * ((int)slots - 1 - 2 * (int)i) : 0;
            int angle = style == 2 ? 7 - 4 * (int)(i % 4) : i % 2 ? 0 : -5;
            glyphs[at++].pose = (pose_t){x, y + slope, em, angle};
        }
    }
}

static void prepare_letters(const lrc_t *lyrics, int line, uint32_t duration, unsigned style)
{
    paper_invalidate();
    glyph_count = letter_count = sung_bytes = 0;
    word_layout = false;
    if (line < 0)
        return;
    const char *body = lyrics->text + lyrics->cue[line].text_offset;
    unsigned count = 0;
    for (unsigned at = 0; body[at];) {
        unsigned bytes = aster_utf8_len(body + at);
        if (!bytes || count + 1 >= sizeof(letters) / sizeof(letters[0]))
            return;
        uint32_t cp = aster_utf8_cp(body + at, bytes);
        letters[count].byte_offset = at;
        glyphs[count].codepoint = cp == '\t' || cp == 0x3000 ? ' ' : cp;
        word_layout |= lyric_latin(cp);
        count++;
        at += bytes;
    }
    letter_count = count;
    letters[count].byte_offset = strlen(body);
    bool bold = style >= LYRIC_VARIANTS || lyric_layout_vertical(style);
    if (count && (word_layout || count <= CHINESE_LAYOUT_LIMIT) &&
        paper_font_line(glyphs, count, bold)) {
        glyph_count = count;
        if (word_layout)
            lyric_layout_words(style, glyphs, count);
        else
            place_glyphs(style);
    } else
        word_layout = false;
    time_glyphs(lyrics, line, duration);
}

void paper_prepare(const lrc_t *lyrics, int line, uint32_t duration, unsigned style)
{
    prepare_letters(lyrics, line, duration, style);
    if (style / LYRIC_VARIANTS == CAMERA_PUSH) {
        light_slot ^= 1;
        build_lights(style, glyph_count ? paper_font_mask(glyphs[0].mask_slot).alpha : NULL,
                     paper_font_cell());
    }
}

bool paper_highlight(const lrc_t *lyrics, int line, uint32_t position)
{
    if (!letter_count || line < 0)
        return false;
    int64_t elapsed = (int64_t)position + lyrics->offset_ms - lyrics->cue[line].time_ms;
    bool dirty = false;
    unsigned bytes = 0;
    for (unsigned i = 0; i < letter_count; i++) {
        struct letter *letter = &letters[i];
        unsigned next = elapsed < letter->begin ? 0
                        : elapsed >= letter->end
                            ? 256
                            : (elapsed - letter->begin) * 256 / (letter->end - letter->begin);
        dirty |= glyph_count && next != letter->progress;
        letter->progress = next;
        if (next == 256)
            bytes = letters[i + 1].byte_offset;
    }
    dirty |= bytes != sung_bytes;
    sung_bytes = bytes;
    return dirty;
}

typedef struct {
    uint16_t *pixels;
    int extent, stride;
    int x0, y0, x1, y1;
} canvas_t;

static void fill_span(uint16_t *row, int x, int end, uint16_t color)
{
    for (; x < end; x++)
        row[x] = color;
}

static void polygon_span(uint16_t *row, int lo, int hi, int y, int size, const point_t *poly,
                         unsigned count, uint16_t color)
{
    int begin = size, end = 0;
    for (unsigned i = 0, j = count - 1; i < count; j = i++) {
        int low_y = poly[j].y, high_y = poly[i].y;
        if ((y < low_y) == (y < high_y))
            continue;
        int x = poly[j].x + (y - low_y) * (poly[i].x - poly[j].x) / (high_y - low_y);
        x = x * size / DESIGN;
        begin = min(begin, x);
        end = max(end, x);
    }
    fill_span(row, max(lo, begin), min(hi, end), color);
}

static void crease(uint16_t *row, int lo, int hi, int y, int size, point_t a, point_t b,
                   uint16_t color)
{
    if (y < min(a.y, b.y) || y >= max(a.y, b.y))
        return;
    int center = (a.x + (y - a.y) * (b.x - a.x) / (b.y - a.y)) * size / DESIGN;
    int span = max(1, (b.x > a.x ? b.x - a.x : a.x - b.x) / (max(a.y, b.y) - min(a.y, b.y)));
    fill_span(row, max(lo, center - span / 2), min(hi, center + (span + 1) / 2), color);
}

static void wall_row(uint16_t *row, int lo, int hi, int y, int size, unsigned style)
{
    const palette_t *p = &palettes[style];
    fill_span(row, lo, hi, rgb565(p->base));
#define POLYGON(poly, color)                                                                       \
    polygon_span(row, lo, hi, y, size, poly, sizeof(poly) / sizeof((poly)[0]), color)
    unsigned variant = style % LYRIC_VARIANTS;
    if (variant >= 3)
        y = DESIGN - 1 - y;
    if (style / LYRIC_VARIANTS == FOLD_PLANES) {
        static const point_t diagonal[] = {{0, 282}, {460, 50}, {460, 201}, {0, 438}};
        static const point_t vertical[] = {{118, 0}, {326, 0}, {299, 460}, {92, 460}};
        static const point_t flap[] = {{356, 312}, {460, 235}, {460, 460}, {356, 460}};
        static const point_t foothold[] = {{0, 320}, {460, 320}, {460, 405}, {0, 352}};
        uint16_t edge = tint(p->panel, p->ink, 46);
        switch (variant % 3) {
        case 0:
            POLYGON(diagonal, rgb565(p->panel));
            for (int i = 0; i < 3; i++) {
                int h = i == 0 ? 271 : i == 1 ? 405 : 429;
                crease(row, lo, hi, y, size, (point_t){0, h}, (point_t){460, h - 232}, edge);
            }
            break;
        case 1:
            POLYGON(vertical, rgb565(p->panel));
            POLYGON(flap, tint(p->base, p->panel, 46));
            for (int x = 146; x <= 304; x += 158)
                crease(row, lo, hi, y, size, (point_t){x, 0}, (point_t){x - 24, 460}, edge);
            break;
        default:
            if (y >= 141 && y < 320)
                fill_span(row, lo, hi, rgb565(p->panel));
            POLYGON(foothold, tint(p->panel, p->base, 64));
            if (y == 129 || y == 335)
                fill_span(row, lo, hi, edge);
        }
    } else if (variant % 3 == 0) {
        POLYGON(pink_sheet, tint(p->base, p->panel, 24));
        POLYGON(pink_band, tint(p->base, p->panel, 61));
        POLYGON(pink_corner, rgb565(p->panel));
        crease(row, lo, hi, y, size, (point_t){0, 312}, (point_t){330, 120},
               tint(p->base, p->panel, 51));
    } else if (variant % 3 == 1) {
        POLYGON(violet_sheet, rgb565(p->panel));
        POLYGON(violet_bottom, tint(p->base, p->panel, 35));
        POLYGON(violet_side, tint(p->base, p->pending, 20));
        crease(row, lo, hi, y, size, (point_t){233, 0}, (point_t){205, 460},
               tint(p->panel, p->pending, 44));
    } else {
        POLYGON(blue_sheet, tint(p->base, p->panel, 68));
        POLYGON(blue_corner, tint(p->base, p->panel, 49));
        crease(row, lo, hi, y, size, (point_t){0, 359}, (point_t){460, 132},
               tint(p->panel, p->ink, 49));
    }
#undef POLYGON
}

static void stage_background(const canvas_t *canvas, unsigned style, unsigned previous,
                             unsigned alpha)
{
    const struct light_map *front = &stage_lights[light_slot];
    const struct light_map *back = &stage_lights[light_slot ^ 1];
    const palette_t *p = &palettes[style];
    bool blend = alpha < 32 && previous != style && back->scene == (int)previous;
    int size = canvas->extent, lo = canvas->x0, hi = canvas->x1;
    uint16_t edges[3];
    for (int i = 0; i < 3; i++)
        edges[i] = tint(p->base, p->pending, 24 - 4 * i);
    for (int y = canvas->y0; y < canvas->y1;) {
        int v = y * LIGHT_MAP / size;
        int bottom = min(canvas->y1, ((v + 1) * size + LIGHT_MAP - 1) / LIGHT_MAP);
        uint16_t *row = canvas->pixels + y * canvas->stride;
        for (int x = lo; x < hi;) {
            int u = size == DESIGN ? x / 4 : x * LIGHT_MAP / size;
            int edge = size == DESIGN ? (u + 1) * 4 : ((u + 1) * size + LIGHT_MAP - 1) / LIGHT_MAP;
            uint16_t color = front->scene == (int)style ? front->pixels[v][u] : rgb565(p->base);
            if (blend)
                color = mix(back->pixels[v][u], color, alpha);
            int end = min(hi, edge);
            fill_span(row, x, end, color);
            x = end;
        }
        /* Expand each light-map row once, including when repainting just a word. */
        for (int next = y + 1; next < bottom; next++)
            memcpy(canvas->pixels + next * canvas->stride + lo, row + lo, (hi - lo) * sizeof(*row));
        for (; y < bottom; y++) {
            row = canvas->pixels + y * canvas->stride;
            for (int i = 0; i < 3; i++) {
                int h = (style % 3 == 2 ? 270 : 309) + 16 * i;
                crease(row, lo, hi, y * DESIGN / size, size, (point_t){0, h},
                       (point_t){460, h - 176}, edges[i]);
            }
        }
    }
}

static void backdrop(const canvas_t *canvas, unsigned style, unsigned previous, uint32_t elapsed)
{
    int size = canvas->extent;
    int revealed[5];
    enum lyric_effect effect = style / LYRIC_VARIANTS;
    if (effect == CAMERA_PUSH) {
        unsigned alpha = (unsigned)(32 * smooth((float)elapsed / CAMERA_MOVE_MS));
        stage_background(canvas, style, previous, alpha);
        return;
    }
    int turn_ms = effect == FOLD_PLANES ? 440 : PAPER_TURN_MS;
    float open = smooth((float)elapsed / turn_ms);
    for (unsigned band = 0; band < 5; band++)
        revealed[band] =
            style == previous ? size : (int)(size * smooth((open - band * .065f) / .71f) + .5f);
    for (int y = canvas->y0; y < canvas->y1; y++) {
        uint16_t *row = canvas->pixels + y * canvas->stride;
        unsigned band = (unsigned)y * 5 / size;
        int span = revealed[band];
        int v = y * DESIGN / size;
        if (effect == FOLD_PLANES && style != previous && open < 1.f) {
            int slope = style % 2 ? v - 230 : 230 - v;
            int edge =
                max(0, min(size, (int)((-280 + 1080 * open + slope * .72f) * size / DESIGN)));
            wall_row(row, 0, edge, v, size, style);
            wall_row(row, edge, size, v, size, previous);
            if (edge && edge < size)
                fill_span(row, edge, min(size, edge + 3),
                          tint(palettes[style].panel, palettes[style].ink, 72));
        } else if (span == 0 || span == size || effect == FOLD_PLANES) {
            wall_row(row, canvas->x0, canvas->x1, v, size, span == size ? style : previous);
        } else {
            int edge = band & 1 ? size - span : span;
            wall_row(row, 0, edge, v, size, band & 1 ? previous : style);
            wall_row(row, edge, size, v, size, band & 1 ? style : previous);
            fill_span(row, max(0, edge - 1), min(size, edge + 2), rgb565(0xdcb191));
        }
    }
}

static unsigned sample_alpha(const uint8_t *mask, int cell, int x, int y)
{
    int u = x / FP, v = y / FP;
    if ((unsigned)u >= (unsigned)cell - 1 || (unsigned)v >= (unsigned)cell - 1)
        return 0;
    unsigned a = (x % FP) / (FP / 16), b = (y % FP) / (FP / 16);
    const uint8_t *p = mask + v * cell + u;
    unsigned top = p[0] * (16 - a) + p[1] * a;
    p += cell;
    unsigned bottom = p[0] * (16 - a) + p[1] * a;
    return (top * (16 - b) + bottom * b + 128) >> 8;
}

typedef struct {
    uint16_t pending, ink, contour;
    unsigned coverage, progress;
    bool vertical, outline;
} brush_t;

static void stamp(const canvas_t *canvas, paper_mask_t mask, float x, float y, float em,
                  float angle, const brush_t *brush)
{
    int mid_x = (int)(x * FP), mid_y = (int)(y * FP);
    int cell = paper_font_cell();
    float ratio = (float)(PAPER_FONT_EM * FP * cell) / (PAPER_FONT_CELL * em);
    float c = cosine(angle), s = sine(angle);
    int du = (int)(c * ratio), dv = (int)(s * ratio);
    int half = (int)((c + (s < 0 ? -s : s)) * em * PAPER_FONT_CELL / (2 * PAPER_FONT_EM)) + 2;
    int glyph_x = max(0, (int)x - half);
    int first_x = max(canvas->x0, glyph_x), last_x = min(canvas->x1, (int)x + half);
    int first_y = max(canvas->y0, (int)y - half), last_y = min(canvas->y1, (int)y + half);
    if (first_x >= last_x || first_y >= last_y)
        return;
    int origin = (cell - 1) * FP / 2;
    int edge = word_layout
                   ? (mask.left * FP + (mask.right - mask.left) * FP * (int)brush->progress / 256) *
                         cell / PAPER_FONT_CELL
                   : origin - PAPER_FONT_EM * FP / 2 + PAPER_FONT_EM * FP * brush->progress / 256;
    if (brush->progress == 256)
        edge = cell * FP;
    else if (!brush->progress)
        edge = 0;

    for (int row = first_y; row < last_y; row++) {
        /* Anchor the fixed-point rounding at the same pixel for full and dirty frames. */
        int dx = glyph_x * FP - mid_x, dy = row * FP - mid_y;
        int skipped = first_x - glyph_x;
        int u = origin + (dx * du + dy * dv) / FP + skipped * du;
        int v = origin + (dy * du - dx * dv) / FP - skipped * dv;
        uint16_t *pixel = canvas->pixels + row * canvas->stride + first_x;
        for (int col = first_x; col < last_x; col++, u += du, v -= dv, pixel++) {
            if (u < 0 || v < 0 || u >= (cell - 1) * FP || v >= (cell - 1) * FP)
                continue;
            unsigned alpha = sample_alpha(mask.alpha, cell, u, v);
            if (brush->outline && alpha < 128 && u >= FP && v >= FP) {
                const uint8_t *near = mask.alpha + (v / FP) * cell + u / FP;
                unsigned rim = max(max(near[-1], near[1]), max(near[-cell], near[cell]));
                if (rim > alpha)
                    *pixel = mix(*pixel, brush->contour, (rim - alpha + 4) / 8);
            }
            if (alpha) {
                int coord = brush->vertical ? v : u;
                unsigned lit = max(0, min(32, (edge - coord + FP / 2) / (FP / 32)));
                uint16_t color = mix(brush->pending, brush->ink, lit);
                *pixel = mix(*pixel, color, (alpha * brush->coverage + 128) / 256);
            }
        }
    }
}

static void typeset(const canvas_t *canvas, unsigned style, uint32_t elapsed,
                    lyric_transition_t transition)
{
    const palette_t *p = &palettes[style];
    enum lyric_effect effect = style / LYRIC_VARIANTS;
    unsigned variant = style % LYRIC_VARIANTS;
    float remain = 1.f - unit(elapsed / (effect == CAMERA_PUSH   ? (float)CAMERA_MOVE_MS
                                         : effect == FOLD_PLANES ? 460.f
                                                                 : 420.f));
    float entry = remain * remain * remain;
    float scale = 1.f - entry * (effect == CAMERA_PUSH   ? .10f
                                 : effect == FOLD_PLANES ? .12f
                                                         : .065f);
    float turn = effect == CAMERA_PUSH   ? (variant % 2 ? 7.f : -5.f)
                 : effect == FOLD_PLANES ? (variant % 2 ? 11.f : -10.f)
                                         : (variant % 2 ? 3.f : -3.f);
    float shift = radians(turn * entry);
    float kick = 0;
    if (transition.leaving) {
        scale *= 1.f - .32f * smooth((float)transition.age_ms / LYRIC_EXIT_MS);
    } else if (transition.age_ms < LYRIC_ENTER_MS) {
        float t = (float)transition.age_ms / LYRIC_ENTER_MS;
        if (effect == CUT_PAPER)
            kick = (1.f - t) * (1.f - t) * (t < .2f ? 9.f : t < .4f ? -6.f : 3.f);
        if (effect == CAMERA_PUSH)
            scale *= 1.f + .10f * (1.f - smooth(t));
    }
    float c = cosine(shift), s = sine(shift);
    float ratio = (float)canvas->extent / DESIGN;
    uint16_t shade = tint(p->panel, p->base, 24);
    brush_t shadow = {.pending = shade, .ink = shade, .coverage = 32};
    brush_t face = {.pending = rgb565(p->pending),
                    .ink = rgb565(p->ink),
                    .contour = tint(p->panel, p->pending, 34),
                    .coverage = (unsigned)(32.f - 3.2f * entry + .5f),
                    .vertical = !word_layout && lyric_layout_vertical(style),
                    .outline = effect != FOLD_PLANES};
    unsigned word_end = 0;
    float word_center = 0.f, word_zoom = 0.f;
    for (unsigned i = 0; i < glyph_count; i++) {
        const lyric_glyph_t *glyph = &glyphs[i];
        if (glyph->codepoint == ' ')
            continue;
        pose_t pose = glyph->pose;
        if (word_layout && effect == CAMERA_PUSH && i >= word_end) {
            word_end = glyph->word_end;
            word_center = (pose.x + glyphs[word_end - 1].pose.x) * .5f;
            unsigned sung = 0;
            for (unsigned j = i; j < word_end; j++)
                sung += letters[j].progress;
            float t = (float)sung / ((word_end - i) * 256);
            word_zoom = .14f * t * (1.f - t);
        }
        float fold = word_layout && effect == FOLD_PLANES
                         ? ((glyph->row + variant) % 2 ? 18.f : -18.f) * entry
                         : 0.f;
        float dx = (pose.x + (pose.x - word_center) * word_zoom + fold - 230) * scale;
        float dy = (pose.y - 230) * scale;
        float x = 230 + dx * c - dy * s + kick;
        float y = 230 + dx * s + dy * c + (effect == CUT_PAPER ? 17.f * entry : 0);
        float progress = letters[i].progress / 256.f;
        float pulse =
            word_layout ? 0.f : sine(3.14159265f * (progress < .5f ? progress : 1.f - progress));
        float angle = radians(pose.degrees + (effect == CAMERA_PUSH ? 2.8f : 1.7f) * pulse) + shift;
        paper_mask_t mask = paper_font_mask(glyph->mask_slot);
        if (effect == CUT_PAPER) {
            float offset = word_layout ? pose.em * .035f : 4.f;
            stamp(canvas, mask, (x + offset) * ratio, (y + offset + 1.f) * ratio,
                  pose.em * scale * ratio, angle, &shadow);
        }
        face.progress = letters[i].progress;
        stamp(canvas, mask, x * ratio, (y - (effect == CAMERA_PUSH ? 6.f : 3.8f) * pulse) * ratio,
              (pose.em * (1.f + word_zoom) + (effect == CAMERA_PUSH ? 7.f : 4.6f) * pulse) * scale *
                  ratio,
              angle, &face);
    }
}

static void accent_frame(const canvas_t *canvas, unsigned scene, lyric_transition_t transition)
{
    enum lyric_effect effect = scene / LYRIC_VARIANTS;
    const palette_t *p = &palettes[scene];
    int size = canvas->extent;
    float t =
        unit((float)transition.age_ms / (transition.leaving ? LYRIC_EXIT_MS : LYRIC_ENTER_MS));
    if (t >= 1.f)
        return;
    if (transition.leaving || effect == FOLD_PLANES) {
        /* Two cut planes meet on exit and peel back diagonally on entry. */
        float cover = transition.leaving ? smooth(t) : 1.f - smooth(t);
        for (int y = 0; y < size; y++) {
            int center = size / 2 + (y - size / 2) / 3;
            int lo = (int)(cover * center), hi = size - (int)(cover * (size - center));
            uint16_t *row = canvas->pixels + y * canvas->stride;
            fill_span(row, 0, lo, rgb565(p->base));
            fill_span(row, hi, size, tint(p->base, p->panel, 46));
            if (lo)
                fill_span(row, max(0, lo - 2), lo, tint(p->panel, p->ink, 70));
            if (hi < size)
                fill_span(row, hi, min(size, hi + 2), rgb565(p->pending));
        }
        return;
    }
    /* Fixed trajectories keep fragments coherent across frames without RNG or allocations. */
    for (int i = 0; i < 28; i++) {
        float depth = 1.f - (1.f - t) * (1.f - t);
        int dx = (i * 179 + 73) % 451 - 225, dy = (i * 103 + 29) % 449 - 224;
        int cx, cy;
        if (effect == CAMERA_PUSH) {
            cx = 230 + (int)(dx * (.24f + 1.35f * depth));
            cy = 230 + (int)(dy * (.24f + 1.35f * depth));
        } else {
            cx = (i * 139 + 17) % 460 + (int)(dx * .26f * t);
            cy = (i * 97 + 51) % 460 - (int)((75 + i % 5 * 14) * depth) + (int)(80 * t * t);
        }
        int radius = max(
            1, (int)((effect == CUT_PAPER ? 8 + i % 5 : 5 + i % 4) * (1.f - t) * size / DESIGN));
        cx = cx * size / DESIGN;
        cy = cy * size / DESIGN;
        uint16_t color = tint(p->panel, p->ink, 30 + (i % 4) * 16);
        for (int j = -radius; j <= radius; j++) {
            int y = cy + j;
            if (y < 0 || y >= size)
                continue;
            int half = radius - (j < 0 ? -j : j);
            int skew = effect == CUT_PAPER ? j / 2 : -j;
            fill_span(canvas->pixels + y * canvas->stride, max(0, cx + skew - half),
                      min(size, cx + skew + half + 1), color);
        }
    }
}

/* Long lines and uncommon glyphs can still use the complete system font. */
static void system_lyric(aster_ge_t *gfx, int left, int top, int size, unsigned style,
                         const char *text, unsigned sung)
{
    uint32_t length = strlen(text);
    static const unsigned faces[] = {32, 28, 24, 20, 16, 12};
    unsigned px;
    int line_height, count;
    for (unsigned f = 0; f < sizeof(faces) / sizeof(faces[0]); f++) {
        px = aster_text_snap(faces[f] * size / DESIGN);
        line_height = aster_text_line_h(px) + 3;
        count = 0;
        for (unsigned offset = 0; offset < length;) {
            uint32_t bytes;
            unsigned skip = aster_text_line(px, text + offset, length - offset, size - 36, &bytes);
            if (!skip)
                return;
            count++;
            offset += skip;
        }
        if (count * line_height <= size - 24)
            break;
    }
    int y = top + max(12, (size - count * line_height) / 2) + aster_text_asc(px);
    for (unsigned offset = 0; offset < length && y < top + size;) {
        char row[768];
        uint32_t bytes;
        unsigned skip = aster_text_line(px, text + offset, length - offset, size - 36, &bytes);
        if (!skip || bytes >= sizeof(row))
            break;
        memcpy(row, text + offset, bytes);
        row[bytes] = 0;
        int width = aster_gfx_text_w(px, row, bytes);
        int x = left + (size - max(0, width)) / 2;
        unsigned lit = sung > offset ? (unsigned)min(sung - offset, bytes) : 0;
        if (lit < bytes) {
            int pre = lit ? aster_gfx_text_w(px, row, lit) : 0;
            aster_ge_text(gfx, x + max(0, pre), y, px, rgb565(palettes[style].pending),
                          ASTER_GFX_NOBG, row + lit);
        }
        if (lit) {
            row[lit] = 0;
            aster_ge_text(gfx, x, y, px, rgb565(palettes[style].ink), ASTER_GFX_NOBG, row);
        }
        offset += skip;
        y += line_height;
    }
}

int paper_draw(aster_ge_t *gfx, int left, int top, int extent, unsigned style, unsigned previous,
               uint32_t elapsed, const char *lyrics, const char *status,
               lyric_transition_t transition)
{
    aster_surface_buffer_t buf = {.size = sizeof(buf), .version = 1};
    int result = aster_surface_acquire(gfx->surf, &buf, sizeof(buf));
    if (result < 0)
        return result;
    if (!buf.ptr || buf.format != ASTER_PIXEL_RGB565)
        return ASTER_ENOTSUP;

    int stride = buf.pitch / sizeof(uint16_t);
    uint16_t *pixels = buf.ptr;
    canvas_t canvas = {pixels + top * stride + left, extent, stride, 0, 0, extent, extent};
    bool switching = transition.leaving || transition.age_ms < LYRIC_ENTER_MS;
    unsigned settle_ms = style / LYRIC_VARIANTS == CAMERA_PUSH   ? CAMERA_MOVE_MS
                         : style / LYRIC_VARIANTS == FOLD_PLANES ? 460
                                                                 : PAPER_TURN_MS;
    bool settled = glyph_count && lyrics && *lyrics && elapsed >= settle_ms && !switching;
    bool partial = false;
    if (settled_frame && settled) {
        int x0 = extent, y0 = extent, x1 = 0, y1 = 0;
        for (unsigned first = 0; first < glyph_count;) {
            unsigned end = word_layout && style / LYRIC_VARIANTS == CAMERA_PUSH
                               ? glyphs[first].word_end
                               : first + 1;
            bool changed = false;
            for (unsigned i = first; i < end; i++)
                changed |= letters[i].progress != displayed[i];
            if (changed) {
                for (unsigned i = first; i < end; i++) {
                    if (glyphs[i].codepoint == ' ')
                        continue;
                    pose_t p = glyphs[i].pose;
                    /* Includes the word zoom, rotation and cast shadow. */
                    int pad = p.em * 3 / 4 + 14;
                    x0 = min(x0, (p.x - pad) * extent / DESIGN);
                    y0 = min(y0, (p.y - pad) * extent / DESIGN);
                    x1 = max(x1, (p.x + pad) * extent / DESIGN + 1);
                    y1 = max(y1, (p.y + pad) * extent / DESIGN + 1);
                }
            }
            first = end;
        }
        if (x0 < x1 && y0 < y1) {
            partial = true;
            canvas.x0 = max(0, x0);
            canvas.y0 = max(0, y0);
            canvas.x1 = min(extent, x1);
            canvas.y1 = min(extent, y1);
        }
    }
    settled_frame = false;
    if (!partial && (left || top))
        for (unsigned row = 0; row < buf.height; row++)
            fill_span(pixels + row * stride, 0, buf.width, rgb565(palettes[style].base));
    backdrop(&canvas, style, previous, elapsed);
    if (glyph_count && lyrics && *lyrics) {
        typeset(&canvas, style, elapsed, transition);
        if (switching)
            accent_frame(&canvas, style, transition);
    } else {
        if (switching)
            accent_frame(&canvas, style, transition);
        const char *text = lyrics && *lyrics ? lyrics : status;
        if (text && *text) {
            system_lyric(gfx, left, top, extent, style, text,
                         text == status ? strlen(text) : sung_bytes);
            aster_ge_touch(gfx, 0, 0, buf.width, buf.height);
            return aster_ge_present(gfx);
        }
    }
    if (partial)
        result = aster_surface_flush(gfx->surf, canvas.pixels + canvas.y0 * stride + canvas.x0,
                                     left + canvas.x0, top + canvas.y0, canvas.x1 - canvas.x0,
                                     canvas.y1 - canvas.y0, stride);
    else
        result = aster_surface_present(gfx->surf);
    if (result >= 0 && settled) {
        settled_frame = true;
        for (unsigned i = 0; i < glyph_count; i++)
            displayed[i] = letters[i].progress;
    }
    return result;
}
