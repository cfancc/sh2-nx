#include "../../src/runtime/rt.h"
#include <assert.h>
#include <unistd.h>
#include <stdarg.h>
#include <strings.h>

ptrdiff_t g_xbox_mem_offset;
int rt_trace;
void rt_log(const char *fmt, ...) {(void)fmt;}
static void check(const char *input,const char *expected) {
    char out[512];
    rt_path(input,out,sizeof out);
    printf("%s -> %s\n",input,out);
    /* FAT/Windows bind mounts may preserve the caller's case on successful stat. */
    assert(!strcasecmp(out,expected));
}
int main(void) {
    assert(chdir("build/path-fixtures")==0);
    g_xbox_mem_offset=(ptrdiff_t)calloc(1,0x02000000u); assert(g_xbox_mem_offset);
    MEM8(0x01DBC006)=0;
    check("DATA\\Etc\\Message\\option_msg_j.mes","lang/etc/message/option_msg_j.mes");
    check("data/etc/message/common_msg_j.mes","sh2e/etc/message/common_msg_j.mes");
    check("data/etc/message/item_msg_j.mes","data/etc/message/item_msg_j.mes");
    check("data/etc/resource/r_menu_e.res","lang/etc/resource/r_menu_j.res");
    check("data/pic/etc/start01j.tex","lang/pic/etc/start01j.tex");
    MEM8(0x01DBC006)=1;
    check("data/etc/resource/r_menu_e.res","lang/etc/resource/r_menu_e.res");
    check("data/save/test.dat","data/save/test.dat");
    free((void *)g_xbox_mem_offset);
    puts("language precedence, pause resource, case matching and save-path checks passed");
    return 0;
}
