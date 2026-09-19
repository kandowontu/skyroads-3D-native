# SkyRoads executable reconstruction

> This is an unofficial native Windows reconstruction. SkyRoads and its
> original assets remain the work of BlueMoon Software/Bluemoon Interactive.
> See [the original credits](CREDITS.md).

The active porting path is a reviewed C reconstruction of `skyroads.exe`, not a
gameplay approximation. The DOS executable and shipped data files are the
authority. Generated decompiler output is evidence; only manually checked code
is admitted to the reconstructed library.

The Win32 target under `src/` now directly links and runs that reconstruction.
Its responsibilities are limited to presenting the recovered 320x200 indexed
framebuffer, supplying devices/timing, and playing recovered sample events; the
old approximate gameplay and perspective renderer are no longer on the active
path. The host also streams the recovered OPL register schedule through a
native OPL2 synthesizer.

The same executable also contains a native reconstruction of Kosmonaut, the
earlier six-lane cockpit game that led to SkyRoads. Choose `KOSMONAUT` on the
original-style title menu to enter its recovered title, road selector, and
complete 26-road campaign. Its five EGA screens, 98 original ship frames,
custom font and scanline color styles, palette, road maps, animated-road
records, moving-star paths, PC-speaker scores, span-renderer tables, and demo
controls are embedded as neutral data;
the port never needs or runs `KOSMO.EXE`. See the
[Kosmonaut reconstruction notes](reverse/kosmonaut/README.md).

## SkyRoads Native 1.1.0

The first stable release combines the executable-authoritative DOS simulation
with optional native enhancements: both 30-road campaigns in one selector,
smooth high-definition presentation, recovered OPL2 audio, a visual road
editor, spatial editor views, and optional in-level shortcuts. See the complete
[1.1.0 release notes](RELEASE_NOTES.md) and [editor manual](EDITOR_README.md).
Version 1.1.0 adds automatic Xbox/XInput controller support, the native
Kosmonaut campaign, an Options menu, and complete mesh-based widescreen and
ultra-widescreen rendering for SkyRoads and Xmas. The Windows executable
remains self-contained.

## Original game requirement

This repository does not include either DOS executable. Release builds embed
the recovered game resources but deliberately omit executable code from both
original programs. To run the combined port, place
`skyroads_native.exe` beside a legally obtained original copy of either:

- `SKYROADS.EXE`, or
- `SKYXMAS.EXE`.

BlueMoon still hosts the original releases on its official history page:

