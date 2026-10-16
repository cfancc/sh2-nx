# Chinese language adaptation (v1.00-zh1)

This fork branch preserves the local Chinese adaptation built from upstream
`v1.00` (`7a56503f453ef05975cb5bfac5bcdb9d6db20a3b`). It is not an upstream
release. The later upstream `main` logging changes are not part of this baseline.

## What changes

- Existing files under `lang/` take priority over `sh2e/` and `data/` resources.
  Missing language files retain the upstream fallback behavior.
- When the selected language is Japanese (slot 0), the pause-menu request for
  `r_menu_e.res` uses an available `lang/etc/resource/r_menu_j.res`. The tested
  Simplified Chinese pack replaces that language slot.
- A true-color TGA font sheet and its optional 448-byte `fontwdata.bin` are loaded
  through that resource path. Both raw and RLE TGA, 24/32-bit pixels and image
  origins are handled. Repeat packets advance the input cursor once per packet.
- Each glyph is reduced on the CPU into the game's original texture slots before
  texture upload, preserving the game's text coordinates and dimensions.
- The local NRO version is `1.00-zh1` to distinguish it from the official build.

No game executable, translation, font artwork, save, controller configuration or
compiled binary is included. Supply your own files and follow the upstream build
instructions in the README.

## Using your language pack

Place the pack's `lang/` alongside the NRO, preserving its directory structure.
The tested Chinese pack needs `SET DX_CONFIG_LANGUAGE 0` in `language.ini`.

This adaptation reads `lang/font/font000.tga`, not PNG. If the pack supplies a PNG,
convert that exact image (with its alpha channel and full size) using Pillow:

```sh
.venv/bin/python tools/convert_ee_font.py /path/to/lang/font/font000.png /path/to/lang/font/font000.tga
```

Keep the matching `fontwdata.bin` from the same pack. Do not reuse a different
language pack's TGA. The tested sheet is 4400 x 9600, with 44 x 64 cells and two
equally sized style halves; its decoded BGRA buffer uses 168,960,000 bytes.
Other font layouts have not been validated. This is a language-pack adaptation,
not a general Unicode text renderer. Normal title-takeover memory requirements
still apply.

## Validation and limits

The original v1.00-zh1 source was fully compiled with devkitPro's devkita64 image
`sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282`.
NRO SHA-256:
`211e313a1a56e1f53c142bbef1537a74347b1118237bb0f9aa7f6f104a6ee9e8`.

Offline checks passed: all 42,240,000 decoded pixels match the source PNG;
six representative glyph outputs match the older ARM64 rasterizer; width-table
loading, malformed RLE rejection, resource priority, menu mapping, case matching
and save-path fallback were checked. These checks do not establish full game or
hardware compatibility. The new v1.00-zh1 build still needs Switch testing.

The native checks are retained in `tests/local/`. After `tools/regen.sh` has
generated the runtime headers, run from the repository root on Linux:

```sh
mkdir -p build
python3 tests/local/prepare_path_fixtures.py
cc -O2 -ffunction-sections -fdata-sections -Isrc/recomp/gen tests/local/path_driver.c src/win32/kernel32.c -Wl,--gc-sections -pthread -lm -o build/path-test
./build/path-test
# Requires your own tested 4400 x 9600 sheet and matching width table in game/lang/font/.
cc -O2 -Wall -Wextra -Werror -Isrc/recomp/gen tests/local/font_driver.c -o build/font-test
./build/font-test
```

The font check writes decoded pixels and six glyph fixtures under `build/` for
local comparison; it does not include or download font assets. The source
converter requires Pillow, already listed in `requirements.txt`.

## Following upstream

Keep the fork's `main` for upstream synchronization and this branch for the
v1.00 adaptation. For a new release, create a branch at its tag, carry over the
small adaptation commits, resolve conflicts and rebuild/test before deployment.
The Japanese-slot menu special case and font layout assumptions need review
before proposing a generic language-pack feature upstream.
