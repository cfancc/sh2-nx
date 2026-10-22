/* Before the game: the warning screen (assets/splash.png), then a launcher with Play and Options, where Options
 * edits sh2e.ini. The images are built into the binary. Drawn on the CPU into a 1280x720 canvas that is blitted to
 * the window, so it needs no shaders and leaves the game a fresh GL state. */
#include "d3d8.h"
#include <SDL2/SDL.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <math.h>

#define FADE_MS 700  /* every screen fades in and out */
#define CW 1280
#define CH 720

#define ASSET(name, path) \
    __asm__(".section .rodata\n.balign 16\n.global " #name "_png\n" #name "_png:\n.incbin \"" SRC_DIR "/" path "\"\n" \
            ".global " #name "_png_end\n" #name "_png_end:\n.previous"); \
    extern const uint8_t name##_png[], name##_png_end[];
ASSET(splash, "assets/splash.png")
ASSET(logo, "docs/banner.png")  /* the README's */
ASSET(font, "assets/font.png")  /* tools/font_atlas.py: ASCII 32-126, 16x6 cells of 40x80, baseline at 60, 36 px advance */

/* ---- the game's menu sounds: entries of the effect table at 0x8A67DC (offsets into data/sound/sddata.bin) ---- */

#define SFX_TABLE 0x008A67DCu
enum { SND_CURSOR, SND_OK };
static SDL_AudioDeviceID snd_dev;
static struct { Uint8 *pcm; Uint32 len; } snd[2];

/* One RIFF of a sound bank, scaled by gain; with want_bytes, only if its sample data is that long. */
static int load_riff(FILE *f, uint32_t off, uint32_t want_bytes, float gain, SDL_AudioSpec *spec, int i) {
    uint8_t hdr[8], *riff;
    uint32_t n;
    if (!f || fseek(f, off, SEEK_SET) || fread(hdr, 1, 8, f) != 8 || memcmp(hdr, "RIFF", 4)) return 0;
    memcpy(&n, hdr + 4, 4);
    if (n > (1u << 20) || !(riff = malloc(n + 8))) return 0;
    memcpy(riff, hdr, 8);
    int ok = fread(riff + 8, 1, n, f) == n && SDL_LoadWAV_RW(SDL_RWFromConstMem(riff, n + 8), 1, spec, &snd[i].pcm, &snd[i].len);
    free(riff);
    if (ok && ((want_bytes && snd[i].len != want_bytes) || spec->format != AUDIO_S16LSB)) {
        SDL_FreeWAV(snd[i].pcm);
        snd[i].pcm = NULL;
        ok = 0;
    }
    for (Uint32 k = 0; ok && k < snd[i].len / 2; k++) ((int16_t *)snd[i].pcm)[k] = (int16_t)(((int16_t *)snd[i].pcm)[k] * gain);
    return ok;
}

static void sound_init(void) {
    /* What the title menu plays on a move and a choice: effect table entries 292 and 295, which are RIFFs 291
     * and 294 of the Enhanced Edition bank (48 kHz) at these offsets, if it is that version of the pack. */
    static const struct { int id; uint32_t ee_off, ee_bytes; } fx[2] = {{292, 47286718, 85286}, {295, 47632428, 102356}};
    /* the game's level: sound effect volume 12 (-1.98 dB), head-relative 3D at the listener (0.707 a side) */
    const float gain = 0.796f * 0.707f;
    SDL_AudioSpec spec;
    FILE *f = fopen("sh2e/sound/sddata.bin", "rb");
    int n = 0;
    for (int i = 0; i < 2; i++) n += load_riff(f, fx[i].ee_off, fx[i].ee_bytes, gain, &spec, i);
    if (f) fclose(f);
    if (n < 2) {  /* the original bank (16 kHz): its offsets are in the exe's table */
        for (int i = 0; i < 2; i++) { SDL_FreeWAV(snd[i].pcm); snd[i].pcm = NULL; }
        f = fopen("data/sound/sddata.bin", "rb");
        for (int i = 0; i < 2; i++) load_riff(f, MEM32(SFX_TABLE + 4 * fx[i].id), 0, gain, &spec, i);
        if (f) fclose(f);
    }
    if (!snd[SND_CURSOR].pcm || !snd[SND_OK].pcm) { rt_log("launcher: no menu sounds in sddata.bin"); return; }
    if ((snd_dev = SDL_OpenAudioDevice(NULL, 0, &spec, NULL, 0))) SDL_PauseAudioDevice(snd_dev, 0);  /* one format: same bank */
    else rt_log("launcher: audio: %s", SDL_GetError());
}

static void sound(int i) {
    if (!snd_dev) return;
    SDL_ClearQueuedAudio(snd_dev);
    SDL_QueueAudio(snd_dev, snd[i].pcm, snd[i].len);
}

static void sound_close(void) {
    for (Uint64 t0 = SDL_GetTicks64(); snd_dev && SDL_GetQueuedAudioSize(snd_dev) && SDL_GetTicks64() - t0 < 1000;) SDL_Delay(10);
    if (snd_dev) SDL_CloseAudioDevice(snd_dev);
    for (int i = 0; i < 2; i++) SDL_FreeWAV(snd[i].pcm);
}

/* Decodes a PNG to RGBA rows; NULL if it can't. */
static uint32_t *decode(const uint8_t *png, const uint8_t *end, int *w, int *h) {
    int n = (int)(end - png);
    uint8_t *buf = calloc(1, n + AV_INPUT_BUFFER_PADDING_SIZE), *px = NULL;
    const AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_PNG);
    AVCodecContext *ctx = codec ? avcodec_alloc_context3(codec) : NULL;
    AVPacket *pkt = av_packet_alloc();
    AVFrame *fr = av_frame_alloc();
    if (buf && ctx && pkt && fr && avcodec_open2(ctx, codec, NULL) == 0) {
        memcpy(buf, png, n);
        pkt->data = buf;
        pkt->size = n;
        if (avcodec_send_packet(ctx, pkt) == 0 && avcodec_receive_frame(ctx, fr) == 0) {
            *w = fr->width;
            *h = fr->height;
            struct SwsContext *sws = sws_getContext(*w, *h, fr->format, *w, *h, AV_PIX_FMT_RGBA, SWS_POINT, NULL, NULL, NULL);
            if (sws && (px = malloc((size_t)*w * *h * 4))) {
                int stride = *w * 4;
                sws_scale(sws, (const uint8_t *const *)fr->data, fr->linesize, 0, *h, &px, &stride);
            }
            sws_freeContext(sws);
        }
    }
    av_frame_free(&fr);
    av_packet_free(&pkt);
    avcodec_free_context(&ctx);
    free(buf);
    return (uint32_t *)px;
}

