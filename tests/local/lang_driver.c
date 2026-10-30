/* Exercises the real parser/pause code with only rendering and allocation stubbed. */
#define RECOMP_GENERATED_CODE
#define RECOMP_NO_ICALL_TRACE
#include "recomp_funcs.h"
#include "../../src/runtime/rt.h"
#include "../../src/game/ee_lang.h"
#include <assert.h>
#include <unistd.h>

ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax, g_ecx, g_edx, g_esp, g_ebp, g_ebx, g_esi, g_edi, g_seh_ebp;
RECOMP_TLS volatile uint32_t g_icall_trace[ICALL_TRACE_SIZE], g_icall_trace_idx;
RECOMP_TLS volatile uint64_t g_icall_count;
int rt_trace;
uint32_t g_xbox_code_lo = 0x00400000u, g_xbox_code_hi = 0x00600000u;
void rt_log(const char *fmt, ...) { (void)fmt; }
void recomp_icall_fail_log(uint32_t va) { (void)va; assert(0); }
void recomp_icall_not_code_log(uint32_t va) { (void)va; assert(0); }
void sub_004829F0(void);
recomp_func_t recomp_lookup_manual(uint32_t va) {
    assert(va == 0x004829F0u);
    return sub_004829F0;
}
recomp_func_t recomp_lookup_kernel(uint32_t va) { (void)va; assert(0); return NULL; }
recomp_func_t recomp_lookup(uint32_t va) { (void)va; assert(0); return NULL; }

static uint32_t heap = 0x02000000u;
uint32_t gstrdup(const char *s) {
    uint32_t result = heap;
    heap += (strlen(s) + 16) & ~15u;
    assert(heap < 0x02100000u);
    strcpy(GPTR(result), s);
    return result;
}

static uint16_t rendered[16][128];
static unsigned draws;
void sub_0044C630(void) { g_eax = 1; g_esp += 4; }
void sub_0047E900(void) { g_esp += 4; }
void sub_0047EE00(void) { g_esp += 4; }
void sub_0047EE10(void) { g_esp += 4; }
void sub_0056FD66(void) { g_esp += 4; } /* compiler security-cookie check */
void sub_00480500(void) {
    assert(draws < 16);
    memcpy(rendered[draws++], GPTR(ARG(0)), sizeof rendered[0]);
    g_esp += 4;
}
#include "lang-under-test.inc"

static void parse(uint32_t text, uint16_t out[128], int mapped) {
    uint32_t stack = 0x01C00000u;
    g_esp = stack;
    MEM32(stack) = 0;
    MEM32(stack + 4) = text;
    MEM16(0x00943D68u) = 0;
    memset(GPTR(0x00943968u), 0, 256);
    if (mapped) sub_004829F0(); else sub_004829F0_gen();
    assert(g_esp == stack + 4);
    /* The original parser uses the argument slot as scratch; our wrapper
     * restores it so changing the pointer cannot leak into its caller. */
    if (mapped) assert(MEM32(stack + 4) == text);
    memcpy(out, GPTR(g_eax), 256);
}

int main(int argc, char **argv) {
    assert(argc == 2);
    g_xbox_mem_offset = (ptrdiff_t)calloc(1, 0x02200000u);
    assert(g_xbox_mem_offset);
    FILE *f = fopen("game/sh2pc.exe", "rb");
    assert(f && fread(GPTR(0x00400000u), 1, 5685248, f) == 5685248);
    fclose(f);
    char resource[8192];
    f = fopen("build/lang-fixtures/expected.bin", "rb");
    assert(f);
    size_t size = fread(resource, 1, sizeof resource, f);
    fclose(f);
    assert(size && size < sizeof resource);
    uint32_t expected[33];
    const char *line = resource;
    for (unsigned i = 0; i < 33; i++) { expected[i] = gstrdup(line); line += strlen(line) + 1; }
    char folder[256];
    snprintf(folder, sizeof folder, "build/lang-fixtures/%s", argv[1]);
    assert(chdir(folder) == 0);
    ee_lang_init();
    MEM8(0x01DBC006u) = 0;
    int valid = !strcmp(argv[1], "valid") || !strcmp(argv[1], "lf");
    const uint32_t addresses[] = {
        0x62F0C4,0x62F0B8,0x62F0A8,0x62F098,0x62F08C,0x62F084,0x62F068,0x62F060,0x62F058,
        0x7ABE3C,0x7ABE70,0x7ABE88,0x7ABE98,0x7ABEB8,0x7ABED0,0x7ABEE4,0x7ABEF4,
        0x7ABF04,0x7ABF18,0x7ABF30,0x7ABF38,0,0x7F2868,0x7F2878,0x7F2888};
    for (unsigned i = 0; i < 25; i++) {
        if (!addresses[i]) continue;
        uint32_t translated = ee_lang_string(addresses[i]);
        if (valid) assert(!strcmp(GSTR(translated), GSTR(expected[i])));
        else assert(translated == addresses[i]);
        uint16_t actual[128], wanted[128];
        parse(addresses[i], actual, 1);
        parse(valid ? expected[i] : addresses[i], wanted, 0);
        assert(!memcmp(actual, wanted, sizeof actual));
    }
    /* All five original pause tables must resolve to the active resource. */
    if (valid) for (unsigned table = 0; table < 5; table++)
        for (unsigned i = 0; i < 9; i++)
            assert(!strcmp(GSTR(ee_lang_string(MEM32(0x7ABD84 + table * 36 + i * 4))), GSTR(expected[i])));
    assert(ee_lang_string(0) == 0 && ee_lang_string(0x123456) == 0x123456);
    /* Execute the actual first-pause path with a deliberately null old table. */
    MEM32(0x01DB95DCu) = 0;
    MEM16(0x00943D68u) = 0;
    memset(GPTR(0x00943968u), 0, 1024);
    g_esp = 0x01B00000u;
    sub_00407300();
    assert(g_esp == 0x01B00004u && draws >= 6);
    assert(MEM32(0x01DB95DCu) == 0x007ABD84u);
    for (unsigned i = 0; i < 6; i++) {
        uint16_t wanted[128];
        parse(valid ? expected[i] : addresses[i], wanted, 0);
        /* Compare through the terminator; unused bytes may hold a previous line. */
        unsigned k = 0;
        do { assert(rendered[i][k] == wanted[k]); } while (wanted[k++] != 0xFFFF && k < 128);
        assert(k < 128);
    }
    MEM8(0x01DBC006u) = 1;
    assert(!strcmp(GSTR(ee_lang_string(addresses[0])), "English00"));
    MEM8(0x01DBC006u) = 2;
    assert(ee_lang_string(addresses[0]) == addresses[0]);
    MEM8(0x01DBC006u) = 255;
    assert(ee_lang_string(addresses[0]) == addresses[0]);
    ee_lang_pause();
    assert(MEM32(0x01DB95DCu) == 0x007ABD84u);
    free((void *)g_xbox_mem_offset);
    printf("menu resource/parser/pause/ABI checks passed: %s\n", argv[1]);
    return 0;
}
