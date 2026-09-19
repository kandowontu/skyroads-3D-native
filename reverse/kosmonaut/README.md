# Kosmonaut executable reconstruction

This directory records the evidence used for the native Kosmonaut game mode.
The runtime does not open, launch, checksum, or otherwise depend on
`KOSMO.EXE`. The executable was used only as the authority for the one-time
reverse engineering and neutral-resource extraction.

## Verified source image

The supplied `KOSMO.EXE` is a Microsoft EXEPACK-compressed MZ program. After
unpacking its load image, the verified SHA-256 is:

`6d91a0a34c71fc771398603b6f489b7dbb3e6923900d6b6011f36030cb627111`

The unpacked reference executable remains local and is excluded from source
control. `extract_assets.py` emits data, never executable instructions. Its
manifest and binary outputs under `extracted/kosmonaut/` are the only inputs to
the native build.

## Recovered layout

All segment values are the original link-time paragraphs, before a DOS loader
adds its runtime base.

| Content | Far location | Native result |
| --- | --- | --- |
| Main game loop | `0000:201C` | gameplay state machine in `src/kosmonaut_game.cpp` |
| Input decoder | `0000:1226` | keyboard and position-indexed demo controls |
| Slab effects | `0000:0C5E` | fuel, oxygen, speed, jump, hazard behavior |
| Perspective renderer | `05CF:0052` | six-lane native cockpit renderer |
| Road copy | `05CF:0C63` | 1,200-byte road records |
| 26 road records | `1711:08B2` | `roads.bin`, 200 rows by six lanes each |
| Animated-road records | `1675:00E0` | `level-effects.bin`, 0x32 bytes per road |
| Gauge/collision tables | `1675:05F4` | `simulation-tables.bin` |
| Player craft frames | `10DC:0001` | `ship-sprites.bin`, 98 transparent 26x9 frames |
| Renderer edge masks | `05CF:2921` | `render-masks.bin` |
| Renderer descriptors | `05CF:2B21` | `render-pointers.bin` and `render-shapes.bin` |
| Font | `40DD:0138` | `font.bin`, 87 eight-row glyphs |
| Font scanline styles | `40DD:0110` | `font-styles.bin`, five eight-row styles |
| Demo controls | `40DD:06B2` | `demo-input.bin`, indexed through position `02F0h` |
| EGA palette map | `05CF:0BA6` | 16 EGA register values |
| Tutorial music | `05CF:0D42` | tempo/release plus 576 PIT notes and durations |
| Selector music | `05CF:1648` | tempo/release plus 544 PIT notes and durations |
| Moving star paths | `05CF:04C7`, `08F3`, `2E39`, `2EA7` | star motion/RNG/path records |

Five complete 320x200, 16-color EGA frames are recovered. The road-selection
and cockpit images share a 139-row upper section and have separate 61-row
panels in segment `1F3A`. The tutorial and title screens are in segment `296C`;
the final completion portrait is in segment `390C`.

## Recovered simulation constants

- 26 roads, each 200 forward rows by six lanes
- start position `0004h`, finish position `02F0h`
- lateral start `00C0h`, bounds `0000h..016Dh`, lane pitch `0032h`
- altitude floor `0009h`, gravity velocity step `-0020h`
- speed `0000h..00FFh`, acceleration and braking steps of five
- fuel and oxygen start at 32,000; jump power starts at `0078h`
- forward distance advances through an 8-bit `00FFh` fractional accumulator

The native source preserves the recovered per-slab rules, position-indexed
demo commands, animated map substitutions, pause/demo paths, road unlocking,
the original EGA screens/font/palette, and PC-speaker-style events. Native save
progress is stored in the original 0x110-byte, XOR-obfuscated `HISCORES.SKY`
format.

The host schedules Kosmonaut at approximately 18 Hz, independently of
SkyRoads' 36 Hz simulation. This matches the timing observed with the DOS
reference configured for a 3,000-cycle, 386-class machine. The craft is drawn
at the original insertion point inside the renderer's two perspective passes;
near raised slabs and tunnel faces therefore occlude it as they did on EGA.

## Reproduce the neutral extraction

After producing an unpacked MZ reference image locally:

```powershell
python reverse\kosmonaut\extract_assets.py `
    C:\path\to\KOSMO_UNPACKED.EXE extracted\kosmonaut
```

CMake runs `tools/embed_kosmonaut_data.py` only on the extracted `.bin` files.
This makes a clean native rebuild independent of every Kosmonaut DOS
executable.