static GLuint tex, fbo;
static int tex_w, tex_h;

/* Uploads w x h RGBA rows (row 0 at the top) and shows them letterboxed. */
static void present(const uint32_t *px, int w, int h) {
    if (w != tex_w || h != tex_h) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
        tex_w = w;
        tex_h = h;
    } else
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px);
    int ww, wh;
    SDL_GL_GetDrawableSize(rt_window, &ww, &wh);
    glClear(GL_COLOR_BUFFER_BIT);
    /* the texture's row 0 is the image's top, the window's row 0 its bottom */
    float s = fminf((float)ww / w, (float)wh / h);
    int dw = w * s, dh = h * s, dx = (ww - dw) / 2, dy = (wh - dh) / 2;
    glBlitFramebuffer(0, 0, w, h, dx, dy + dh, dx + dw, dy, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    SDL_GL_SwapWindow(rt_window);
}

/* ---- the canvas ---- */

static uint32_t canvas[CW * CH];
static uint32_t *font_px;
static int font_w;

static uint32_t rgb(int r, int g, int b) { return 0xFF000000u | b << 16 | g << 8 | r; }  /* RGBA bytes */

/* Text at x (left edge) and baseline y; size = glyph height in px (80 = the atlas'), sampled bilinearly. */
static int text(int x, int y, int size, uint32_t color, const char *s) {
    float k = size / 80.0f;
    int adv = (int)(36 * k + 0.5f), cw = (int)(40 * k + 0.5f), ch = size, top = y - (int)(60 * k);
    for (; *s; s++, x += adv) {
        int c = (unsigned char)*s - 32;
        if (c <= 0 || c >= 95 || !font_px) continue;
        int gx = c % 16 * 40, gy = c / 16 * 80;
        for (int py = 0; py < ch; py++) {
            int oy = top + py;
            if (oy < 0 || oy >= CH) continue;
            float fy = (py + 0.5f) / k - 0.5f;
            int y0 = (int)fy;
            float ty = fy - y0;
            if (y0 < 0) y0 = 0, ty = 0;
            if (y0 >= 79) y0 = 78, ty = 1;
            for (int px = 0; px < cw; px++) {
                int ox = x + px;
                if (ox < 0 || ox >= CW) continue;
                float fx = (px + 0.5f) / k - 0.5f;
                int x0 = (int)fx;
                float tx = fx - x0;
                if (x0 < 0) x0 = 0, tx = 0;
                if (x0 >= 39) x0 = 38, tx = 1;
                const uint32_t *r0 = font_px + (size_t)(gy + y0) * font_w + gx + x0, *r1 = r0 + font_w;
                float a = ((r0[0] >> 24) * (1 - tx) + (r0[1] >> 24) * tx) * (1 - ty) +
                          ((r1[0] >> 24) * (1 - tx) + (r1[1] >> 24) * tx) * ty;
                if (a < 1) continue;
                uint32_t *d = &canvas[oy * CW + ox], o = 0xFF000000u;
                for (int sh = 0; sh < 24; sh += 8) {
                    float v = ((*d >> sh) & 0xFF) + (((color >> sh) & 0xFF) - (float)((*d >> sh) & 0xFF)) * a / 255;
                    o |= (uint32_t)v << sh;
                }
                *d = o;
            }
        }
    }
    return x;
}
/* An RGBA image scaled bilinearly into the canvas rect x, y, dw, dh, blended by its alpha. */
static void image(const uint32_t *img, int w, int h, int x, int y, int dw, int dh) {
    for (int py = 0; py < dh; py++) {
        float fy = (py + 0.5f) * h / dh - 0.5f;
        int y0 = fy < 0 ? 0 : (int)fy, y1 = y0 + 1 < h ? y0 + 1 : y0;
        float ty = fy < 0 ? 0 : fy - y0;
        for (int px = 0; px < dw; px++) {
            float fx = (px + 0.5f) * w / dw - 0.5f;
            int x0 = fx < 0 ? 0 : (int)fx, x1 = x0 + 1 < w ? x0 + 1 : x0;
            float tx = fx < 0 ? 0 : fx - x0;
            const uint32_t q[4] = {img[y0 * w + x0], img[y0 * w + x1], img[y1 * w + x0], img[y1 * w + x1]};
            float k[4] = {(1 - tx) * (1 - ty), tx * (1 - ty), (1 - tx) * ty, tx * ty}, a = 0, c[3] = {0};
            for (int i = 0; i < 4; i++) {
                float qa = (q[i] >> 24) * k[i];
                a += qa;
                for (int j = 0; j < 3; j++) c[j] += ((q[i] >> 8 * j) & 0xFF) * qa;
            }
            if (a < 1) continue;
            uint32_t *d = &canvas[(y + py) * CW + x + px], o = 0xFF000000u;
            for (int j = 0; j < 3; j++) {
                float dv = (*d >> 8 * j) & 0xFF;
                o |= (uint32_t)(dv + (c[j] / a - dv) * a / 255) << 8 * j;
            }
            *d = o;
        }
    }
}

