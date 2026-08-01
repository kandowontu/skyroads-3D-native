# Reviewed DGROUP state map

Addresses are offsets from the executable's `DS = load_segment + 066E`.
“Verified” means the role follows directly from producers and consumers in the
machine code; “provisional” means the storage and behavior are certain but the
original developer's name is not.

| DS offset | Reconstructed role | Confidence / evidence |
|---:|---|---|
| `0036` | VGA-mode flag | Verified by VGA/EGA startup and renderer branches |
| `0BA0` | current PC-speaker note cursor | Verified by `03C2`, `0476`, and the timer ISR |
| `0BA2..0BAC` | keyboard pressed flags | Verified by keyboard ISR and `074C`; bit 7 is pressed |
| `0BC2` | current music track, `FFFFh` for none | Verified by `57A8` track de-duplication and settings-menu disable path |
| `0C8F` | OPL event delay counter | Verified opcode-0 producer at `58F8` and timer decrement at `5A39` |
| `0CC2` | Sound Blaster DSP base port | Verified by reset/probe and DMA playback code |
| `160C` | gameplay tick counter | Verified by timer-owned increments and `1F2C` synchronization |
| `31A0` | current OPL instrument-table base | Verified by instrument opcode and stream setup at `5A61` |
| `31A2` | current OPL event cursor | Verified by timer fetch and loop commands |
| `31A4` | saved OPL loop cursor | Verified opcodes 5/6 at `5A26`/`5A2D` |
| `31A6` | OPL rhythm register shadow | Verified note-on/off updates written to register `BDh` |
| `31A7..31B1` | selected instrument for OPL channels 0..10 | Verified by instrument and volume opcodes |
| `31B2` | OPL stream signal byte | Verified opcode 7 and reset at stream start |
| `41CC` | gameplay input lock / forced termination state | Verified behavior: zeros live controls and triggers result 2 after 144 locked ticks; producer not yet found |
| `41B6` | centralized DOS I/O error | Verified by all file wrappers and `0069` |
| `4524` | config checksum | Verified by `56C3`, `571B`, and `5770` |
| `4528` | sound/music disabled flag | Verified by effect and OPL dispatch guards |
| `4526` | selected input/game mode | Verified by startup menus and input dispatch |
| `452A..4565` | 30 road completion counters | Verified by `01B8` and the 0x42-byte config block |
| `456C` | Sound Blaster sample-buffer segment | Verified by allocation, `sfx.snd` load, and DMA dispatch |
| `456E` | road gravity | Verified by `55F8` load order and gravity calculation in `1F2C` |
| `4572` | allocation-stack depth | Verified by `3ED8`–`3F56` |
| `4574` | road oxygen parameter | Verified by `55F8` load order and constant oxygen drain |
| `4576` | lateral velocity | Verified by steering assignment and horizontal integration |
| `4578` | post-result tick counter | Verified by crash/termination branches in `1F2C` |
| `457C` | level result code | Verified return source of `1F2C` |
| `457E` | collision trajectory recovery enabled | Verified startup assignment to 1 and sole use guarding `1D4D` during a jump |
| `5180`, `548A` | joystick X/Y centers | Verified by `0707` and `074C` |
| `5488` | jump input | Verified across all four branches of `074C` |
| `54A0` | fuel remaining, initial 30000 | Verified against the road fuel parameter |
| `54A2` | lateral surface impulse | Provisional; updated during collision/landing resolution |
| `54AE` | road fuel parameter | Verified by `55F8` load order and speed-scaled fuel drain |
| `54B6` | per-tick gravity step | Verified formula: `-(gravity * 0x1680 / 400)` |
| `54B8:54BA` | 32-bit forward speed | Verified integration and clamp to `0..0x2AAA` |
| `933C` | throttle/brake input | Verified by `074C` and forward-speed integration |
| `933E` | current road index | Verified by loader selection, completion, and world selection |
| `9342` | vertical velocity | Verified by jump impulse, gravity, and height integration |
| `9600` | steering input | Verified by `074C` and lateral-velocity assignment |
| `9602` | input/game mode (`0..3`) | Verified dispatch: keyboard, joystick, mouse, demo |
| `9620:9622` | 32-bit demo elapsed ticks | Verified by demo timeout branch |
| `9628:962A` | 32-bit 16.16 road distance | Verified road-row and `demo.rec` indexing |
| `962E` | `demo.rec` byte array | Verified loader destination and input decoder |
| `AF2C` | horizontal ship position | Verified road lookup and collision sampling; starts `0x8000` |
| `AF48` | tick when the latest sound effect was requested | Verified by `03C2` and `0476` |
| `AF3C` | ship height | Verified vertical integration and road-clearance tests; starts `0x2800` |
| `B14C` | oxygen remaining, initial 30000 | Verified constant drain and result code 5 |

Several collision temporaries remain address-named until their producers are
fully resolved. Names above describe only behavior demonstrated by the binary;
they do not claim the original developers' identifiers.
