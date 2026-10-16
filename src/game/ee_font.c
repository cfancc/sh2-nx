/* Local Chinese-language adaptation for sh2-nx v1.00.
 * Decode a full EE font sheet on the CPU and replace each glyph as the game fills
 * its original texture. This preserves the game's existing text UVs and sizes.
 * The glyph slots and image addresses refer to the exact PC exe used by this port. */
#include "ee_font.h"
#include "../runtime/rt.h"

static uint8_t *font;
static uint32_t font_w, font_h, font_cols, font_rows, cell_w, cell_h;
static uint8_t widths[448];
static int widths_loaded;
static uint32_t widths_applied[2];

static uint32_t le16(const uint8_t *p) { return p[0] | (uint32_t)p[1] << 8; }

/* Standard true-colour TGA, including repeat and raw RLE packets. The input
 * cursor consumes a repeat pixel once per packet, not once per output pixel. */
static uint8_t *decode_tga(const uint8_t *in, size_t n, uint32_t *w, uint32_t *h) {
    if (n < 18 || in[1] || (in[2] != 2 && in[2] != 10) || (in[16] != 24 && in[16] != 32)) return NULL;
    *w = le16(in + 12); *h = le16(in + 14);
    if (!*w || !*h || (uint64_t)*w * *h > 64u * 1024u * 1024u) return NULL;
    size_t at = 18u + in[0], total = (size_t)*w * *h, bpp = in[16] / 8;
    uint8_t *out = malloc(total * 4);
    if (!out) return NULL;
    for (size_t i = 0; i < total;) {
        size_t count = 1;
        int repeat = 0;
        if (in[2] == 10) {
            if (at >= n) goto bad;
            uint8_t packet = in[at++];
            count = (packet & 127u) + 1u;
            repeat = packet & 128u;
        }
        if (count > total - i || at > n || (repeat ? bpp : count * bpp) > n - at) goto bad;
        for (size_t j = 0; j < count; j++, i++) {
            size_t x = i % *w, y = i / *w;
            if (in[17] & 16) x = *w - 1 - x;
            if (!(in[17] & 32)) y = *h - 1 - y;
            uint8_t *p = out + (y * *w + x) * 4;
            const uint8_t *s = in + at + (repeat ? 0 : j * bpp);
            p[0] = s[0]; p[1] = s[1]; p[2] = s[2]; p[3] = bpp == 4 ? s[3] : 255;
        }
        at += repeat ? bpp : count * bpp;
    }
    return out;
bad:
    free(out);
    return NULL;
}

static void apply_widths(void) {
    if (!widths_loaded) return;
    for (int i = 0; i < 2; i++) {
        uint32_t p = MEM32(0x008038A0u + 4u * i);
        if (p && p <= UINT32_MAX - 240u && p != widths_applied[i]) {
            uint8_t *dst = GPTR(p);
            memcpy(dst + 16, widths + 224 * i, 224);
            widths_applied[i] = p;
        }
    }
}

void ee_font_init(void) {
    char path[512];
    FILE *f = fopen(rt_path("data/font/font000.tga", path, sizeof path), "rb");
    if (!f) { rt_log("ee: no custom TGA font at %s", path); return; }
    if (fseek(f, 0, SEEK_END)) { fclose(f); return; }
    long size = ftell(f);
    if (size < 18 || size > 64 * 1024 * 1024 || fseek(f, 0, SEEK_SET)) { fclose(f); return; }
    uint8_t *bytes = malloc((size_t)size);
    if (!bytes) { fclose(f); return; }
    size_t got = fread(bytes, 1, (size_t)size, f);
    fclose(f);
    if (got == (size_t)size) font = decode_tga(bytes, got, &font_w, &font_h);
    free(bytes);
    if (!font) { rt_log("ee: invalid custom TGA font %s", path); return; }
    font_cols = font_w / 44; font_rows = font_h / 64;
    if (!font_cols || font_rows < 2 || (font_rows & 1)) { free(font); font = NULL; return; }
    cell_w = font_w / font_cols; cell_h = font_h / font_rows;
    rt_log("ee: custom font %ux%u (%ux%u cell, %ux%u grid) from %s", font_w, font_h, cell_w, cell_h, font_cols, font_rows, path);
    f = fopen(rt_path("data/font/fontwdata.bin", path, sizeof path), "rb");
    if (f) { widths_loaded = fread(widths, 1, sizeof widths, f) == sizeof widths; fclose(f); }
    apply_widths();
}

void ee_font_overlay(uint32_t texture, uint32_t bits, uint32_t pitch, uint32_t width, uint32_t height) {
    if (!font || !bits || texture != MEM32(0x00943928u)) return;
    apply_widths();
    uint32_t slot = MEM16(0x01F5FEC0u);
    if (slot > 424) return;
    uint16_t code = MEM16(0x01F5F560u + slot * 2);
    uint32_t glyph = code & 0x1FFFu, row = glyph / font_cols;
    if (row * 2 >= font_rows) return;
    uint32_t sx = glyph % font_cols * cell_w;
    uint32_t sy = row * cell_h + (code & 0x2000 ? font_h / 2 : 0);
    uint32_t dx = slot % 25 * 22, dy = slot / 25 * 32;
    if (dx + 22 > width || dy + 32 > height || pitch < width * 4) return;
    uint8_t *dst = GPTR(bits);
    for (uint32_t y = 0; y < 32; y++) {
        uint32_t ya = y * cell_h / 32, yb = (y + 1) * cell_h / 32;
        if (yb <= ya) yb = ya + 1;
        for (uint32_t x = 0; x < 22; x++) {
            uint32_t xa = x * cell_w / 22, xb = (x + 1) * cell_w / 22;
            if (xb <= xa) xb = xa + 1;
            uint32_t sums[4] = {0}, count = (yb - ya) * (xb - xa);
            for (uint32_t yy = ya; yy < yb; yy++)
                for (uint32_t xx = xa; xx < xb; xx++) {
                    const uint8_t *p = font + ((sy + yy) * font_w + sx + xx) * 4;
                    for (int c = 0; c < 4; c++) sums[c] += p[c];
                }
            uint8_t *p = dst + (dy + y) * pitch + (dx + x) * 4;
            for (int c = 0; c < 4; c++) p[c] = sums[c] / count;
        }
    }
}
