"""Convert original offscreen PPM readbacks to labeled-order comparison PNGs.

Uses only the Python standard library. No simulator or user image is processed.
The comparison order is native input, SMAA output, 16x16 spatial reference.
Pixels are enlarged by nearest-neighbor replication for inspection, not enhanced.
"""
import argparse
import binascii
import json
from pathlib import Path
import struct
import zlib


def ppm(path):
    with path.open("rb") as stream:
        if stream.readline().strip() != b"P6":
            raise ValueError("Expected fixture P6 PPM")
        width, height = map(int, stream.readline().split())
        if stream.readline().strip() != b"255" or not (0 < width <= 4096 and 0 < height <= 4096):
            raise ValueError("Unsupported fixture dimensions/encoding")
        pixels = stream.read()
    if len(pixels) != width * height * 3:
        raise ValueError("Truncated fixture readback")
    return width, height, pixels


def png(path, width, height, rows):
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", binascii.crc32(kind + data))
    data = b"\x89PNG\r\n\x1a\n"
    data += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    data += chunk(b"sRGB", b"\0")
    data += chunk(b"IDAT", zlib.compress(b"".join(b"\0" + row for row in rows)))
    path.write_bytes(data + chunk(b"IEND", b""))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    report = json.loads((args.directory / "result.json").read_text(encoding="utf-8"))
    if report.get("status") != "PASS":
        raise ValueError("Refusing to display readbacks from an incomplete/failed test")
    names = ("native-input", "smaa-output", "supersampled-reference")
    images = [ppm(args.directory / f"{name}.ppm") for name in names]
    width, height, _ = images[0]
    if any((w, h) != (width, height) for w, h, _ in images):
        raise ValueError("Evidence dimensions differ")
    for name, (_, _, data) in zip(names, images):
        png(args.directory / f"{name}.png", width, height,
            [data[y * width * 3:(y + 1) * width * 3] for y in range(height)])
    scale = 3
    rows = []
    for y in range(height):
        row = b""
        for _, _, data in images:
            row += b"".join(data[(y * width + x) * 3:(y * width + x + 1) * 3] * scale for x in range(width))
        rows.extend([row] * scale)
    png(args.directory / "comparison-native-smaa-reference.png", width * 3 * scale, height * scale, rows)
    print("Wrote synthetic comparison: native | SMAA | supersampled reference (3x nearest-neighbor view)")


if __name__ == "__main__":
    main()
