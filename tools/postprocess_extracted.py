#!/usr/bin/env python3
"""Create convenient PNG previews and an HTML catalog for extracted SkyRoads data."""

from __future__ import annotations

import argparse
import html
import json
from pathlib import Path

from PIL import Image, ImageDraw


def convert_ppm(root: Path) -> int:
    converted = 0
    for source in root.rglob("*.ppm"):
        destination = source.with_suffix(".png")
        with Image.open(source) as image:
            image.save(destination, optimize=True)
        converted += 1
    return converted


def vga_palette(path: Path) -> list[tuple[int, int, int]]:
    raw = path.read_bytes()
    colors = []
    for offset in range(0, len(raw) - 2, 3):
        colors.append(tuple((value * 255) // 63 for value in raw[offset : offset + 3]))
    return colors


def road_color(value: int, palette: list[tuple[int, int, int]]) -> tuple[int, int, int]:
    bottom = value & 0x0F
    if bottom == 0:
        return (3, 4, 12)
    return palette[bottom] if bottom < len(palette) else (160, 160, 160)


def build_road_previews(root: Path) -> int:
    roads = root / "roads"
    count = 0
    for description_path in sorted(roads.glob("road_[0-9][0-9].json")):
        description = json.loads(description_path.read_text(encoding="ascii"))
        rows: list[list[int]] = description["rows"]
        palette = vga_palette(description_path.with_suffix(".palette.vga.bin"))
        cell_width = 18
        cell_height = 6
        image = Image.new("RGB", (7 * cell_width, max(1, len(rows)) * cell_height), (3, 4, 12))
        draw = ImageDraw.Draw(image)

        for row_index, row in enumerate(rows):
            for column, value in enumerate(row):
                x0 = column * cell_width
                y0 = row_index * cell_height
                x1 = x0 + cell_width - 1
                y1 = y0 + cell_height - 1
                draw.rectangle((x0, y0, x1, y1), fill=road_color(value, palette))

                # Renderer descriptor flags: tunnel, half block, full block.
                if value & 0x1000:
                    draw.line((x0, y0, x1, y0), fill=(0, 220, 255), width=2)
                if value & 0x2000:
                    draw.rectangle((x0 + 3, y0 + 1, x1 - 3, y1 - 1), fill=(210, 210, 210))
                if value & 0x4000:
                    draw.rectangle((x0 + 1, y0 + 1, x1 - 1, y1 - 1), outline=(255, 255, 255), width=2)

        destination = description_path.with_suffix(".overview.png")
        image.save(destination, optimize=True)
        count += 1
    return count


def build_anim_frames(root: Path) -> int:
    """Recompose ANIM's 221 PICT fragments into its 100 declared display frames."""
    master = json.loads((root / "manifest.json").read_text(encoding="ascii"))
    source_path = Path(master["source_root"]) / "anim.lzs"
    anim_root = root / "images" / "anim"
    manifest_path = anim_root / "manifest.json"
    if not source_path.exists() or not manifest_path.exists():
        return 0

    source = source_path.read_bytes()
    if source[:4] != b"ANIM":
        raise ValueError(f"{source_path} does not begin with ANIM")
    declared_frame_count = int.from_bytes(source[4:6], "little")
    fragment_manifest = json.loads(manifest_path.read_text(encoding="ascii"))["frames"]
    palette = vga_palette(anim_root / "palette_000.vga.bin")
    flat_palette = [component for color in palette for component in color]
    flat_palette.extend([0] * (768 - len(flat_palette)))

    palette_count = source[10]
    cursor = 6 + 4 + 1 + palette_count * 5
    fragment_index = 0
    frame_manifest = []

    for frame_index in range(declared_frame_count):
        if cursor + 2 > len(source):
            raise ValueError(f"ANIM frame {frame_index} part count is truncated")
        part_count = int.from_bytes(source[cursor : cursor + 2], "little")
        cursor += 2
        canvas = bytearray(320 * 200)
        used_fragments = []

        for _ in range(part_count):
            if fragment_index >= len(fragment_manifest):
                raise ValueError(f"ANIM frame {frame_index} references a missing fragment")
            fragment = fragment_manifest[fragment_index]
            if source[cursor : cursor + 4] != b"PICT" or int(fragment["offset"]) != cursor:
                raise ValueError(
                    f"ANIM structure diverges at frame {frame_index}, fragment {fragment_index}, 0x{cursor:x}"
                )

            width = int(fragment["width"])
            height = int(fragment["height"])
            screen_offset = int(fragment["unknown"])
            x_offset = screen_offset % 320
            y_offset = screen_offset // 320
            pixels = (anim_root / fragment["indices_path"]).read_bytes()

            for y in range(height):
                destination_y = y_offset + y
                if destination_y < 0 or destination_y >= 200:
                    continue
                for x in range(width):
                    destination_x = x_offset + x
                    source_index = y * width + x
                    if destination_x < 0 or destination_x >= 320 or source_index >= len(pixels):
                        continue
                    palette_index = pixels[source_index]
                    if palette_index:
                        canvas[destination_y * 320 + destination_x] = palette_index

            used_fragments.append(fragment_index)
            cursor = int(fragment["next_offset"])
            fragment_index += 1

        image = Image.frombytes("P", (320, 200), bytes(canvas))
        image.putpalette(flat_palette[:768])
        destination = anim_root / f"composite_frame_{frame_index:03d}.png"
        image.save(destination, optimize=True)
        frame_manifest.append(
            {
                "index": frame_index,
                "part_count": part_count,
                "fragment_indices": used_fragments,
                "png_path": destination.name,
            }
        )

    if fragment_index != len(fragment_manifest):
        raise ValueError(
            f"ANIM declared frames consumed {fragment_index} of {len(fragment_manifest)} PICT fragments"
        )
    (anim_root / "composite_manifest.json").write_text(
        json.dumps(
            {
                "declared_frame_count": declared_frame_count,
                "fragment_count": fragment_index,
                "frames": frame_manifest,
            },
            indent=2,
            sort_keys=True,
        )
        + "\n",
        encoding="ascii",
    )
    return declared_frame_count


def contact_sheet(
    paths: list[Path], destination: Path, columns: int, tile_size: tuple[int, int], scale: int = 1
) -> None:
    if not paths:
        return
    tile_width, tile_height = tile_size
    rows = (len(paths) + columns - 1) // columns
    sheet = Image.new("RGB", (columns * tile_width, rows * tile_height), (6, 7, 15))
    for index, path in enumerate(paths):
        with Image.open(path) as source:
            image = source.convert("RGB")
        if scale != 1:
            image = image.resize((image.width * scale, image.height * scale), Image.Resampling.NEAREST)
        image.thumbnail((tile_width - 4, tile_height - 4), Image.Resampling.NEAREST)
        x = (index % columns) * tile_width + (tile_width - image.width) // 2
        y = (index // columns) * tile_height + (tile_height - image.height) // 2
        sheet.paste(image, (x, y))
    sheet.save(destination, optimize=True)


def build_contact_sheets(root: Path) -> list[str]:
    generated = []
    images_root = root / "images"

    world_paths = [images_root / f"world{index}" / "frame_000.png" for index in range(10)]
    world_paths = [path for path in world_paths if path.exists()]
    world_destination = images_root / "worlds.contact-sheet.png"
    contact_sheet(world_paths, world_destination, columns=2, tile_size=(324, 142))
    if world_destination.exists():
        generated.append(str(world_destination.relative_to(root)))

    ui_paths = []
    for group in ("intro", "gomenu", "helpmenu", "mainmenu", "setmenu", "dashbrd"):
        ui_paths.extend(sorted((images_root / group).glob("frame_*.png")))
    ui_destination = images_root / "ui.contact-sheet.png"
    contact_sheet(ui_paths, ui_destination, columns=3, tile_size=(324, 204))
    if ui_destination.exists():
        generated.append(str(ui_destination.relative_to(root)))

    sprite_paths = sorted((images_root / "anim").glob("frame_*.png"))
    sprite_destination = images_root / "anim.fragments.contact-sheet.png"
    contact_sheet(sprite_paths, sprite_destination, columns=16, tile_size=(80, 80), scale=2)
    if sprite_destination.exists():
        generated.append(str(sprite_destination.relative_to(root)))

    composite_paths = sorted((images_root / "anim").glob("composite_frame_*.png"))
    composite_destination = images_root / "anim.frames.contact-sheet.png"
    contact_sheet(composite_paths, composite_destination, columns=5, tile_size=(324, 204))
    if composite_destination.exists():
        generated.append(str(composite_destination.relative_to(root)))

    return generated


def build_animation_preview(root: Path) -> str | None:
    paths = sorted((root / "images" / "anim").glob("composite_frame_*.png"))
    if not paths:
        return None

    frames = []
    for path in paths:
        with Image.open(path) as source:
            sprite = source.convert("RGB")
        frames.append(sprite)

    destination = root / "images" / "anim.preview.gif"
    frames[0].save(
        destination,
        save_all=True,
        append_images=frames[1:],
        duration=60,
        loop=0,
        optimize=False,
    )
    return str(destination.relative_to(root))


def link(path: Path, root: Path, label: str | None = None) -> str:
    relative = path.relative_to(root).as_posix()
    return f'<a href="{html.escape(relative)}">{html.escape(label or path.name)}</a>'


def build_browser(root: Path) -> None:
    sections = []

    image_groups = []
    for directory in sorted((root / "images").iterdir()):
        if not directory.is_dir():
            continue
        pngs = sorted(directory.glob("frame_*.png"))
        if not pngs:
            continue
        thumbs = "".join(
            f'<a href="{path.relative_to(root).as_posix()}"><img loading="lazy" src="{path.relative_to(root).as_posix()}" alt="{html.escape(path.stem)}"></a>'
            for path in pngs[:24]
        )
        remainder = f"<p>Showing 24 of {len(pngs)} frames.</p>" if len(pngs) > 24 else ""
        image_groups.append(f"<h3>{html.escape(directory.name)} ({len(pngs)} frames)</h3><div class=grid>{thumbs}</div>{remainder}")
    sections.append("<h2>Decoded images</h2>" + "".join(image_groups))

    road_links = []
    for path in sorted((root / "roads").glob("road_*.overview.png")):
        road_links.append(
            f'<figure><a href="{path.relative_to(root).as_posix()}"><img loading="lazy" src="{path.relative_to(root).as_posix()}" alt="{path.stem}"></a><figcaption>{html.escape(path.stem)}</figcaption></figure>'
        )
    sections.append("<h2>Road overviews</h2><div class=roads>" + "".join(road_links) + "</div>")

    audio_links = []
    for path in sorted((root / "sounds").rglob("*.wav")):
        relative = path.relative_to(root).as_posix()
        audio_links.append(f"<p>{html.escape(path.stem)}<br><audio controls preload=none src=\"{relative}\"></audio></p>")
    sections.append("<h2>PCM audio</h2>" + "".join(audio_links))

    useful = [
        root / "images" / "worlds.contact-sheet.png",
        root / "images" / "ui.contact-sheet.png",
        root / "images" / "anim.frames.contact-sheet.png",
        root / "images" / "anim.fragments.contact-sheet.png",
        root / "images" / "anim.preview.gif",
        root / "trekdat" / "records" / "record_00.preview.png",
        root / "manifest.json",
    ]
    sections.insert(0, "<h2>Quick views</h2><ul>" + "".join(f"<li>{link(path, root)}</li>" for path in useful if path.exists()) + "</ul>")

    document = f"""<!doctype html>
<html lang="en"><meta charset="utf-8"><title>SkyRoads extracted assets</title>
<style>
body{{background:#080a18;color:#e8e9ff;font:16px system-ui;margin:2rem;max-width:1200px}}
a{{color:#71d8ff}} .grid{{display:flex;flex-wrap:wrap;gap:8px;align-items:center}}
.grid img{{max-width:320px;max-height:200px;image-rendering:pixelated;border:1px solid #29305a}}
.roads{{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:12px}}
.roads figure{{margin:0;background:#10142b;padding:8px}} .roads img{{width:100%;image-rendering:pixelated}}
h2{{margin-top:2.5rem;border-bottom:1px solid #29305a;padding-bottom:.35rem}}
</style>
<h1>SkyRoads extracted assets</h1>
<p>Generated from the local DOS distribution. PNGs are nearest-neighbor previews; raw indexed pixels, VGA palettes, JSON metadata, decompressed streams, and WAV files remain alongside them.</p>
{''.join(sections)}
</html>"""
    (root / "index.html").write_text(document, encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, required=True, help="Existing extractor output directory")
    args = parser.parse_args()
    root = args.root.resolve()
    if not (root / "manifest.json").exists():
        raise SystemExit(f"No extraction manifest found under {root}")

    converted = convert_ppm(root)
    roads = build_road_previews(root)
    anim_frames = build_anim_frames(root)
    sheets = build_contact_sheets(root)
    animation = build_animation_preview(root)
    build_browser(root)

    result = {
        "ppm_files_converted": converted,
        "road_overviews": roads,
        "animation_frames": anim_frames,
        "contact_sheets": sheets,
        "animation_preview": animation,
        "browser": "index.html",
    }
    (root / "postprocess_manifest.json").write_text(
        json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="ascii"
    )
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
