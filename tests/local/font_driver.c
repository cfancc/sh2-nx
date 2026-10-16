/* Offline comparison with the original, hardware-confirmed font rasterizer. */
#include <stdarg.h>
#include <assert.h>
#include "../../src/game/ee_font.c"

ptrdiff_t g_xbox_mem_offset;
void rt_log(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap); fputc('\n', stderr);
}
const char *rt_path(const char *guest, char *out, size_t n) {
    snprintf(out,n,"game/lang/%s",guest+5);
    return out;
}
static void write_file(const char *name, const void *data, size_t n) {
    FILE *f=fopen(name,"wb"); assert(f); assert(fwrite(data,1,n,f)==n); fclose(f);
}
int main(void) {
    g_xbox_mem_offset=(ptrdiff_t)calloc(1,0x02500000u);
    assert(g_xbox_mem_offset);
    MEM32(0x008038A0)=0x100000; MEM32(0x008038A4)=0x101000;
    ee_font_init();
    assert(font && font_w==4400 && font_h==9600 && widths_loaded);
    assert(!memcmp(GPTR(0x100010),widths,224));
    assert(!memcmp(GPTR(0x101010),widths+224,224));
    write_file("build/font-decoded.bgra",font,(size_t)font_w*font_h*4);
    const uint16_t cases[][2]={{0,0},{1,33},{24,350},{25,7000},{424,0x2000|7000},{100,0x2000|7499}};
    for(size_t i=0;i<sizeof cases/sizeof *cases;i++) {
        uint32_t slot=cases[i][0], code=cases[i][1];
        MEM32(0x00943928)=0x1234;
        MEM16(0x01F5FEC0)=slot;
        MEM16(0x01F5F560+slot*2)=code;
        memset(GPTR(0x02000000),0,1024*1024*4);
        ee_font_overlay(0x1234,0x02000000,4096,1024,1024);
        uint8_t tile[22*32*4];
        uint8_t *surface=GPTR(0x02000000);
        for(int y=0;y<32;y++) memcpy(tile+y*88,surface+(slot/25*32+y)*4096+slot%25*88,88);
        char name[128]; snprintf(name,sizeof name,"build/glyph-%zu.bgra",i);
        write_file(name,tile,sizeof tile);
    }
    /* A malformed repeat packet must fail rather than overrun its input. */
    uint8_t bad[19]={0}; bad[2]=10; bad[12]=1; bad[14]=1; bad[16]=32; bad[18]=0xFF;
    uint32_t w,h; assert(!decode_tga(bad,sizeof bad,&w,&h));
    free(font); free((void *)g_xbox_mem_offset);
    puts("font decode, widths, raster fixtures and malformed-input checks passed");
    return 0;
}
