#!/usr/bin/env python3
"""Build an oracle-only copy of skyroads.exe without modifying the original."""

from __future__ import annotations

import argparse
import struct
from pathlib import Path


HEADER_BYTES = 0x200
HOOK_IMAGE_OFFSET = 0x12040
HOOK_SEGMENT = HOOK_IMAGE_OFFSET // 16
# The original initial stack ends at image 12030.  Its C runtime shrinks the
# process block to that boundary before switching SS to DGROUP, which would
# release and overwrite an appended hook.  Give only the trace copy a higher
# temporary loader stack so the hook remains owned throughout execution.
TRACE_INITIAL_SS = 0x1300
TRACE_RUNTIME_SP = 0xF000
TRACE_PRESENTATION_RET_OFFSETS = (
    (0x0BE3, bytes.fromhex("c80e0000"), "render_gameplay_frame"),
)


def expect(data: bytearray, offset: int, expected: bytes, label: str) -> None:
    actual = bytes(data[offset : offset + len(expected)])
    if actual != expected:
        raise ValueError(
            f"{label} mismatch at image {offset:04x}: "
            f"expected {expected.hex()}, found {actual.hex()}"
        )


def far_call(offset: int) -> bytes:
    return b"\x9a" + struct.pack("<HH", offset, HOOK_SEGMENT)