static int text_w(int size, const char *s) { return (int)strlen(s) * (int)(36 * size / 80.0f + 0.5f); }
static void text_c(int y, int size, uint32_t color, const char *s) { text((CW - text_w(size, s)) / 2, y, size, color, s); }

/* ---- sh2e.ini ---- */

typedef struct {
    const char *label, *key;
    const char *vals[4], *names[4];  /* NULL-terminated; vals[def] is what the game uses without the key */
    int def, cur, was;
    char other[32];  /* a value the file has that the list doesn't: shown until changed */
} Opt;

static Opt opts[] = {
    {"Resolution", "resolution", {"1920x1080", "1280x720", "960x720"}, {"1080p", "720p (lighter)", "4:3 (original)"}},
    {"Handheld render", "handheld", {"720", "480", "0"}, {"720p", "480p (faster)", "Game resolution"}},
    {"Docked render", "docked", {"0", "1080", "720"}, {"Game resolution", "1080p", "720p (faster)"}},
    {"Movies", "movie", {"0", "1"}, {"Original", "Enhanced (1080p60)"}},
    {"EE load/save sounds", "PreserveSoundsOnLoad", {"1", "0"}, {"On", "Off"}},
    {"Game threads", "cores", {"3", "1"}, {"Three cores", "One core"}},
};
#define N_OPTS (int)(sizeof opts / sizeof *opts)

