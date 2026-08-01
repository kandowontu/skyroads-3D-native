#!/usr/bin/env python3
"""Reproducible structural analysis for the 16-bit SkyRoads MZ executable."""

from __future__ import annotations

import argparse
import csv
import json
import re
import struct
from collections import defaultdict, deque
from dataclasses import dataclass
from pathlib import Path

from capstone import CS_ARCH_X86, CS_GRP_CALL, CS_GRP_JUMP, CS_GRP_RET, CS_MODE_16, Cs
from capstone.x86 import X86_OP_IMM


@dataclass(frozen=True)
class MzImage:
    file_bytes: bytes
    header_bytes: int
    module: bytes
    entry: int
    initial_ss: int
    initial_sp: int
    min_extra_paragraphs: int
    max_extra_paragraphs: int
    relocations: tuple[tuple[int, int], ...]


def load_mz(path: Path) -> MzImage:
    data = path.read_bytes()
    if data[:2] != b"MZ" or len(data) < 0x1C:
        raise ValueError(f"{path} is not a DOS MZ executable")
    last_page, pages, relocation_count, header_paragraphs = struct.unpack_from("<HHHH", data, 2)
    min_extra, max_extra, initial_ss, initial_sp = struct.unpack_from("<HHHH", data, 0x0A)
    initial_ip, initial_cs, relocation_offset = struct.unpack_from("<HHH", data, 0x14)
    declared_size = (pages - 1) * 512 + (last_page or 512)
    if declared_size != len(data):
        raise ValueError(f"overlay or truncated file: declared={declared_size} actual={len(data)}")
    header_bytes = header_paragraphs * 16
    relocations = tuple(
        struct.unpack_from("<HH", data, relocation_offset + index * 4)
        for index in range(relocation_count)
    )
    return MzImage(
        file_bytes=data,
        header_bytes=header_bytes,
        module=data[header_bytes:declared_size],
        entry=initial_cs * 16 + initial_ip,
        initial_ss=initial_ss,
        initial_sp=initial_sp,
        min_extra_paragraphs=min_extra,
        max_extra_paragraphs=max_extra,
        relocations=relocations,
    )


def load_symbols(path: Path) -> dict[int, dict[str, str]]:
    document = json.loads(path.read_text(encoding="utf-8"))
    result = {}
    for symbol in document["symbols"]:
        address = int(symbol["address"], 0)
        result[address] = {key: value for key, value in symbol.items() if key != "address"}
    return result


def direct_target(instruction) -> int | None:
    if len(instruction.operands) == 1 and instruction.operands[0].type == X86_OP_IMM:
        return int(instruction.operands[0].imm) & 0xFFFFF
    return None


def instruction_record(instruction) -> dict[str, object]:
    return {
        "address": instruction.address,
        "size": instruction.size,
        "bytes": bytes(instruction.bytes).hex(),
        "mnemonic": instruction.mnemonic,
        "operands": instruction.op_str,
    }


def analyze_code(module: bytes, seeds: set[int]):
    decoder = Cs(CS_ARCH_X86, CS_MODE_16)
    decoder.detail = True
    functions: dict[int, dict[int, dict[str, object]]] = {}
    calls: set[tuple[int, int, int, str]] = set()
    pending_functions = deque(sorted(seeds))

    while pending_functions:
        entry = pending_functions.popleft()
        if entry in functions or not (0 <= entry < len(module)):
            continue
        instructions: dict[int, dict[str, object]] = {}
        pending_blocks = deque([entry])
        visited_blocks: set[int] = set()

        while pending_blocks:
            address = pending_blocks.popleft()
            if address in visited_blocks or not (0 <= address < len(module)):
                continue
            visited_blocks.add(address)
            cursor = address
            while 0 <= cursor < len(module):
                if cursor in instructions:
                    break
                decoded = next(decoder.disasm(module[cursor : cursor + 15], cursor, count=1), None)
                if decoded is None or decoded.size == 0:
                    break
                instructions[cursor] = instruction_record(decoded)
                next_address = cursor + decoded.size
                target = direct_target(decoded)

                if decoded.group(CS_GRP_CALL):
                    if target is not None and 0 <= target < len(module):
                        calls.add((entry, cursor, target, "call"))
                        if target not in functions:
                            pending_functions.append(target)
                    cursor = next_address
                    continue

                if decoded.group(CS_GRP_RET) or decoded.mnemonic in {"iret", "retf"}:
                    break

                if decoded.group(CS_GRP_JUMP):
                    if target is not None and 0 <= target < len(module):
                        if target in seeds and target != entry:
                            calls.add((entry, cursor, target, "tail"))
                        else:
                            pending_blocks.append(target)
                    if decoded.mnemonic == "jmp" or decoded.mnemonic.startswith("ljmp"):
                        break
                    cursor = next_address
                    continue

                # Stop on obvious data/undefined traps. Software interrupts are
                # legitimate calls into BIOS/DOS and execution continues unless
                # the surrounding routine explicitly exits.
                if decoded.mnemonic in {"ud2", "hlt"}:
                    break
                cursor = next_address

        functions[entry] = instructions

    return functions, sorted(calls)


def ascii_strings(module: bytes, data_segment_image_offset: int) -> list[dict[str, object]]:
    results = []
    for match in re.finditer(rb"[ -~]{4,}", module):
        address = match.start()
        value = match.group().decode("ascii")
        entry: dict[str, object] = {"image_address": address, "text": value}
        if address >= data_segment_image_offset:
            entry["ds_offset"] = address - data_segment_image_offset
        results.append(entry)
    return results


