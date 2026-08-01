# Reviewed source reconstruction

This tree contains manually reviewed C derived from `skyroads.exe`. It is kept
separate from both generated Ghidra evidence and the Win32 presentation/device
host that links it.

Each routine names its executable address. Code enters this directory only when
the machine instructions, calling sequence, and data behavior agree. Naming is
based on verified callers and effects rather than visual resemblance.

[`state_map.md`](state_map.md) records the global fixed-point/input/resource
state recovered from DGROUP, along with the evidence level for each name.

Currently reviewed:

- `allocation.c`: the game's scoped DOS-segment allocation stack
  (`1000:3ED8`–`1000:3F56`), including its value-1 sentinels.
- `config.c`: the exact 0x42-byte `SKYROADS.CFG` checksum routine
  (`1000:56C3`).
- `lzs.c`: `1000:6660`, including the three field widths, MSB-first token tree,
  overlapping history copies, exact output boundary, and final byte alignment.
- `hardware.c`: VGA/EGA BIOS probes (`1000:5FFD`, `1000:5FE9`) and PIT seed
  sampler (`1000:019C`).
- `input.c`: the complete four-mode keyboard/joystick/mouse/demo sampler
  (`1000:074C`), including the original thresholds, 16-bit intermediate
  overflow, ordered mouse reads/recenter, and demo bit packing.
- `road.c`: fixed-point road lookup, executable-resident ship collision
  profiles, collision predicate, and movement subdivision/refinement
  (`1000:04C0`, `1000:1584`, `1000:1685`, `1000:17BE`).
- `road_archive.c`: the indexed `roads.lzs` record loader (`1000:55F8`),
  including the three parameters, 216-byte VGA palette, exact LZS payload,
  and seven 16-bit cells per row.
- `physics.c`: verified scalar portions of the gameplay loop (`1000:1F2C`):
  gravity, forward-speed clamping, airborne velocity, fuel/oxygen drain, and
  the low-nibble slow/refill/boost/end cell effects (`1000:1A9C`).
- `motion.c`: the complete landing predictor and its ordered lateral/speed
  collision-recovery search (`1000:1C20`, `1000:1D4D`).
- `gameplay.c`: the exact initialization, fixed-timer inner tick, landing and
  bounce transitions, resource/result handling, delayed-result predicate, and
  72-tick finish-tube coast from the gameplay loop (`1000:0E58`, `1000:1F2C`).
  Platform input/render/pause handling is kept outside this deterministic core.
- `render_params.c`: the exact gameplay-frame sprite, attitude, lane, road
  phase, surface-clearance, and car-frame selectors (`1000:0AFA`–`1000:0BE3`)
  that feed the VGA/EGA road renderer.
- `trek_archive.c`: the eight-record `trekdat.lzs` loader and its in-place
  1,040-shape span expansion (`1000:00BB`, `1000:3A7A`).
- `graphics_archive.c`: the streamed `CMAP` palette/dither-table and `PICT`
  LZS image loader, including the VGA nontransparent palette-base adjustment
  (`1000:3F75`, `1000:4036`, `1000:4068`).
- `animation_archive.c`: the `ANIM` group table, shared color map, and all 221
  grouped intro picture records consumed by the intro sequence (`1000:4575`).
- `intro.c`: the VGA intro routine's exact 36 Hz presentation schedule,
  including its animation-group waits, logo scanline reveal, palette fades,
  credit-card holds, sample trigger, fade-out, and any-key abort (`1000:4575`).
- `keyboard.c`: drains pending BIOS key events and latches the intro-abort flag
  exactly as the shared wait/poll helper does (`1000:4137`).
- `palette.c`: the signed-IDIV palette interpolation and exact percentage steps
  used by every timed VGA fade (`1000:4315`, `1000:4B72`).
- `bios_font.c`, `level_flow.c`: the BIOS-bank-3 8x8 text pixels used by the
  completion messages and `run_level`'s exact 36/27/36-tick entry, success-hold,
  and exit sequence (`1000:2B21`, `1000:3C36`, `1000:450A`).
- `display_table.c`: the offset-table and uncompressed dashboard sprite records
  loaded from oxygen, fuel, and speed DAT files (`1000:5465`, `1000:0EDF`).
- `picture_blitter.c`: the shared VGA row-composition protocol, including the
  original's self-modified transparency cutoff and reference-buffer restore
  interval (`1000:4162`–`1000:4293`).
- `embedded_hud.c`, `dashboard.c`: executable-resident digit/jumpmaster bitmaps,
  world/dashboard composition, gravity display, delta gauges, warning lamps,
  progress strip, and jumpmaster state (`1000:0EB5`–`1000:1464`).
- `car_sprites.c`, `renderer_tables.c`, `renderer_vga.c`: the exact `cars.lzs`
  sprite sheet, executable-resident ship clipping/shadow tables, and a direct
  linear-framebuffer translation of the VGA road descriptor traversal, six draw
  dispatches, two TREKDAT span primitives, persistent road-buffer delta
  transfers, 29-by-33 visibility/occupancy mask, all 74 executable-resident
  descriptor color mappings, 29-by-24 car blit, and five-frame road-shadow transform
  (`1000:2D03`–`1000:3461`).
- `sound_effects.c`: the five `sfx.snd` offset-delimited PCM records, five
  executable-resident zero-terminated PC-speaker divisor streams, exact effect
  dispatch, speaker cursor advancement, and eight-game-tick sampled-effect
  activity test (`1000:03C2`, `1000:0476`, `1000:3B18`).
- `menus.c`: exact main-menu activation, settings-grid navigation/application,
  two-page help behavior, 30-level selector movement, completion-marker cap,
  and selector/marker coordinates (`1000:4C04`–`1000:536A`).
- `menu_flow.c`: the blocking menu-call wrapper translated into a deterministic
  state machine, including each 36-tick fade and the exact main/settings/help/
  level-selector return routes (`1000:4C04`–`1000:536A`).
- `music.c`: all fourteen `muzax.lzs` directory records and the complete eight-
  opcode OPL stream interpreter, including 16-byte instruments, melodic and
  rhythm-channel note handling, 31-step volume table, loop commands, preserved
  inter-track delay, and initialization/silence register order
  (`1000:57A8`–`1000:5A7A`).
- `startup.c`: the complete reachable control flow of game `main`
  (`1000:01B8`), with every directly reached game callee identified.

Build and verify independently of the native host:

```powershell
cmake -S reverse\reconstructed -B reverse\generated\reconstructed-build -G Ninja
cmake --build reverse\generated\reconstructed-build
ctest --test-dir reverse\generated\reconstructed-build --output-on-failure
```

`recovered_demo_trace` feeds `demo.rec` through the recovered sampler and
fixed-point tick against original road record 0. Its whole-state trace is kept
as a deterministic regression vector. `recovered_dos_oracle` additionally
compares all 1,702 ticks with the executable's byte-for-byte state and filtered
effect traces, while `recovered_dos_opl_oracle` compares the effective OPL2
register stream over the same demo run. The root `skyroads_frame_oracle_tests`
target compares all 1,702 indexed VGA frames—108,928,000 pixels—with a capture
from the DOS renderer and currently matches byte-for-byte.
`recovered_dos_input_oracle` additionally compares all 2,048 key masks, 648
joystick boundary vectors, and 512 ordered mouse vectors directly with
`1000:074C`; the root palette oracle covers all 635 distinct DAC states observed
from the executable intro through the demo.