static int n_vals(const Opt *o) { int n = 0; while (n < 4 && o->vals[n]) n++; return n; }
static const char *opt_name(const Opt *o) { return o->cur < 0 ? o->other : o->names[o->cur]; }

/* The value after "key =" on an uncommented line, as the game's own parsers (sscanf " key = ") read it. */
static int ini_value(const char *line, const char *key, char *val) {
    char fmt[64];
    snprintf(fmt, sizeof fmt, " %s = %%31[^ \t\r\n;]", key);
    return sscanf(line, fmt, val) == 1;
}

static void ini_load(void) {
    FILE *f = fopen("sh2e.ini", "r");
    char line[256], val[32];
    for (int i = 0; i < N_OPTS; i++) opts[i].cur = opts[i].def;
    while (f && fgets(line, sizeof line, f))
        for (int i = 0; i < N_OPTS; i++)
            if (ini_value(line, opts[i].key, val)) {
                Opt *o = &opts[i];
                o->cur = -1;
                snprintf(o->other, sizeof o->other, "%s", val);
                for (int j = 0; j < n_vals(o); j++)
                    if (!strcmp(o->vals[j], val)) o->cur = j;
            }
    if (f) fclose(f);
    for (int i = 0; i < N_OPTS; i++) opts[i].was = opts[i].cur;
}

/* Rewrites the changed keys in place (the last line that sets each, as that's the one the game ends up
 * with) and appends the ones the file lacks; comments and every other line stay as they are. */
static void ini_save(void) {
    char *lines[512];
    int n = 0;
    FILE *f = fopen("sh2e.ini", "r");
    char line[256], val[32];
    while (f && n < 512 && fgets(line, sizeof line, f)) lines[n++] = strdup(line);
    if (f) fclose(f);
    if (!(f = fopen("sh2e.ini", "w"))) { rt_log("launcher: can't write sh2e.ini"); return; }
    for (int i = 0; i < N_OPTS; i++) {
        Opt *o = &opts[i];
        if (o->cur == o->was) continue;
        int last = -1;
        for (int j = 0; j < n; j++)
            if (ini_value(lines[j], o->key, val)) last = j;
        char set[96];
        snprintf(set, sizeof set, "%s=%s\n", o->key, o->vals[o->cur]);
        if (last >= 0) { free(lines[last]); lines[last] = strdup(set); }
        else if (n < 512) {
            if (n && !strchr(lines[n - 1], '\n')) {  /* a last line without its newline */
                lines[n - 1] = realloc(lines[n - 1], strlen(lines[n - 1]) + 2);
                strcat(lines[n - 1], "\n");
            }
            lines[n++] = strdup(set);
        }
        o->was = o->cur;
    }
    for (int j = 0; j < n; j++) { fputs(lines[j], f); free(lines[j]); }
    fclose(f);
}

/* ---- input ---- */

enum { IN_NONE, IN_UP, IN_DOWN, IN_LEFT, IN_RIGHT, IN_OK, IN_BACK };

