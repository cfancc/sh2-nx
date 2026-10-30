# Chinese compatibility build (v1.02-zh1)

Based on upstream `v1.02`, commit `95ae2f16efc09c82f405f04383a27d98c2fa804a`.
Windows build and packaging instructions: [MAINTENANCE_ZH.md](MAINTENANCE_ZH.md).

## Font and resource paths

Upstream now supplies `lang/` precedence and native PNG fonts. This branch keeps
that implementation, including the larger 44 x 64 glyph cache and the new
launcher, options, rendering, audio and thread behavior.

The tested Simplified Chinese pack occupies Japanese language slot 0:
`SET DX_CONFIG_LANGUAGE 0` in `language.ini`. Its PNG is 4400 x 9600 and its
matching `fontwdata.bin` is 448 bytes. Keep the original PNG and width table.
Width-table writes skip null or overflowing guest pointers.
The Chinese pause resource is selected from `lang/etc/resource/r_menu_j.res`
when slot 0 requests the English-named pause resource.

The older CPU TGA renderer is retained as a fallback. `ee_init()` first calls
upstream `ee_fonts_init()` and initializes the TGA renderer only if no PNG font
was loaded. Thus the legacy texture overlay remains inactive when the upstream
PNG/HD path succeeds. The fallback uses 44 x 64 source cells reduced into the
original 22 x 32 cache cells, with correct raw/RLE TGA decoding.

To regenerate a fallback TGA from your own matching PNG:

```sh
.venv/bin/python tools/convert_ee_font.py /path/to/lang/font/font000.png /path/to/lang/font/font000.tga
```

Do not reuse the 4400 x 896 TGA from an older Russian pack with the Chinese PNG.
No game executable, translation, font artwork, saves or compiled binaries are
tracked in this repository.

## Validation

The v1.02-zh1 NRO builds with the pinned devkitPro image used by
`tools/build_zh.ps1`. Full generation reports no failed functions; unresolved
targets are reported by the upstream generator and remain a runtime limitation.

The existing native tests check language priority, Japanese-slot pause resources,
case matching, save paths, TGA decoding, width tables and fallback glyphs.
`tests/local/png_font_driver.c` includes the current font implementation extracted
from `src/game/ee.c` by `check_png_font.py prepare`. It exercises actual upstream
FFmpeg PNG decoding, width tables, the HD cache, normal/small glyphs, out-of-range
glyph fallback and the original-font path. Pillow independently checks all
42,240,000 PNG pixels and seven glyph fixtures.

The additional PNG native check needs host `libavcodec-dev` and `libavutil-dev`
in a Linux test container, independently of the Switch cross-compiler libraries:

```sh
.venv/bin/python tests/local/check_png_font.py prepare
cc -O2 -Wall -Wextra -Werror -Ibuild -Isrc/recomp/gen tests/local/png_font_driver.c -lavcodec -lavutil -o build/png-font-test
./build/png-font-test
.venv/bin/python tests/local/check_png_font.py check
```

Private test inputs belong in `game/lang/font/`: the Chinese PNG, matching TGA
and width table. Generated code, test outputs, dependencies and game inputs are
ignored by Git. The PNG checker removes its large pixel fixtures after success.

Compilation and offline checks do not establish Switch compatibility. The
v1.02-zh1 build still needs hardware testing, especially text, the new launcher,
controls, saves and dialogue audio.
