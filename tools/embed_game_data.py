#!/usr/bin/env python3
"""Generate a C++ translation unit containing the original compressed assets."""

from __future__ import annotations

import argparse
from pathlib import Path


BASE_FILES = (
    "anim.lzs",
    "cars.lzs",
    "dashbrd.lzs",
    "demo.rec",
    "ful_disp.dat",
    "gomenu.lzs",
    "helpmenu.lzs",
    "intro.lzs",
    "intro.snd",
    "mainmenu.lzs",
    "muzax.lzs",
    "oxy_disp.dat",
    "roads.lzs",
    "setmenu.lzs",
    "sfx.snd",
    "speed.dat",
    "trekdat.lzs",
    *(f"world{index}.lzs" for index in range(10)),
)

XMAS_FILES = (
    "gomenu.lzs",
    "roads.lzs",
    *(f"world{index}.lzs" for index in range(10)),
)


def resolve_case_insensitive(root: Path, name: str) -> Path:
    exact = root / name
    if exact.is_file():
        return exact
    wanted = name.casefold()
    for candidate in root.iterdir():
        if candidate.is_file() and candidate.name.casefold() == wanted:
            return candidate
    raise FileNotFoundError(f"Missing original game build input: {root / name}")


def read_campaign(root: Path | None, names: tuple[str, ...]) -> list[tuple[str, bytes]]:
    if root is None:
        return []
    return [(name, resolve_case_insensitive(root, name).read_bytes()) for name in names]


def emit_blob(lines: list[str], symbol: str, files: list[tuple[str, bytes]]) -> None:
    blob = b"".join(data for _, data in files)
    lines.append(f"alignas(16) const std::uint8_t {symbol}[] = {{")
    if blob:
        for start in range(0, len(blob), 20):
            chunk = blob[start : start + 20]
            lines.append("    " + ",".join(f"0x{value:02x}" for value in chunk) + ",")
    else:
        lines.append("    0x00,")
    lines.append("};")
    lines.append("")


def emit_lookup(
    lines: list[str], symbol: str, files: list[tuple[str, bytes]], indent: str
) -> None:
    offset = 0
    for name, data in files:
        lines.append(
            f'{indent}if (name == "{name}") return '
            f"{{{symbol} + {offset}u, {len(data)}u}};"
        )
        offset += len(data)


def generate(base_root: Path, xmas_root: Path | None) -> str:
    base_files = read_campaign(base_root, BASE_FILES)
    xmas_files = read_campaign(xmas_root, XMAS_FILES)
    lines = [
        '#include "embedded_game_data.hpp"',
        "",
        "#include <cstdint>",
        "",
        "namespace skyroads {",
        "namespace {",
        "",
    ]
    emit_blob(lines, "base_blob", base_files)
    emit_blob(lines, "xmas_blob", xmas_files)
    lines.extend(
        [
            "} // namespace",
            "",
            "std::span<const std::uint8_t> embedded_game_file(",
            "    EmbeddedCampaign campaign,",
            "    std::string_view name) {",
            "    if (campaign == EmbeddedCampaign::SkyRoads) {",
        ]
    )
    emit_lookup(lines, "base_blob", base_files, "        ")
    lines.append("    }")
    if xmas_files:
        lines.append("    else {")
        emit_lookup(lines, "xmas_blob", xmas_files, "        ")
        lines.append("    }")
    lines.extend(
        [
            "    return {};",
            "}",
            "",
            "bool embedded_xmas_data_available() {",
            f"    return {'true' if xmas_files else 'false'};",
            "}",
            "",
            "} // namespace skyroads",
            "",
        ]
    )
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--base-dir", required=True, type=Path)
    parser.add_argument("--xmas-dir", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    content = generate(args.base_dir.resolve(),
                       args.xmas_dir.resolve() if args.xmas_dir else None)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if not args.output.exists() or args.output.read_text(encoding="utf-8") != content:
        args.output.write_text(content, encoding="utf-8", newline="\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
