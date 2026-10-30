/* Native support for EE's external executable strings (Patches/Langs.cpp).
 * This is an independent implementation for the supported PC v1.0 executable.
 * Translate at the text-parser boundary, not by editing generated C or live x86.
 * Resource bytes are game glyph codes, not UTF-8; preserve them verbatim.
 */
#include "../runtime/rt.h"
#include "ee_lang.h"

#define LANG_ID 0x01DBC006u
#define STR_COUNT 33
#define MAX_LINE 120
#define MAX_FILE 65536

static uint32_t strings[6][STR_COUNT];
static uint32_t pause_original[5][9];
static int initialized;

/* The first nine lines are the pause menu. These are the remaining in-game
 * strings in the PC v1.0 image, indexed as in EE's resource format. Lines 21
 * (keyboard button printf format) and 25+ (EE-only UI) have no use at this
 * game's text-parser boundary and are intentionally not redirected here.
 */
static const struct { uint32_t address; unsigned index; } other[] = {
    {0x007ABE3Cu, 9},
    {0x007ABE64u, 10}, {0x007ABE70u, 10}, {0x007ABE7Cu, 10},
    {0x007ABE88u, 11}, {0x007ABE98u, 12}, {0x007ABEB8u, 13},
    {0x007ABED0u, 14}, {0x007ABEE4u, 15}, {0x007ABEF4u, 16},
    {0x007ABF04u, 17}, {0x007ABF18u, 18},
    {0x007ABF30u, 19}, {0x007ABF38u, 20},
    {0x007F2868u, 22}, {0x007F2878u, 23}, {0x007F2888u, 24},
};

static void load_language(unsigned lang, char suffix) {
    char guest[80], path[512];
    snprintf(guest, sizeof guest, "data/etc/resource/r_menu_%c.res", suffix);
    FILE *f = fopen(rt_path(guest, path, sizeof path), "rb");
    if (!f) return;
    char *data = malloc(MAX_FILE + 1);
    if (!data) { fclose(f); return; }
    size_t size = fread(data, 1, MAX_FILE + 1, f);
    int invalid = ferror(f) || size > MAX_FILE || memchr(data, 0, size) != NULL;
    fclose(f);
    char *lines[STR_COUNT];
    unsigned count = 0;
    if (!invalid) {
        data[size] = 0;
        char *line = data;
        while (*line) {
            char *end = strchr(line, '\n');
            char *next = end ? end + 1 : data + size;
            if (end) *end = 0;
            size_t len = strlen(line);
            if (len && line[len - 1] == '\r') line[--len] = 0;
            if (len && strncmp(line, "//", 2)) {
                /* Newer packs may append EE-only settings; the first 33
                 * entries keep their meaning. Never shift or recode them. */
                if (count < STR_COUNT) {
                    if (len > MAX_LINE) { invalid = 1; break; }
                    lines[count++] = line;
                }
            }
            line = next;
        }
    }
    /* Require entries through quick-save line 24. Reject truncated resources
     * atomically; positions of blank/comment lines follow EE's file format. */
    if (!invalid && count >= 25) {
        for (unsigned i = 0; i < count; i++) strings[lang][i] = gstrdup(lines[i]);
        rt_log("lang: loaded %u executable strings from %s", count, path);
    } else {
        rt_log("lang: ignoring incomplete or invalid executable strings: %s", path);
    }
    free(data);
}

void ee_lang_init(void) {
    if (initialized) return;
    initialized = 1;
    for (unsigned lang = 0; lang < 5; lang++)
        for (unsigned i = 0; i < 9; i++)
            pause_original[lang][i] = MEM32(0x007ABD84u + lang * 36 + i * 4);
    const char suffixes[] = "jefgis";
    for (unsigned lang = 0; lang < 6; lang++) load_language(lang, suffixes[lang]);
}

uint32_t ee_lang_string(uint32_t original) {
    unsigned lang = MEM8(LANG_ID);
    if (!original || lang >= 6 || !strings[lang][0]) return original;
    for (unsigned table = 0; table < 5; table++)
        for (unsigned i = 0; i < 9; i++)
            if (pause_original[table][i] == original) return strings[lang][i];
    for (unsigned i = 0; i < sizeof other / sizeof *other; i++)
        if (other[i].address == original) return strings[lang][other[i].index];
    return original;
}

void ee_lang_pause(void) {
    /* Original selector only assigns a table for slots 1..5. Give slot 0 a
     * valid source table even before the first pause; the parser hook below
     * supplies slot 0's translations. Missing resources fall back to English.
     * Slots 1..5 retain the original selector and can change at run time. */
    unsigned lang = MEM8(LANG_ID);
    if (lang == 0 || lang > 5) MEM32(0x01DB95DCu) = 0x007ABD84u;
}
