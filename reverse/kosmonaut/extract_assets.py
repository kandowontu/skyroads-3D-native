#!/usr/bin/env python3
"""Extract the executable-resident Kosmonaut data used by the native port.

KOSMO.EXE was linked with Microsoft EXEPACK.  Run an EXEPACK unpacker first,
then pass the resulting MZ executable to this script.  None of the emitted
files contains executable code.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path


SCREEN_WIDTH = 320
SCREEN_HEIGHT = 200
PLANE_STRIDE = SCREEN_WIDTH // 8
PLANE_SIZE = PLANE_STRIDE * SCREEN_HEIGHT
ROAD_COUNT = 26
ROAD_COLUMNS = 6
ROAD_ROWS = 200
ROAD_SIZE = ROAD_COLUMNS * ROAD_ROWS

# Segment values below are the link-time values stored in the executable.
# Ghidra displays them 0x1000 paragraphs higher because of its DOS load base.
DATA_SEGMENT = 0x40DD
# The gameplay loop loads ES with the literal far segment 1675h before it
# indexes each road's 0x32-byte animated-tile record.  This is deliberately
# separate from DATA_SEGMENT; treating Ghidra's flattened ES references as DS
# would incorrectly walk into the font table after road two.
LEVEL_EFFECTS_SEGMENT = 0x1675
LEVEL_SEGMENT = 0x1711
SCREEN01_SEGMENT = 0x1F3A
SCREEN23_SEGMENT = 0x296C
SCREEN4_SEGMENT = 0x390C
RENDERER_SEGMENT = 0x05CF
SHIP_SEGMENT = 0x10DC
SHIP_FRAME_COUNT = 98
SHIP_FRAME_BYTES = 26 * 9

EGA_REGISTER_TO_RGB = tuple(
    (
        (((value >> 2) & 1) * 0xAA) + (((value >> 5) & 1) * 0x55),
        (((value >> 1) & 1) * 0xAA) + (((value >> 4) & 1) * 0x55),
        (((value >> 0) & 1) * 0xAA) + (((value >> 3) & 1) * 0x55),
    )
    for value in range(64)
)


class MzImage:
    def __init__(self, path: Path) -> None:
        self.path = path
        self.data = path.read_bytes()
        if self.data[:2] != b"MZ":
            raise ValueError(f"{path} is not an MZ executable")
        self.header_size = struct.unpack_from("<H", self.data, 0x08)[0] * 16
        if self.header_size < 0x1C or self.header_size >= len(self.data):
            raise ValueError("invalid MZ header size")

    def slice(self, segment: int, offset: int, size: int) -> bytes:
        start = self.header_size + segment * 16 + offset
        end = start + size
        if start < self.header_size or end > len(self.data):
            raise ValueError(
                f"far range {segment:04x}:{offset:04x}+{size:x} is outside the image"
            )
        return self.data[start:end]


def decode_planes(planes: list[bytes]) -> bytes:
    if len(planes) != 4 or any(len(plane) != PLANE_SIZE for plane in planes):
        raise ValueError("a Kosmonaut screen must contain four 8,000-byte planes")
    pixels = bytearray(SCREEN_WIDTH * SCREEN_HEIGHT)
    for y in range(SCREEN_HEIGHT):
        for byte_x in range(PLANE_STRIDE):
            source = y * PLANE_STRIDE + byte_x
            for bit in range(8):
                mask = 0x80 >> bit
                color = 0
                for plane_index, plane in enumerate(planes):
                    if plane[source] & mask:
                        color |= 1 << plane_index
                pixels[y * SCREEN_WIDTH + byte_x * 8 + bit] = color
    return bytes(pixels)


def full_screen(image: MzImage, segment: int, base: int) -> bytes:
    offsets = [base + plane * PLANE_SIZE for plane in range(4)]
    return decode_planes([image.slice(segment, offset, PLANE_SIZE) for offset in offsets])


def split_screen(image: MzImage, screen_index: int) -> bytes:
    # The top 139 rows are shared.  Each screen has a distinct 61-row panel.
    common_size = 0x15B8
    panel_size = 0x0988
    common_offsets = [0x4C42, 0x61FA, 0x77B2, 0x8D6A]
    panel_base = 0x0002 if screen_index == 0 else 0x2622
    panel_offsets = [panel_base + plane * panel_size for plane in range(4)]
    planes = []
    for common_offset, panel_offset in zip(common_offsets, panel_offsets):
        planes.append(
            image.slice(SCREEN01_SEGMENT, common_offset, common_size)
            + image.slice(SCREEN01_SEGMENT, panel_offset, panel_size)
        )
    return decode_planes(planes)


def write_ppm(path: Path, pixels: bytes, palette: list[tuple[int, int, int]]) -> None:
    rgb = bytearray()
    for pixel in pixels:
        rgb.extend(palette[pixel])
    path.write_bytes(
        f"P6\n{SCREEN_WIDTH} {SCREEN_HEIGHT}\n255\n".encode("ascii") + rgb
    )


def extract(executable: Path, output: Path) -> None:
    image = MzImage(executable)
    output.mkdir(parents=True, exist_ok=True)

    palette_registers = image.slice(RENDERER_SEGMENT, 0x0BA6, 16)
    if len(set(palette_registers)) != 16 or any(value >= 64 for value in palette_registers):
        raise ValueError("the executable does not contain the expected EGA palette table")
    palette = [EGA_REGISTER_TO_RGB[value] for value in palette_registers]

    screens = [
        split_screen(image, 0),
        split_screen(image, 1),
        full_screen(image, SCREEN23_SEGMENT, 0x0002),
        full_screen(image, SCREEN23_SEGMENT, 0x7D02),
        full_screen(image, SCREEN4_SEGMENT, 0x0002),
    ]
    screen_names = ["road-select", "gameplay", "tutorial", "title", "completed"]
    (output / "screens.bin").write_bytes(b"".join(screens))
    for name, pixels in zip(screen_names, screens):
        write_ppm(output / f"{name}.ppm", pixels, palette)

    # The selected road is one-based in the DOS loop.  Offset 0402h is a
    # non-playable record; road 1 starts at 0402h + 04B0h.
    roads = b"".join(
        image.slice(LEVEL_SEGMENT, 0x0402 + (index + 1) * ROAD_SIZE, ROAD_SIZE)
        for index in range(ROAD_COUNT)
    )
    (output / "roads.bin").write_bytes(roads)
    # 0402h is the road copied by the playable flight demonstration.  The
    # earlier 000Ch record belongs only to the astronaut tutorial fly-over.
    (output / "demo-road.bin").write_bytes(
        image.slice(LEVEL_SEGMENT, 0x0402, ROAD_SIZE)
    )
    (output / "tutorial-road.bin").write_bytes(
        image.slice(LEVEL_SEGMENT, 0x000C, ROAD_SIZE)
    )
    (output / "font.bin").write_bytes(image.slice(DATA_SEGMENT, 0x0138, 0x02B8))
    (output / "font-styles.bin").write_bytes(
        image.slice(DATA_SEGMENT, 0x0110, 5 * 8)
    )
    (output / "level-effects.bin").write_bytes(
        # Like the road data, normal play uses the one-based road number.
        image.slice(LEVEL_EFFECTS_SEGMENT, 0x00AE + 0x32, ROAD_COUNT * 0x32)
    )
    (output / "demo-effects.bin").write_bytes(
        image.slice(LEVEL_EFFECTS_SEGMENT, 0x00AE, 0x32)
    )
    # Demo steering is indexed by the live forward position and may be read
    # through the exact finish coordinate (02F0h), rather than as a short
    # sequential recording.
    (output / "demo-input.bin").write_bytes(image.slice(DATA_SEGMENT, 0x06B2, 0x2F1))
    (output / "palette.bin").write_bytes(palette_registers)
    # The original player is a family of transparent 26x9 indexed sprites.
    # Frame selection covers five animation phases, rising/falling poses,
    # seven lateral perspectives, effects, and the destruction sequence.
    (output / "ship-sprites.bin").write_bytes(
        image.slice(SHIP_SEGMENT, 0x0001, SHIP_FRAME_COUNT * SHIP_FRAME_BYTES)
    )
    # These are data records consumed by the native translation of the EGA
    # road renderer: edge masks, 4*33*3 descriptor offsets, and span records.
    # The range begins after every executable renderer routine.
    (output / "render-masks.bin").write_bytes(
        image.slice(RENDERER_SEGMENT, 0x2921, 0x0200)
    )
    (output / "render-pointers.bin").write_bytes(
        image.slice(RENDERER_SEGMENT, 0x2B21, 0x0318)
    )
    (output / "render-shapes.bin").write_bytes(
        image.slice(RENDERER_SEGMENT, 0x3795, 0xB0D0 - 0x3795)
    )
    # Small executable-neutral lookup records used by the recovered gauges,
    # ship collision profile, and five-step ship animation.  These reside in
    # the same segment as the animated-road records, not in the main data
    # segment selected by the Microsoft C runtime.
    (output / "simulation-tables.bin").write_bytes(
        image.slice(LEVEL_EFFECTS_SEGMENT, 0x05F4, 0x00B0)
    )
    # The two original footer crawls are fixed character streams in the same
    # far segment as the effects and dashboard tables.  The C runtime's main
    # data segment happens to contain unrelated library text at these offsets.
    (output / "title-scroll.bin").write_bytes(
        image.slice(LEVEL_EFFECTS_SEGMENT, 0x0014, 0x001D)
    )
    (output / "tutorial-scroll.bin").write_bytes(
        image.slice(LEVEL_EFFECTS_SEGMENT, 0x0032, 0x007C)
    )
    (output / "destruction-frames.bin").write_bytes(
        image.slice(DATA_SEGMENT, 0x00CA, 34 * 2)
    )
    # Two PC-speaker songs are stored as paired PIT-divisor and duration-word
    # streams.  The range begins with track-one tempo/release bytes and ends
    # immediately before the interrupt handler's mutable playback state.
    (output / "music.bin").write_bytes(
        image.slice(RENDERER_SEGMENT, 0x0D42, 0x118C)
    )
    # Executable-neutral state tables for the three-rate moving starfield.
    # Code is deliberately excluded: these are motion deltas, RNG constants,
    # path offsets, and zero-terminated path byte streams only.
    (output / "star-motion.bin").write_bytes(
        image.slice(RENDERER_SEGMENT, 0x04C7, 0x0010)
    )
    (output / "star-rng.bin").write_bytes(
        image.slice(RENDERER_SEGMENT, 0x08F3, 0x0020)
    )
    (output / "star-path-pointers.bin").write_bytes(
        image.slice(RENDERER_SEGMENT, 0x2E39, 55 * 2)
    )
    (output / "star-paths.bin").write_bytes(
        image.slice(RENDERER_SEGMENT, 0x2EA7, 0x08EE)
    )

    manifest = {
        "source": executable.name,
        "source_sha256": hashlib.sha256(image.data).hexdigest(),
        "screens": screen_names,
        "screen_width": SCREEN_WIDTH,
        "screen_height": SCREEN_HEIGHT,
        "road_count": ROAD_COUNT,
        "road_columns": ROAD_COLUMNS,
        "road_rows": ROAD_ROWS,
        "road_bytes": ROAD_SIZE,
        "palette_registers": list(palette_registers),
        "files": {},
    }
    for path in sorted(output.iterdir()):
        if path.is_file() and path.name != "manifest.json":
            data = path.read_bytes()
            manifest["files"][path.name] = {
                "bytes": len(data),
                "sha256": hashlib.sha256(data).hexdigest(),
            }
    (output / "manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8"
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("executable", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    extract(args.executable.resolve(), args.output.resolve())


if __name__ == "__main__":
    main()
