# SkyRoads DOS resource reverse engineering

This workspace contains the original DOS distribution plus generated, decoded
artifacts under `extracted/`. The original files are never modified.

## Reproduce the extraction

From PowerShell:

```powershell
.\tools\extract_all.ps1
```

The wrapper pins the extractor implementation to Git commit
`4c5917392b6892792c06336df019152a81cdffab`, runs it against this directory,
and then creates PNG previews, contact sheets, road maps, and `extracted/index.html`.

Requirements are Git, Python, and Pillow. The bundled machine currently has all
three. The extractor implementation is fetched into the system temporary folder
rather than copied into this project; its upstream repository is
[SkyRoads-Codex](https://github.com/ammaarreshi/SkyRoads-Codex).

## Verified input build

| File | SHA-256 |
| --- | --- |
| `skyroads.exe` | `c3a55223e359749555535e138ef4219bc94458639e44f70edf3cfcb2f28c26ac` |
| `roads.lzs` | `eae3754bcbf9db20f076e330038d29401b2b5eb400bb148d772edb4ba23442ce` |
| `trekdat.lzs` | `513b572fc159f6e8219fd3b53f5f8e51df71ef0df7e4e6c72219b08755f3819b` |
| `muzax.lzs` | `be741b53f33a1d7a488b1a66d7df4a20ca44c4d7a252a2984e2325f7a8b0d8a4` |

## Container findings

### Compression

SkyRoads uses a bit-packed LZSS variant. Each compressed stream begins with
three bit widths. Tokens are read most-significant-bit first:

- `0`: short history copy; distance uses width 2 and count uses width 1.
- `10`: long history copy; distance uses width 3 plus the short-distance range.
- `11`: an eight-bit literal.

All 31 road streams and all recognized image, renderer, and music streams expand
to their declared sizes with valid history references.

### Image archives

Image resources contain `CMAP` palette chunks and `PICT` frames. `ANIM.LZS` adds
an `ANIM` prefix and inter-frame metadata. Pixels are 8-bit palette indices;
palette channels are VGA six-bit values scaled to RGB888 for previews.

Decoded inventory:

- 100 declared animation frames composed from 221 positioned `PICT` fragments
- one 24 x 2310 ship/explosion strip
- two dashboard frames
- 31 menu/intro frames across the UI archives
- ten 320 x 138 world backgrounds
- two EXE-embedded HUD sets: digits and Jumpmaster indicators

### Roads

`ROADS.LZS` begins with 31 `(offset, decompressed_size)` entries. Entry zero is
the attract-mode demo road; entries 1-30 are the playable roads. Each entry has
gravity, fuel, oxygen, a 72-color VGA palette, and a compressed grid of seven
little-endian 16-bit descriptors per row.

The descriptor includes the bottom surface color, top color, tunnel flag,
half-height block flag, and full-height block flag. Six renderer dispatch kinds
(`0` through `5`) occur in the shipped levels. Decoded lengths range from 48 to
237 rows.

### Audio and music

- `INTRO.SND` is 32,100 unsigned 8-bit mono PCM samples at 8 kHz (4.0125 s).
- `SFX.SND` begins with a six-entry offset table. Five entries contain unsigned
  8-bit mono PCM; the sixth is a zero-length sentinel.
- `MUZAX.LZS` has a 20-entry song table. Fourteen entries are populated. Each
  expands into 16-byte OPL instrument definitions followed by two-byte music
  commands. These are preserved as instrument/command binaries and analyzed
  JSON, but are not yet rendered to WAV because faithful playback requires the
  original scheduler and OPL timing.

### Renderer and executable

`TREKDAT.LZS` contains eight compressed records. Each expands into a 13 x 24
pointer table and referenced horizontal-span shapes used by the software road
renderer. The output includes expanded buffers, pointer tables, parsed shapes,
and visual sheets.

The MZ executable contains two relocations and a 29,960-byte load module. The
extraction records its entry point, selected physics/demo constants, renderer
dispatch tables, and embedded HUD images. The reports in `extracted/exe/reports/`
separate verified addresses from interpretation.

## Output guide

- `extracted/index.html`: local visual/audio browser
- `extracted/images/`: indexed pixels, VGA palettes, PNG/PPM frames, sheets, GIF
- `extracted/roads/`: raw tiles, per-road JSON, palettes, overview PNGs
- `extracted/sounds/`: raw unsigned PCM and playable WAV files
- `extracted/muzax/`: decompressed OPL instruments and command streams
- `extracted/trekdat/`: renderer records, pointers, shapes, and previews
- `extracted/dats/`: dashboard indicator fragments and composites
- `extracted/demo/`: byte-for-byte recording plus decoded per-tick input JSON
- `extracted/exe/`: DOS load module, embedded images, tables, and reports

## Fidelity gaps before a native port is exact

The assets are decoded, but an exact port still needs runtime behavior recovered
from the executable: fixed-point ship physics, collision state transitions,
TREKDAT painter order and clipping, demo tick rate, OPL command scheduling, menu
state timing, and palette-change timing. Those should be validated against DOS
captures rather than inferred from screenshots.

Format references:

- [SkyRoads formats](https://moddingwiki.shikadi.net/wiki/SkyRoads)
- [SkyRoads compression](https://moddingwiki.shikadi.net/wiki/SkyRoads_compression)
- [SkyRoads image format](https://moddingwiki.shikadi.net/wiki/SkyRoads_Image_Format)
- [SkyRoads level format](https://moddingwiki.shikadi.net/wiki/SkyRoads_level_format)
