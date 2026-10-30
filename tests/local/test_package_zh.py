"""Small synthetic fixtures exercise the release packager without game assets."""
import contextlib
import hashlib
import importlib.util
import io
from pathlib import Path
import struct
import tempfile
from types import SimpleNamespace
import unittest
import zipfile

spec = importlib.util.spec_from_file_location('package_zh', Path(__file__).resolve().parents[2] / 'tools/package_zh.py')
package_zh = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package_zh)


class PackagingTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='sh2-zh-test-')
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.nro = self.root / 'fixture.nro'
        self.write_nro('1.00-zh1')
        self.lang = self.root / 'lang'
        (self.lang / 'font').mkdir(parents=True)
        header = bytearray(18)
        header[2] = 2
        struct.pack_into('<HH', header, 12, 44, 128)
        header[16:18] = bytes((32, 0x28))
        (self.lang / 'font/font000.tga').write_bytes(header + bytes(44 * 128 * 4))
        (self.lang / 'font/fontwdata.bin').write_bytes(bytes(448))

    def write_nro(self, version):
        nro = bytearray(0x80)
        nro[0x10:0x14] = b'NRO0'
        struct.pack_into('<I', nro, 0x18, len(nro))
        assets = struct.pack('<4sIQQQQQQ', b'ASET', 0, 56, 0, 56, 0x4000, 0x4038, 0)
        nacp = bytearray(0x4000)
        nacp[:7] = b'Fixture'
        nacp[0x3060:0x3060 + len(version)] = version.encode()
        self.nro.write_bytes(nro + assets + nacp)

    def args(self, include_ini=False, name='release'):
        return SimpleNamespace(nro=self.nro, lang=self.lang, output=self.root / name,
                               include_language_ini=include_ini, no_zip=False, dry_run=False)

    def run_package(self, args):
        with contextlib.redirect_stdout(io.StringIO()):
            package_zh.package(args)

    def test_inspect(self):
        info = package_zh.inspect_nro(self.nro)
        self.assertEqual(info['version'], '1.00-zh1')
        self.assertEqual(info['sha256'], hashlib.sha256(self.nro.read_bytes()).hexdigest())

    def test_minimal_output_and_no_overwrite(self):
        args = self.args()
        self.run_package(args)
        with zipfile.ZipFile(self.root / 'release.zip') as archive:
            self.assertEqual(set(archive.namelist()), {
                'switch/sh2-nx/sh2-nx.nro', 'switch/sh2-nx/lang/font/font000.tga',
                'switch/sh2-nx/lang/font/fontwdata.bin'})
        with self.assertRaises(FileExistsError):
            self.run_package(args)

    def test_language_ini_is_opt_in(self):
        self.run_package(self.args(include_ini=True))
        self.assertEqual((self.root / 'release/switch/sh2-nx/language.ini').read_bytes(),
                         b'SET DX_CONFIG_LANGUAGE 0\r\n')

    def test_png_only_uncompressed_package(self):
        (self.lang / 'font/font000.tga').unlink()
        png = b'\x89PNG\r\n\x1a\n' + struct.pack('>I', 13) + b'IHDR' + struct.pack('>II', 44, 128)
        (self.lang / 'font/font000.png').write_bytes(png)
        args = self.args()
        args.no_zip = True
        self.run_package(args)
        self.assertFalse((self.root / 'release.zip').exists())
        self.assertEqual((args.output / 'switch/sh2-nx/lang/font/font000.png').read_bytes(), png)
        self.assertFalse((args.output / 'switch/sh2-nx/lang/font/font000.tga').exists())

    def test_unmarked_nro_does_not_create_output(self):
        self.write_nro('1.00')
        with self.assertRaises(ValueError):
            self.run_package(self.args())
        self.assertFalse((self.root / 'release').exists())

    def test_stale_font_dimensions_are_rejected(self):
        png = b'\x89PNG\r\n\x1a\n' + struct.pack('>I', 13) + b'IHDR' + struct.pack('>II', 44, 256)
        (self.lang / 'font/font000.png').write_bytes(png)
        with self.assertRaises(ValueError):
            self.run_package(self.args())
        self.assertFalse((self.root / 'release').exists())

    def test_invalid_nro_bounds(self):
        data = bytearray(self.nro.read_bytes())
        struct.pack_into('<I', data, 0x18, len(data) + 1)
        self.nro.write_bytes(data)
        with self.assertRaises(ValueError):
            package_zh.inspect_nro(self.nro)


if __name__ == '__main__':
    unittest.main()