- [Official SkyRoads history and download page](http://www.bluemoon.ee/history/skyroads/)
- [Download the full original SkyRoads](http://www.bluemoon.ee/history/skyroads/skyroads.zip)
- [Download the full SkyRoads Xmas Special](http://www.bluemoon.ee/history/skyroads/skyxmas.zip)

The site is a preserved period website served over HTTP. Download and extract
either official archive, then put `skyroads_native.exe` beside its DOS
executable. Do not rename the original executable.

The Windows host deliberately checks only the folder containing the running
native executable. It does not search parent directories or silently use a
different installation. No loose `.LZS`, `.DAT`, `.SND`, or `.REC` files are
required at runtime, and the compiler, C++, threading, and synthesis runtimes
are linked into the release executable. Users do not need to install Visual C++,
MinGW, or any other support library. Startup stops with an explanatory message
when neither original executable is present.

Kosmonaut itself has no ownership-file or data-file requirement. The existing
`SKYROADS.EXE`/`SKYXMAS.EXE` check remains the combined application's SkyRoads
ownership check; no `KOSMO.EXE` is accepted or requested by it.

## Built-in Kosmonaut campaign

Choose `KOSMONAUT` on the lower row of the main menu. The recovered Kosmonaut
title uses the original controls: `Space`/`Enter` advances to the tutorial and
road selector, arrows choose among unlocked roads, `Space` starts, `P` pauses,
`S` toggles sound, and `Escape` backs out. During the tutorial, `D` starts the
original recorded demonstration. Progress uses the original obfuscated
`HISCORES.SKY` layout beside the native game.

Kosmonaut begins with roads 1 and 2 available and unlocks the next road after
a successful run, through all 26 executable-resident maps. The cockpit,
tutorial art, road-selection craft, title logo, completion portrait, 16-color
EGA palette, position-indexed demo, custom glyphs, road flashing, fuel/oxygen,
jump-power, speed, hazards, gaps, raised blocks, moving starfield, and both
original PC-speaker songs all come from the recovered DOS layout. Unlike
SkyRoads mode, Kosmonaut does not offer Hi-Def rendering;
its native view intentionally preserves its original EGA presentation.

Building from source still requires legally obtained SkyRoads archives so CMake
can generate the embedded-data translation unit. Point the cache variables at
the two original installations when they are not in the default locations:

```powershell
cmake -S . -B build -G Ninja `
    -DSKYROADS_DATA_DIR="C:\Games\SkyRoads" `
    -DSKYROADS_XMAS_DATA_DIR="C:\Games\SkyRoads Xmas"
cmake --build build
```

Then copy `build\skyroads_native.exe` beside either original DOS executable and
run it there.

## Current recovered source

[`reverse/reconstructed/`](reverse/reconstructed/) contains reviewed C for:

- MZ startup order, allocation scopes, configuration checksum, and hardware probes
- the exact LZS decoder and streamed `CMAP`, `PICT`, `ANIM`, road, TREKDAT, car,
  and dashboard-DAT resource layouts
- all four input paths, including byte-exact `demo.rec` playback
- fixed-point road lookup, physics, ship profiles, collision subdivision,
  landing prediction/recovery, fuel, oxygen, and level outcomes
- gameplay frame-parameter selection and the VGA TREKDAT road/car draw traversal
- the self-modifying DOS picture-blitter semantics and asset-driven VGA
  world/dashboard, gauges, warnings, progress, gravity, and jumpmaster rendering
- exact main/settings/help/30-level selection dispatch and menu-cell geometry,
  plus a native four-column extension for the 30 SkyRoads Xmas levels
- exact intro/demo/main-menu/run-level control flow, including every original
  timed hold and 36 Hz palette-transition phase
- all five PC-speaker/PCM effects plus the complete 14-track `muzax.lzs`
  OPL instrument/event scheduler and original register programming order

[`reverse/symbols.json`](reverse/symbols.json) is the reviewed address/name map.
[`reverse/generated/`](reverse/generated/) is reproducible output and is ignored
by source control.

## Build and verify the reconstruction

```powershell
cmake -S reverse\reconstructed -B reverse\generated\reconstructed-build -G Ninja
cmake --build reverse\generated\reconstructed-build
ctest --test-dir reverse\generated\reconstructed-build --output-on-failure
```

The standalone suite currently has 29 deterministic regressions for original
archives, palette arithmetic, control-flow timing, the full demo-driven gameplay
trace, renderer parameters, and asset-driven VGA scenes. One regression compares
all 1,702 serialized gameplay states and 51 Sound Blaster-filtered effect events
against a trace captured directly from the DOS executable. A second executable
oracle compares 1,241 effective OPL2 register changes across 8,510 original
180 Hz scheduler ticks. The root build also has native integration and OPL PCM
tests. The DOS framebuffer probe captures all 1,702 complete 320x200 indexed
VGA gameplay frames in the original demo. The native reconstruction matches all
108,928,000 captured pixel bytes, including persistent road-buffer transfers,
TREKDAT mutations, ship clipping, shadows, and dashboard updates. Additional
executable probes cover all 2,048 keyboard masks, 648 joystick threshold and
16-bit-overflow cases, 512 ordered mouse vectors, and 635 distinct VGA DAC
states observed from the intro through the demo.

## SkyRoads Xmas campaign

When the Xmas archives are supplied at build time, the native port embeds its
30 roads as levels 31–60. The first two selector columns contain the original
SkyRoads campaign and the final two contain SkyRoads Xmas. Gameplay loads each
campaign's embedded `ROADS.LZS` records, VGA palettes, and ten `WORLD*.LZS`
backgrounds through the same recovered renderer.
Each world entry also uses the thumbnail cropped from that campaign's original
`GOMENU.LZS` artwork; the images are reduced with nearest-neighbor sampling for
the four-column 320x200 layout.

Xmas completion counts are stored separately in `SKYXMAS.CFG`; the original
30-level `SKYROADS.CFG` layout is left compatible and unchanged.
Every selector road shows its saved completion count as a digit. New roads
start at `0`, and counts above nine are displayed as `9` in the compact layout.
The count is bright yellow so it remains distinct from numeric road names.

## Built-in custom road editor

Choose `EDITOR` on the lower row of the main menu to open the creation browser.
It scans `custom_levels/*.srlevel`, lists every saved creation with
paging, and includes the new `SKYBRIDGE RUN` and `NEBULA SLALOM` demo roads.
The `ORIGINAL LEVELS` folder contains editable copies of all 30 decoded base-game
roads, named `1-1` through `10-3`. Missing copies are imported from the embedded
`ROADS.LZS` records at startup; valid edited copies are preserved between runs.
Custom files preserve the recovered seven-column, 16-bit road-cell descriptors
used directly by the original renderer, collision code, and physics.

In the editor, the arrow keys move through the road grid. Press `V` (or click
the view control) to cycle between the editable top-down grid, left isometric,
straight road, and right isometric views. The spatial views render the recovered
low, middle, and high shape tiers as solid blocks and show ramps as slopes.
The numbered material palette or its clickable color swatches selects a road,
gap, slow, slide, refill, boost, or terminal brush; `Space` paints in every view,
and a click also paints in the top-down grid.
`T` cycles the six recovered road-shape silhouettes, and `W` changes the
original world theme. `G`, `F`, and `O` adjust gravity,
fuel usage, and oxygen usage. `Insert` duplicates a row, `Delete` removes one,
`S` saves, and `P` saves and immediately play-tests the creation through the
recovered game loop. `Escape` returns to the creation browser.

The full browser, painting, spatial-view, metadata, file-location, restoration,
and play-testing instructions are in [EDITOR_README.md](EDITOR_README.md).

## Native display and shortcut extensions

Choose `OPTIONS` on the authentic-styled lower main-menu row to configure the
native display extensions. `HI-DEF POLYGONS` makes the recovered TREK renderer
report every individual road, wall, ramp, and tunnel face in its original
painter order. The host rerasterizes those faces as smooth high-resolution
polygons and presents each completed geometry frame
atomically through a double buffer. The player craft is replaced by a palette-
matched polygon model clipped by the recovered road-visibility mask, so
foreground tunnels occlude it correctly. World artwork and dashboard graphics
remain pixel-authentic rather than filtered. The verified 320x200 indexed frame
and fixed-point simulation remain unchanged underneath.

The `DISPLAY MODE` row offers the undistorted original 8:5 presentation,
16:9 widescreen, and 21:9 ultra-widescreen. SkyRoads and Xmas wider gameplay
modes build complete meshes for the road, tile edges, low/high blocks, arches,
and tunnel openings from the level cells. The original TREK tables calibrate
depth; complete faces are clipped against the expanded camera frustum before
perspective division and depth-tested, including the player. The cockpit is
composited over the scene using its own silhouette. Hi-Def renders the expanded
field at the window resolution; classic mode keeps the original 200-line style
and recovered sprite, including outside the old viewport. Background and
cockpit artwork remain bitmaps. Fixed-size menu artwork remains centered rather than
inventing interface art. Both display choices are stored in
`SKYROADS.NATIVE.CFG`. The original Controls screen is again reserved for the
five original input and sound settings.

Press `Alt+Enter` to switch between the resizable window and borderless
fullscreen on the current monitor. Press `Escape` on the main menu to quit.

Xbox-compatible XInput controllers are detected automatically, including when
connected after launch. No Controls setting or additional library installation
is required. The first connected controller is used; disconnected controllers
are checked once per second. Input is accepted only while the game is focused.

| Context | Controller controls |
| --- | --- |
| Menus | D-pad/left stick: navigate; A/Start: confirm; B/Back: return |
| SkyRoads / Xmas | D-pad/left stick: steer, accelerate (up), brake (down); A: jump; B/Back/Start: leave road |
| Kosmonaut | D-pad/left stick: drive; A: jump/confirm; Start: pause/resume; B/Back: return |
| Editor | D-pad/left stick: cursor; A: paint; X: play-test; Y: shape; Start: save; LB/RB: page; left-stick click: view; B/Back: return |

The left stick has a dead zone to prevent drift, and menu directions repeat
after a short hold. Keyboard controls remain available alongside the pad.

During a level, the following native shortcuts are available:

- `Ctrl+F12` toggles mid-air jumping and full left/right air steering.
- `Ctrl+F11` immediately refills fuel and oxygen.
- `Ctrl+F10` toggles zero gravity.
- `Ctrl+F9` toggles a 200% recovered-physics speed ceiling.

## Port scope

The native target intentionally follows the original VGA branch. The legacy
EGA compatibility renderer is documented in the disassembly and symbol map but
is not part of this port.

The OPL2 backend uses the pinned public-domain/MIT Opal core described in
[`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).

Capture and run the framebuffer diagnostic with:

```powershell
.\reverse\tools\capture_dos_frame_oracle.ps1
cmake -S . -B build -G Ninja -DSKYROADS_ENABLE_DOS_FRAME_ORACLE=ON
cmake --build build
ctest --test-dir build -R skyroads_dos_vga_frames --output-on-failure
```

Capture the palette and controlled raw-input diagnostics with:

```powershell
.\reverse\tools\capture_dos_palette_oracle.ps1
.\reverse\tools\capture_dos_input_oracle.ps1
```

Re-run the executable analysis with:

```powershell
.\reverse\tools\run_ghidra.ps1
```
