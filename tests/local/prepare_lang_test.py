"""Extract live generated parser/pause functions; private fixtures stay in build/."""
from pathlib import Path
import re

root = Path('build/lang-fixtures')
original = Path('game/lang/etc/resource/r_menu_j.res').read_bytes()
lines = [x for x in original.splitlines() if x and not x.startswith(b'//')]
assert len(lines) >= 25
fixtures = {
    'valid': original,
    'lf': b'// fixture comment\n\n' + b'\n'.join(lines),
    'short': b'\n'.join(lines[:8]),
    'embedded_nul': original + b'\0',
    'long': b'x' * 121 + b'\n' + b'\n'.join(lines[1:]),
    'oversize': b'x' * 65537,
}
for name in [*fixtures, 'missing']:
    folder = root / name / 'lang/etc/resource'
    folder.mkdir(parents=True, exist_ok=True)
    if name in fixtures:
        (folder / 'r_menu_j.res').write_bytes(fixtures[name])
    # A distinct English resource catches accidental j/e aliasing on slot 0.
    (folder / 'r_menu_e.res').write_bytes(b'\n'.join(b'English%02d' % i for i in range(33)))
(root / 'expected.bin').write_bytes(b'\0'.join(lines[:33]) + b'\0')

pattern = r'void (sub_[0-9A-Fa-f]+(?:_gen)?)\(void\)\s*\{.*?\n\}'
functions = {}
for path in Path('src/recomp/gen').glob('recomp_*.c'):
    for match in re.finditer(pattern, path.read_text(), re.S):
        functions[match[1]] = match[0]
manual_source = Path('src/game/ee.c').read_text()
for address in ('00407300', '004829F0'):
    assert f'case 0x{address}u: return sub_{address};' in manual_source
manual = {m[1]: m[0] for m in re.finditer(pattern, manual_source, re.S)}
stubs = {'sub_0044C630', 'sub_0047E900', 'sub_0047EE00', 'sub_0047EE10',
         'sub_00480500', 'sub_0056FD66'}
required = ['sub_00407300_gen', 'sub_004829F0_gen']
selected = {name: manual[name] for name in ('sub_00407300', 'sub_004829F0')}
while required:
    name = required.pop()
    if name in selected or name in stubs:
        continue
    selected[name] = functions[name]
    required.extend(set(re.findall(r'\bsub_[0-9A-Fa-f]+(?:_gen)?\b', functions[name])) - {name})
Path('build/lang-under-test.inc').write_text('\n\n'.join(selected.values()))
print(f'Prepared {len(selected)} live functions and {len(fixtures) + 1} resource cases')