def far_jump(offset: int) -> bytes:
    return b"\xea" + struct.pack("<HH", offset, HOOK_SEGMENT)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("original", type=Path)
    parser.add_argument("hook", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument(
        "--frame", action="store_true",
        help="build the VGA framebuffer oracle instead of the state/sound oracle",
    )
    parser.add_argument(
        "--palette", action="store_true",
        help="build the VGA DAC-write oracle instead of the state/sound oracle",
    )
    parser.add_argument(
        "--input", action="store_true",
        help="build the controlled raw-input oracle instead of the state/sound oracle",
    )
    args = parser.parse_args()
    if sum((args.frame, args.palette, args.input)) > 1:
        parser.error("--frame, --palette, and --input are mutually exclusive")
    trace_mode = not args.frame and not args.palette and not args.input

    original = bytearray(args.original.read_bytes())
    hook = args.hook.read_bytes()
    if original[:2] != b"MZ" or len(original) < HEADER_BYTES:
        raise ValueError("input is not the reviewed SkyRoads MZ executable")
    header_paragraphs = struct.unpack_from("<H", original, 8)[0]
    relocation_count = struct.unpack_from("<H", original, 6)[0]
    relocation_offset = struct.unpack_from("<H", original, 0x18)[0]
    if header_paragraphs * 16 != HEADER_BYTES or relocation_count != 2:
        raise ValueError("unexpected SkyRoads MZ header layout")
    if args.input:
        if len(hook) < 24:
            raise ValueError("input hook symbol table is missing")
        (
            entry_hook,
            input_harness,
            joystick_axis_hook,
            joystick_button_hook,
            mouse_value_hook,
            mouse_set_hook,
            _,
            sample_call_relocation,
            axis_return_relocation,
            button_return_relocation,
            mouse_value_return_relocation,
            mouse_set_return_relocation,
        ) = struct.unpack_from("<HHHHHHHHHHHH", hook, 0)
    elif args.frame or args.palette:
        if len(hook) < 12:
            raise ValueError("frame/palette hook symbol table is missing")
        if args.frame:
            (
                state_hook,
                entry_hook,
                _,
                state_resume_relocation,
                transfer_hook,
                transfer_resume_relocation,
            ) = struct.unpack_from("<HHHHHH", hook, 0)
        else:
            (
                state_hook,
                palette_hook,
                entry_hook,
                _,
                state_resume_relocation,
                palette_resume_relocation,
            ) = struct.unpack_from("<HHHHHH", hook, 0)
    else:
        if len(hook) < 24:
            raise ValueError("trace hook symbol table is missing")
        (
            state_hook,
            sound_hook,
            entry_hook,
            _,
            state_resume_relocation,
            sound_disabled_relocation,
            sound_enabled_relocation,
            opl_tick_relocation_1,
            opl_tick_relocation_2,
            opl_tick_relocation_3,
            opl_tick_relocation_4,
            opl_tick_relocation_5,
        ) = struct.unpack_from("<HHHHHHHHHHHH", hook, 0)
    entry_dgroup_pattern = b"\xbf\x6e\x06\x8e\xdf\xcb"
    entry_dgroup_instruction = hook.find(entry_dgroup_pattern, entry_hook)
    if entry_dgroup_instruction < 0:
        raise ValueError("trace hook entry does not contain the displaced DGROUP setup")
    entry_dgroup_relocation = entry_dgroup_instruction + 1

    module = bytearray(original[HEADER_BYTES:])
    expect(
        module,
        0x2ADD,
        bytes.fromhex("8346fe01a10c163946fe"),
        "gameplay hook",
    )
    if trace_mode:
        expect(
            module,
            0x03C8,
            bytes.fromhex("a10c16a348af833e284500"),
            "sound hook",
        )
    expect(module, 0x4575, bytes.fromhex("c8e800"), "intro entry")
    expect(module, 0x3AE3, b"\xe4", "PIT divisor low")
    expect(module, 0x3AE7, b"\x19", "PIT divisor high")
    expect(module, 0x3B03, bytes.fromhex("e8331f"), "timer OPL call")
    if args.frame:
        expect(module, 0x38A3, bytes.fromhex("53551ea380"), "road delta hook")
    if args.palette:
        expect(module, 0x6233, bytes.fromhex("8bdc1e0660"), "VGA palette hook")
    if trace_mode:
        expect(module, 0x5A47, b"\xc3", "OPL tick return")
    if args.input:
        expect(module, 0x05F6, bytes.fromhex("c800000056"), "joystick axis hook")
        expect(module, 0x0672, bytes.fromhex("c800000056"), "joystick button hook")
        expect(module, 0x06A4, bytes.fromhex("c800000056"), "mouse position hook")
        expect(module, 0x06B9, bytes.fromhex("c806000056"), "mouse value hook")
        expect(module, 0x0AF9, b"\xc3", "input sampler return")
    expect(
        module,
        0x225B,
        bytes.fromhex("8b46fc39060c167503e9f4ff"),
        "gameplay timer wait",
    )
    expect(module, 0x22A0, bytes.fromhex("e93e08"), "gameplay catch-up branch")
    expect(module, 0x60D0, bytes.fromhex("bf6e068edf"), "MZ entry hook")
    expect(module, 0x60FD, bytes.fromhex("81c450b1"), "runtime stack setup")
    if trace_mode or args.palette:
        for offset, expected, name in TRACE_PRESENTATION_RET_OFFSETS:
            expect(module, offset, expected, name)

    if not args.input:
        module[0x2ADD : 0x2AE7] = far_jump(state_hook) + b"\x90" * 5
    if trace_mode:
        module[0x03C8 : 0x03D3] = far_jump(sound_hook) + b"\x90" * 6
    if args.input:
        module[0x4575 : 0x457A] = far_jump(input_harness)
        module[0x05F6 : 0x05FB] = far_jump(joystick_axis_hook)
        module[0x0672 : 0x0677] = far_jump(joystick_button_hook)
        module[0x06A4 : 0x06A9] = far_jump(mouse_set_hook)
        module[0x06B9 : 0x06BE] = far_jump(mouse_value_hook)
        module[0x0AF9] = 0xCB
    elif not args.palette:
        module[0x4575 : 0x4578] = b"\x31\xc0\xc3"  # skip intro, enter demo
    module[0x60D0 : 0x60D5] = far_call(entry_hook)
    # Startup immediately switches SS to DGROUP and derives its DOS resize
    # boundary from SP.  Keep the fixed DGROUP layout but move this temporary
    # stack ceiling above the appended probe.
    module[0x60FF : 0x6101] = struct.pack("<H", TRACE_RUNTIME_SP)
    if not args.input:
        module[0x225B : 0x2267] = bytes.fromhex("ff060c16") + b"\x90" * 8
    if args.frame:
        # 22A0 normally enters the clock comparison at 2AE1, part of the
        # instruction range occupied by the frame probe.  The controlled wait
        # above has made the global clock one tick newer here, so preserve the
        # comparison's taken path directly.
        module[0x22A0 : 0x22A3] = bytes.fromhex("e90000")
        module[0x38A3 : 0x38A8] = far_jump(transfer_hook)
    if args.palette:
        module[0x6233 : 0x6238] = far_jump(palette_hook)
    module[0x3B03 : 0x3B06] = b"\x90" * 3
    if trace_mode:
        # The trace hook calls this formerly-near ISR helper across segments.
        module[0x5A47] = 0xCB
    if trace_mode or args.palette:
        # State/palette traces need original transitions, not VGA throughput.
        for offset, _, _ in TRACE_PRESENTATION_RET_OFFSETS:
            module[offset] = 0xC3

    if len(module) > HOOK_IMAGE_OFFSET:
        raise ValueError("hook image offset overlaps the original image")
    module.extend(b"\0" * (HOOK_IMAGE_OFFSET - len(module)))
    module.extend(hook)

    # The original relocation at 0000:60d1 adjusted the displaced
    # `mov di,066e` startup immediate. Move it to the copy inside the hook;
    # otherwise DOS would relocate the offset word of our new far call.
    struct.pack_into(
        "<HH",
        original,
        relocation_offset + 4,
        entry_dgroup_relocation,
        HOOK_SEGMENT,
    )

    # Add relocations for the entry call, probe jumps, and their return jumps.
    if args.input:
        new_relocations = (
            (0x05F9, 0),
            (0x0675, 0),
            (0x06A7, 0),
            (0x06BC, 0),
            (0x4578, 0),
            (0x60D3, 0),
            (sample_call_relocation, HOOK_SEGMENT),
            (axis_return_relocation, HOOK_SEGMENT),
            (button_return_relocation, HOOK_SEGMENT),
            (mouse_value_return_relocation, HOOK_SEGMENT),
            (mouse_set_return_relocation, HOOK_SEGMENT),
        )
    elif args.frame:
        new_relocations = (
            (0x2AE0, 0),
            (0x38A6, 0),
            (0x60D3, 0),
            (state_resume_relocation, HOOK_SEGMENT),
            (transfer_resume_relocation, HOOK_SEGMENT),
        )
    elif args.palette:
        new_relocations = (
            (0x2AE0, 0),
            (0x6236, 0),
            (0x60D3, 0),
            (state_resume_relocation, HOOK_SEGMENT),
            (palette_resume_relocation, HOOK_SEGMENT),
        )
    else:
        new_relocations = (
            (0x2AE0, 0),
            (0x03CB, 0),
            (0x60D3, 0),
            (state_resume_relocation, HOOK_SEGMENT),
            (sound_disabled_relocation, HOOK_SEGMENT),
            (sound_enabled_relocation, HOOK_SEGMENT),
            (opl_tick_relocation_1, HOOK_SEGMENT),
            (opl_tick_relocation_2, HOOK_SEGMENT),
            (opl_tick_relocation_3, HOOK_SEGMENT),
            (opl_tick_relocation_4, HOOK_SEGMENT),
            (opl_tick_relocation_5, HOOK_SEGMENT),
        )
    for index, relocation in enumerate(new_relocations, start=relocation_count):
        struct.pack_into(
            "<HH", original, relocation_offset + index * 4, *relocation
        )
    struct.pack_into("<H", original, 6, relocation_count + len(new_relocations))

    output = original[:HEADER_BYTES] + module
    struct.pack_into("<H", output, 0x0E, TRACE_INITIAL_SS)
    pages = (len(output) + 511) // 512
    last_page = len(output) % 512
    struct.pack_into("<HH", output, 2, last_page, pages)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(output)
    print(
        f"wrote {args.output} ({len(output)} bytes), "
        f"{'entry' if args.input else 'state'} hook="
        f"{entry_hook if args.input else state_hook:04x}, "
        f"mode={'input' if args.input else ('frame' if args.frame else ('palette' if args.palette else 'trace'))}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