static int poll_input(void) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_QUIT: exit(0);
        case SDL_CONTROLLERDEVICEADDED: SDL_GameControllerOpen(e.cdevice.which); break;
        case SDL_KEYDOWN:
            switch (e.key.keysym.sym) {
            case SDLK_UP: return IN_UP;
            case SDLK_DOWN: return IN_DOWN;
            case SDLK_LEFT: return IN_LEFT;
            case SDLK_RIGHT: return IN_RIGHT;
            case SDLK_RETURN: case SDLK_SPACE: return IN_OK;
            case SDLK_ESCAPE: case SDLK_BACKSPACE: return IN_BACK;
            }
            break;
        case SDL_CONTROLLERBUTTONDOWN:
            switch (e.cbutton.button) {
            case SDL_CONTROLLER_BUTTON_DPAD_UP: return IN_UP;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return IN_DOWN;
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return IN_LEFT;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return IN_RIGHT;
#ifdef __SWITCH__  /* Nintendo's way round, as in the game (dinput.c): confirm on A (east), back on B (south) */
            case SDL_CONTROLLER_BUTTON_B: return IN_OK;
            case SDL_CONTROLLER_BUTTON_A: return IN_BACK;
#else
            case SDL_CONTROLLER_BUTTON_A: return IN_OK;
            case SDL_CONTROLLER_BUTTON_B: return IN_BACK;
#endif
            case SDL_CONTROLLER_BUTTON_START: return IN_OK;
            }
            break;
        case SDL_CONTROLLERAXISMOTION: {  /* the left stick, once per push past half way */
            static int held[2];
            int ax = e.caxis.axis;
            if (ax != SDL_CONTROLLER_AXIS_LEFTX && ax != SDL_CONTROLLER_AXIS_LEFTY) break;
            int dir = e.caxis.value > 16000 ? 1 : e.caxis.value < -16000 ? -1 : 0, was = held[ax];
            held[ax] = dir;
            if (dir && dir != was) return ax == SDL_CONTROLLER_AXIS_LEFTX ? (dir > 0 ? IN_RIGHT : IN_LEFT) : (dir > 0 ? IN_DOWN : IN_UP);
            break;
        }
        }
    }
    return IN_NONE;
}

/* ---- screens ---- */

#define C_BG rgb(8, 8, 10)
#define C_TEXT rgb(200, 200, 200)
#define C_DIM rgb(110, 110, 110)
#define C_SEL rgb(220, 40, 30)

static uint32_t *logo_px;
static int logo_w, logo_h;

static void draw_menu(int sel) {
    static const char *items[] = {"Play", "Options"};
    for (int i = 0; i < CW * CH; i++) canvas[i] = C_BG;
    if (logo_px) image(logo_px, logo_w, logo_h, (CW - 800) / 2, 90, 800, 800 * logo_h / logo_w);
    text_c(360, 32, C_DIM, "sh2-nx");
    for (int i = 0; i < 2; i++) text_c(470 + i * 70, 48, i == sel ? C_SEL : C_TEXT, items[i]);
    text_c(680, 24, C_DIM, "A: select");
}

static void draw_options(int sel) {
    for (int i = 0; i < CW * CH; i++) canvas[i] = C_BG;
    text_c(110, 56, C_TEXT, "Options");
    for (int i = 0; i <= N_OPTS; i++) {
        int y = 190 + i * 56;
        uint32_t c = i == sel ? C_SEL : C_TEXT;
        if (i == N_OPTS) { text_c(y + 20, 36, c, "Back"); break; }
        text(140, y, 36, c, opts[i].label);
        char v[64];
        snprintf(v, sizeof v, i == sel ? "< %s >" : "%s", opt_name(&opts[i]));
        text(CW - 140 - text_w(36, v), y, 36, c, v);
    }
    text_c(690, 24, C_DIM, "Left/Right: change   B: back (saves sh2e.ini)");
}

/* Fades the canvas in from black, or out to black, then leaves it as it was. Input meanwhile is dropped. */
static void fade(int in) {
    static uint32_t pic[CW * CH];
    memcpy(pic, canvas, sizeof pic);
    for (Uint64 t0 = SDL_GetTicks64(), t = 0; t < FADE_MS;) {
        poll_input();
        t = SDL_GetTicks64() - t0;
        uint32_t k = t >= FADE_MS ? 256 : (uint32_t)(t * 256 / FADE_MS);
        if (!in) k = 256 - k;
        for (int i = 0; i < CW * CH; i++)
            canvas[i] = 0xFF000000u | ((pic[i] & 0xFF00FFu) * k >> 8 & 0xFF00FFu) | ((pic[i] & 0xFF00u) * k >> 8 & 0xFF00u);
        present(canvas, CW, CH);
    }
    memcpy(canvas, pic, sizeof pic);
}

