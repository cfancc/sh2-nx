"""Convert your own EE font PNG to a lossless, alpha-preserving RLE TGA."""
import argparse
from pathlib import Path
from PIL import Image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    if args.source.resolve() == args.destination.resolve():
        parser.error("source and destination must be different files")
    with Image.open(args.source) as source:
        image = source.convert("RGBA")
        image.save(args.destination, format="TGA", compression="tga_rle")
    with Image.open(args.destination) as result:
        if result.convert("RGBA").tobytes() != image.tobytes():
            raise RuntimeError("converted pixels do not match the source")
    print(f"Converted and verified {image.width} x {image.height} pixels")


if __name__ == "__main__":
    main()
