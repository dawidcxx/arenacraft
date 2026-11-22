#!/usr/bin/env python3
"""Regenerate AppIcon.ico from AppIcon.jpeg.

    ./scripts/make_icon.py [source.jpeg] [output.ico]

Requires ffmpeg on PATH. The icon carries the sizes Windows picks between at the
common DPI scales; every entry below 256 px is a 32-bit BGRA DIB, the 256 px
entry is PNG (the one size Windows expects to be PNG-compressed).
"""

import struct
import subprocess
import sys
from pathlib import Path

SIZES = (16, 20, 24, 32, 40, 48, 64, 96, 128, 256)


def scale(source: Path, size: int) -> bytes:
    """Return the source image as size x size RGBA pixels."""
    result = subprocess.run(
        [
            "ffmpeg",
            "-v",
            "error",
            "-i",
            str(source),
            "-vf",
            f"scale={size}:{size}:flags=lanczos",
            "-frames:v",
            "1",
            "-f",
            "rawvideo",
            "-pix_fmt",
            "rgba",
            "-",
        ],
        check=True,
        capture_output=True,
    )
    pixels = result.stdout
    if len(pixels) != size * size * 4:
        raise SystemExit(f"ffmpeg returned {len(pixels)} bytes for {size}px")
    return pixels


def png(source: Path, size: int) -> bytes:
    result = subprocess.run(
        [
            "ffmpeg",
            "-v",
            "error",
            "-i",
            str(source),
            "-vf",
            f"scale={size}:{size}:flags=lanczos",
            "-frames:v",
            "1",
            "-f",
            "image2pipe",
            "-vcodec",
            "png",
            "-",
        ],
        check=True,
        capture_output=True,
    )
    return result.stdout


def dib(pixels: bytes, size: int) -> bytes:
    """Pack RGBA pixels as a bottom-up 32-bit BGRA DIB with an empty AND mask."""
    bgra = bytearray(pixels)
    bgra[0::4], bgra[2::4] = bgra[2::4], bgra[0::4]

    rows = bytearray()
    stride = size * 4
    for y in reversed(range(size)):
        rows += bgra[y * stride : (y + 1) * stride]

    header = struct.pack(
        "<IiiHHIIiiII",
        40,  # biSize
        size,  # biWidth
        size * 2,  # biHeight: XOR bitmap plus the AND mask
        1,  # biPlanes
        32,  # biBitCount
        0,  # biCompression (BI_RGB)
        len(rows),  # biSizeImage
        0,  # biXPelsPerMeter
        0,  # biYPelsPerMeter
        0,  # biClrUsed
        0,  # biClrImportant
    )
    # 1 bpp AND mask, rows padded to 4 bytes. Alpha in the XOR bitmap does the
    # real transparency work; a zero mask keeps every pixel.
    mask_stride = ((size + 31) // 32) * 4
    return header + rows + bytes(mask_stride * size)


def main() -> None:
    here = Path(__file__).resolve().parent.parent
    source = Path(sys.argv[1]) if len(sys.argv) > 1 else here / "AppIcon.jpeg"
    output = Path(sys.argv[2]) if len(sys.argv) > 2 else here / "AppIcon.ico"

    entries = []
    for size in SIZES:
        if size == 256:
            data = png(source, size)
        else:
            data = dib(scale(source, size), size)
        entries.append((size, data))

    directory = struct.pack("<HHH", 0, 1, len(entries))
    offset = len(directory) + 16 * len(entries)
    index = bytearray()
    payload = bytearray()
    for size, data in entries:
        index += struct.pack(
            "<BBBBHHII",
            0 if size == 256 else size,  # width
            0 if size == 256 else size,  # height
            0,  # palette entries
            0,  # reserved
            1,  # color planes
            32,  # bits per pixel
            len(data),
            offset,
        )
        payload += data
        offset += len(data)

    output.write_bytes(directory + index + payload)
    print(f"wrote {output} ({output.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