def render_disassembly(
    functions: dict[int, dict[int, dict[str, object]]],
    calls: list[tuple[int, int, int, str]],
    symbols: dict[int, dict[str, str]],
) -> str:
    called_targets = {target for _, _, target, _ in calls}
    entries = sorted(functions)
    auto_names = {address: f"sub_{address:04X}" for address in set(entries) | called_targets}
    names = {address: symbols.get(address, {}).get("name", auto_names[address]) for address in auto_names}
    lines = ["; Generated by reverse/tools/analyze_mz.py", "; Addresses are load-module offsets (CS=0).", ""]
    for entry in entries:
        lines.append(f"{names[entry]}: ; 0x{entry:04X}")
        if entry in symbols:
            lines.append(f"    ; {symbols[entry].get('confidence')}: {symbols[entry].get('evidence')}")
        for address, instruction in sorted(functions[entry].items()):
            operands = str(instruction["operands"])
            mnemonic = str(instruction["mnemonic"])
            target = None
            for caller, site, callee, _ in calls:
                if caller == entry and site == address:
                    target = callee
                    break
            if target is not None and mnemonic in {"call", "jmp"}:
                operands = names.get(target, f"sub_{target:04X}")
            lines.append(
                f"    {address:04X}  {str(instruction['bytes']):<20} {mnemonic:<8} {operands}".rstrip()
            )
        lines.append("")
    return "\n".join(lines)


def write_outputs(image: MzImage, symbols: dict[int, dict[str, str]], output: Path) -> None:
    output.mkdir(parents=True, exist_ok=True)
    seeds = set(symbols) | {image.entry}
    functions, calls = analyze_code(image.module, seeds)

    # The relocated word at entry+1 is the Borland DGROUP paragraph relative to
    # the load segment. Record it rather than assuming the known 0x066E value.
    data_segment_paragraph = struct.unpack_from("<H", image.module, image.entry + 1)[0]
    data_segment_image_offset = data_segment_paragraph * 16
    strings = ascii_strings(image.module, data_segment_image_offset)

    mz_manifest = {
        "file_size": len(image.file_bytes),
        "header_bytes": image.header_bytes,
        "load_module_bytes": len(image.module),
        "entry_image_address": image.entry,
        "initial_ss": image.initial_ss,
        "initial_sp": image.initial_sp,
        "min_extra_paragraphs": image.min_extra_paragraphs,
        "max_extra_paragraphs": image.max_extra_paragraphs,
        "data_segment_paragraph": data_segment_paragraph,
        "data_segment_image_offset": data_segment_image_offset,
        "relocations": [
            {"segment": segment, "offset": offset, "image_address": segment * 16 + offset}
            for offset, segment in image.relocations
        ],
        "reachable_function_entries": len(functions),
        "direct_call_edges": len(calls),
        "ascii_strings": len(strings),
    }
    (output / "mz.json").write_text(json.dumps(mz_manifest, indent=2) + "\n", encoding="utf-8")
    (output / "strings.json").write_text(json.dumps(strings, indent=2) + "\n", encoding="utf-8")

    with (output / "symbols.tsv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream, delimiter="\t", lineterminator="\n")
        writer.writerow(["address", "name", "confidence", "evidence"])
        for address, symbol in sorted(symbols.items()):
            writer.writerow([
                f"0x{address:04x}",
                symbol["name"],
                symbol["confidence"],
                symbol["evidence"],
            ])

    callers: dict[int, set[int]] = defaultdict(set)
    callees: dict[int, set[int]] = defaultdict(set)
    for caller, _, callee, _ in calls:
        callers[callee].add(caller)
        callees[caller].add(callee)
    function_manifest = []
    for entry, instructions in sorted(functions.items()):
        addresses = sorted(instructions)
        function_manifest.append(
            {
                "address": entry,
                "name": symbols.get(entry, {}).get("name", f"sub_{entry:04X}"),
                "confidence": symbols.get(entry, {}).get("confidence", "unknown"),
                "instruction_count": len(instructions),
                "lowest_address": addresses[0] if addresses else entry,
                "highest_address": max(
                    (address + int(instructions[address]["size"]) for address in addresses), default=entry
                ),
                "callers": sorted(callers[entry]),
                "callees": sorted(callees[entry]),
            }
        )
    (output / "functions.json").write_text(json.dumps(function_manifest, indent=2) + "\n", encoding="utf-8")

    with (output / "calls.csv").open("w", newline="", encoding="ascii") as stream:
        writer = csv.writer(stream)
        writer.writerow(["caller", "site", "callee", "kind"])
        for row in calls:
            writer.writerow([f"0x{row[0]:04x}", f"0x{row[1]:04x}", f"0x{row[2]:04x}", row[3]])

    disassembly = render_disassembly(functions, calls, symbols)
    (output / "disassembly.asm").write_text(disassembly, encoding="utf-8")

    names = {entry: symbols.get(entry, {}).get("name", f"sub_{entry:04X}") for entry in functions}
    dot = ["digraph skyroads {", "  rankdir=LR;"]
    for entry in sorted(functions):
        dot.append(f'  n{entry:04x} [label="{names[entry]}\\n{entry:04X}"];')
    for caller, _, callee, kind in calls:
        if caller in functions and callee in functions:
            style = "dashed" if kind == "tail" else "solid"
            dot.append(f"  n{caller:04x} -> n{callee:04x} [style={style}];")
    dot.append("}")
    (output / "callgraph.dot").write_text("\n".join(dot) + "\n", encoding="ascii")

    print(json.dumps(mz_manifest, indent=2))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("exe", type=Path)
    parser.add_argument("--symbols", type=Path, default=Path("reverse/symbols.json"))
    parser.add_argument("--output", type=Path, default=Path("reverse/generated"))
    args = parser.parse_args()
    write_outputs(load_mz(args.exe), load_symbols(args.symbols), args.output)


if __name__ == "__main__":
    main()
