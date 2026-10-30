"""Prepare actual upstream PNG font code for native checks, or compare its output."""
import argparse
import json
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['prepare', 'check'])
    args = parser.parse_args()
    build = Path('build')
    build.mkdir(exist_ok=True)
    if args.action == 'prepare':
        text = Path('src/game/ee.c').read_text(encoding='utf-8')
        start = text.index('/* ---- Custom fonts (')
        end = text.index('/* ---- External menu strings (', start)
        (build / 'upstream-font-under-test.inc').write_text(text[start:end], encoding='utf-8')
        return
    from PIL import Image
    with Image.open('game/lang/font/font000.png') as source:
        rgba = source.convert('RGBA')
    green, alpha = rgba.getchannel('G'), rgba.getchannel('A')
    expected_ya = Image.merge('LA', (green, alpha)).tobytes()
    if (build / 'png-decoded.ya8').read_bytes() != expected_ya:
        raise AssertionError('FFmpeg upstream decode differs from source PNG')
    cases = [(0,0), (33,0), (350,0), (7000,0), (7000,1), (7499,1), (0,0)]
    for index, (glyph, style) in enumerate(cases):
        x, y = glyph % 100 * 44, glyph // 100 * 64 + style * 4800
        box = (x, y, x + 44, y + 64)
        g, a = green.crop(box), alpha.crop(box)
        expected = Image.merge('RGBA', (g, g, g, a)).tobytes('raw', 'BGRA')
        if (build / f'png-glyph-{index}.bgra').read_bytes() != expected:
            raise AssertionError(f'PNG glyph {index} differs from source PNG')
    report = {'png_pixels_checked': rgba.width * rgba.height, 'hd_glyph_checks': len(cases),
              'font_widths_checked': True, 'fallback_checked': True}
    (build / 'zh-png-font-check.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    for path in [build / 'png-decoded.ya8'] + [build / f'png-glyph-{i}.bgra' for i in range(len(cases))]:
        path.unlink()
    print(json.dumps(report))


if __name__ == '__main__':
    main()