static void options(void) {
    int sel = 0;
    draw_options(sel);
    fade(1);
    for (int dirty = 0;; ) {
        if (dirty) { draw_options(sel); dirty = 0; }
        present(canvas, CW, CH);
        int in = poll_input();
        if (in == IN_NONE) continue;
        dirty = 1;
        Opt *o = sel < N_OPTS ? &opts[sel] : NULL;
        if (in != IN_BACK) sound(!o && in == IN_OK ? SND_OK : SND_CURSOR);
        if (in == IN_UP) sel = (sel + N_OPTS) % (N_OPTS + 1);
        if (in == IN_DOWN) sel = (sel + 1) % (N_OPTS + 1);
        if (o && (in == IN_LEFT || in == IN_RIGHT || in == IN_OK)) {
            int n = n_vals(o);
            o->cur = o->cur < 0 ? 0 : (o->cur + (in == IN_LEFT ? n - 1 : 1)) % n;
        }
        if (in == IN_BACK || (!o && in == IN_OK)) {
            ini_save();
            fade(0);
            return;
        }
    }
}

/* A warning screen: wait_ms after it has faded in, "Press A" fades in, and the screen waits for it. */
static void splash(const uint8_t *png, const uint8_t *end, Uint64 wait_ms) {
    int w, h;
    uint32_t *px = decode(png, end, &w, &h);
    if (!px) { rt_log("launcher: can't decode a splash image"); return; }
    for (int i = 0; i < CW * CH; i++) canvas[i] = rgb(0, 0, 0);
    int dw = CW, dh = CW * h / w;
    if (dh > CH) dh = CH, dw = CH * w / h;
    image(px, w, h, (CW - dw) / 2, (CH - dh) / 2, dw, dh);
    free(px);
    fade(1);
    Uint64 t0 = SDL_GetTicks64();
    while (SDL_GetTicks64() - t0 < wait_ms) { poll_input(); present(canvas, CW, CH); }
    static uint32_t pic[CW * CH];
    memcpy(pic, canvas, sizeof pic);
    for (Uint64 t1 = SDL_GetTicks64(), t = 0; poll_input() != IN_OK || t < FADE_MS;) {
        t = SDL_GetTicks64() - t1;
        int k = t >= FADE_MS ? 256 : (int)(t * 256 / FADE_MS);  /* the text's colour from black up */
        memcpy(canvas, pic, sizeof pic);
        text_c(CH - 40, 32, rgb(235 * k >> 8, 235 * k >> 8, 235 * k >> 8), "Press A");  /* the picture's corners are taken */
        present(canvas, CW, CH);
    }
    sound(SND_OK);
    fade(0);
}

void launcher_run(void) {
    /* Scripted runs (SH2_KEYS) go straight to the game; SH2_LAUNCHER=1 keeps the launcher for testing it. */
    if (getenv("SH2_KEYS") && !getenv("SH2_LAUNCHER")) return;
    rt_window_open();
    gl_context_open();
    int fh;
    font_px = decode(font_png, font_png_end, &font_w, &fh);
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    glClearColor(0, 0, 0, 1);
    logo_px = decode(logo_png, logo_png_end, &logo_w, &logo_h);
    sound_init();
    splash(splash_png, splash_png_end, 4000);
    ini_load();
    draw_menu(0);
    fade(1);
    for (int sel = 0, dirty = 0;; ) {
        if (dirty) { draw_menu(sel); dirty = 0; }
        present(canvas, CW, CH);
        int in = poll_input();
        if (in == IN_UP || in == IN_DOWN) sel ^= 1, dirty = 1, sound(SND_CURSOR);
        if (in == IN_OK) sound(SND_OK);
        if (in == IN_OK && sel == 0) { fade(0); break; }
        if (in == IN_OK && sel == 1) {
            fade(0);
            options();
            draw_menu(sel);
            fade(1);
        }
    }
    /* leave GL as the game expects a new context: framebuffer 0, nothing bound */
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &tex);
    free(font_px);
    free(logo_px);
    sound_close();
}
