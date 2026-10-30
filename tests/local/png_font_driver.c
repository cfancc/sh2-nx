/* Includes the live upstream font implementation extracted by check_png_font.py. */
#include "../../src/runtime/rt.h"
#include <assert.h>
#include <libavcodec/avcodec.h>

ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax, g_ecx, g_edx, g_esp;
void rt_log(const char *fmt, ...) { (void)fmt; }
const char *rt_path(const char *guest, char *out, size_t n) {
    snprintf(out, n, "game/lang/%s", guest + 5);
    return out;
}
#include "upstream-font-under-test.inc"

static void save(const char *name, const void *bytes, size_t n) {
    FILE *f = fopen(name, "wb");
    assert(f && fwrite(bytes, 1, n, f) == n);
    fclose(f);
}
int main(void) {
    /* Includes EE's reserved variables at 0x0EC01010, beyond the font texture. */
    g_xbox_mem_offset = (ptrdiff_t)calloc(1, 0x0ED00000u);
    assert(g_xbox_mem_offset);
    MEM32(FONT_DATA) = 0x100000;
    MEM32(FONT_DATA + 4) = 0x101000;
    ee_fonts_init();
    assert(font_px && font_w == 4400 && font_h == 9600);
    assert(font_cw == 44 && font_ch == 64 && font_glyphs == 7500);
    uint8_t widths[448];
    FILE *f = fopen("game/lang/font/fontwdata.bin", "rb");
    assert(f && fread(widths, 1, sizeof widths, f) == sizeof widths);
    fclose(f);
    assert(!memcmp(GPTR(0x100010), widths, 224));
    assert(!memcmp(GPTR(0x101010), widths + 224, 224));
    save("build/png-decoded.ya8", font_px, (size_t)font_w * font_h * 2);
    g_esp = 0x200000;
    sub_00401280();
    assert(g_ecx == 1100 && g_eax == 1088 && g_esp == 0x200004);
    assert(MEM32(0x00943944) == 1100 && MEM32(0x00943948) == 1088);
    const uint16_t cases[][3] = {{0,0,0}, {1,33,0}, {24,350,0}, {25,7000,0},
                                 {424,7000,1}, {100,7499,1}, {2,8191,0}};
    for (size_t i = 0; i < sizeof cases / sizeof *cases; i++) {
        uint32_t slot = cases[i][0], frame = 0x200000;
        MEM32(FONT_TYPE) = cases[i][2];
        MEM32(frame + 0x2C) = 1100 * 4;
        MEM32(frame + 0x30) = 0x02000000;
        MEM16(frame + 0x38) = cases[i][1];
        memset(GPTR(0x02000000), 0x5A, 1100 * 1088 * 4);
        g_esp = frame - 4;
        g_edx = slot;
        sub_00401288();
        assert(g_eax == 0x80000000u && g_esp == frame && MEM16(FONT_SLOT) == slot);
        uint8_t tile[44 * 64 * 4];
        for (int y = 0; y < 64; y++)
            memcpy(tile + y * 44 * 4,
                   (uint8_t *)GPTR(0x02000000) + (slot / 25 * 64 + y) * 4400 + slot % 25 * 44 * 4, 44 * 4);
        char name[80];
        snprintf(name, sizeof name, "build/png-glyph-%zu.bgra", i);
        save(name, tile, sizeof tile);
    }
    free(font_px);
    font_px = NULL;
    g_esp = 0x200000;
    sub_00401280();
    assert(g_ecx == 550 && g_eax == 544);
    MEM32(FONT_TYPE) = 1;
    g_esp = 0x200000 - 4;
    sub_00401288();
    assert(g_eax == 1 && MEM32(EE_FONT_DATA) == 0x101000);
    MEM32(FONT_DATA) = 0;
    MEM32(FONT_DATA + 4) = UINT32_MAX - 8;
    ee_fonts_init();
    assert(font_px);  /* Invalid width destinations must be skipped safely. */
    free(font_px);
    free((void *)g_xbox_mem_offset);
    puts("upstream PNG decode, widths, HD cache, glyph cases and original-font fallback passed");
    return 0;
}
