"""Inspect a Switch NRO or package a rebuilt Chinese NRO with user-owned lang assets."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import zipfile


def sha256(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def inspect_nro(path):
    """Layout follows libnx switch/nro.h and switch/nacp.h."""
    size = path.stat().st_size
    with path.open('rb') as stream:
        header = stream.read(0x80)
        if len(header) < 0x80 or header[0x10:0x14] != b'NRO0':
            raise ValueError(f'Not a valid NRO: {path}')
        code_size = struct.unpack_from('<I', header, 0x18)[0]
        if not 0x80 <= code_size <= size:
            raise ValueError('Invalid executable size in NRO header')
        version = title = author = ''
        stream.seek(code_size)
        assets = stream.read(56)
        if len(assets) == 56 and assets[:4] == b'ASET':
            asset_version = struct.unpack_from('<I', assets, 4)[0]
            offset, length = struct.unpack_from('<QQ', assets, 24)
            if asset_version != 0 or offset < 56 or length < 0x3070 or code_size + offset + length > size:
                raise ValueError('Invalid or unsupported NACP asset section')
            stream.seek(code_size + offset)
            nacp = stream.read(0x3070)
            title = nacp[:0x200].split(b'\0', 1)[0].decode('utf-8', errors='replace')
            author = nacp[0x200:0x300].split(b'\0', 1)[0].decode('utf-8', errors='replace')
            version = nacp[0x3060:0x3070].split(b'\0', 1)[0].decode('utf-8', errors='replace')
    return {'path': str(path), 'bytes': size, 'sha256': sha256(path),
            'executable_bytes': code_size, 'version': version, 'title': title, 'author': author}


def check_font(lang):
    tga = lang / 'font/font000.tga'
    widths = lang / 'font/fontwdata.bin'
    with tga.open('rb') as stream:
        header = stream.read(18)
    if len(header) != 18 or header[1] or header[2] not in (2, 10) or header[16] not in (24, 32):
        raise ValueError('Font must be an unindexed 24/32-bit raw or RLE TGA')
    width, height = struct.unpack_from('<HH', header, 12)
    if not width or not height or width % 44 or height % 128:
        raise ValueError('Font does not match the 44 x 64 cell, two-style layout')
    if widths.stat().st_size != 448:
        raise ValueError('fontwdata.bin must contain 448 bytes from the matching language pack')
    png = lang / 'font/font000.png'
    if png.exists():
        with png.open('rb') as stream:
            png_header = stream.read(24)
        if len(png_header) != 24 or png_header[:8] != b'\x89PNG\r\n\x1a\n' or png_header[12:16] != b'IHDR':
            raise ValueError('Invalid source font PNG')
        if struct.unpack_from('>II', png_header, 16) != (width, height):
            raise ValueError('TGA and PNG dimensions differ; convert the Chinese PNG instead of using a stale TGA')
    return {'width': width, 'height': height, 'width_table_bytes': 448,
            'tested_font_layout': (width, height) == (4400, 9600)}


def package(args):
    nro = args.nro.resolve(strict=True)
    lang = args.lang.resolve(strict=True)
    output = args.output.resolve()
    archive = output.parent / (output.name + '.zip')
    if output.exists() or archive.exists():
        raise FileExistsError('Output already exists; choose a new path to avoid overwriting a release')
    if output == lang or lang in output.parents:
        raise ValueError('Output must be outside the input lang directory')
    if not lang.is_dir():
        raise ValueError('--lang must point to the language pack lang directory')
    nro_info = inspect_nro(nro)
    if '-zh' not in nro_info['version'].lower():
        raise ValueError('NRO is not labelled as a Chinese rebuild; merge the adaptation and rebuild first')
    font_info = check_font(lang)
    files = []
    for path in sorted(lang.rglob('*')):
        if path.is_symlink():
            raise ValueError(f'Linked language asset is not supported: {path}')
        if path.is_file():
            relative = path.relative_to(lang)
            if '__pycache__' in relative.parts or path.name.lower() in {'thumbs.db', '.ds_store'}:
                continue
            files.append((path, 'switch/sh2-nx/lang/' + relative.as_posix()))
    if not files:
        raise ValueError('Language pack contains no files')
    report = {'nro': nro_info, 'font': font_info, 'language_assets': len(files),
              'language_slot_required': 0, 'language_ini_included': args.include_language_ini,
              'output': str(output), 'archive': str(archive), 'dry_run': args.dry_run}
    if args.dry_run:
        print(json.dumps(report, ensure_ascii=False, indent=2))
        return

    destination = output / 'switch/sh2-nx'
    destination.mkdir(parents=True)
    files.append((nro, 'switch/sh2-nx/sh2-nx.nro'))
    expected = {}
    for source, relative in files:
        target = output / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
        expected[relative] = sha256(source)
        if sha256(target) != expected[relative]:
            raise RuntimeError(f'Copy verification failed: {relative}')
    if args.include_language_ini:
        relative = 'switch/sh2-nx/language.ini'
        target = output / relative
        target.write_bytes(b'SET DX_CONFIG_LANGUAGE 0\r\n')
        expected[relative] = sha256(target)
    with zipfile.ZipFile(archive, 'x', compression=zipfile.ZIP_DEFLATED, compresslevel=6) as zipped:
        for relative in sorted(expected):
            zipped.write(output / relative, relative)
    with zipfile.ZipFile(archive) as zipped:
        if set(zipped.namelist()) != set(expected) or zipped.testzip() is not None:
            raise RuntimeError('ZIP entry or CRC verification failed')
        for relative, digest in expected.items():
            if hashlib.sha256(zipped.read(relative)).hexdigest() != digest:
                raise RuntimeError(f'ZIP hash verification failed: {relative}')
    report.update({'files': len(expected), 'archive_bytes': archive.stat().st_size,
                   'archive_sha256': sha256(archive), 'hardware_validation': 'not established by packaging'})
    print(json.dumps(report, ensure_ascii=False, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='command', required=True)
    inspect = sub.add_parser('inspect', help='Read NRO metadata and SHA-256 without modifying it')
    inspect.add_argument('nro', type=Path)
    pack = sub.add_parser('pack', help='Create only NRO + lang, with optional language.ini')
    pack.add_argument('--nro', type=Path, required=True)
    pack.add_argument('--lang', type=Path, required=True)
    pack.add_argument('--output', type=Path, required=True)
    pack.add_argument('--include-language-ini', action='store_true')
    pack.add_argument('--dry-run', action='store_true')
    args = parser.parse_args()
    try:
        if args.command == 'inspect':
            print(json.dumps(inspect_nro(args.nro.resolve(strict=True)), ensure_ascii=False, indent=2))
        else:
            package(args)
    except (OSError, ValueError, RuntimeError, zipfile.BadZipFile) as error:
        parser.exit(1, f'Error: {error}\n')


if __name__ == '__main__':
    main()
