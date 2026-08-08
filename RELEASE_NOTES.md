# SkyRoads Native 1.0.2

SkyRoads Native 1.0.2 is the second update to the executable-authoritative
Windows reconstruction. It keeps the recovered DOS simulation, timing, data
formats, and VGA output as the reference while improving the optional native
presentation and editor around that verified core.

## What is new in 1.0.2

- Replaces the Hi-Def screen filter with high-resolution polygons generated
  from the original TREK road, wall, ramp, and tunnel geometry in recovered
  painter order.
- Adds a palette-matched polygon player craft in Hi-Def mode. The recovered
  road-visibility mask now clips the craft correctly beneath foreground
  tunnels.
- Presents completed Hi-Def polygon frames atomically through the existing
  Windows back buffer, eliminating the discontinuous hybrid-frame flicker.
- Adds `Alt+Enter` borderless fullscreen support.
- Makes `Escape` quit from the main menu instead of entering level selection.
- Adds saved completion digits from `0` through `9` to the four-column level
  selector. Counts are bright yellow so they cannot be confused with numeric
  road names.
- Refines the title-screen Editor lettering to match the original menu style.
- Corrects the editor's handling of original shape-only road descriptors such
  as `0x0200` and `0x0400`; level 10-3 now shows its obstacle heights in the
  top, straight, and both isometric views.
- Adds direct snapshot coverage for original level 10-3 and regression coverage
  for shape-only geometry, polygon ship clipping, completion colors, and the
  recovered polygon scene.

## Existing native enhancements

- Combines the 30 original roads and 30 Xmas Special roads in a four-column,
  60-level selector with each campaign's original world artwork.
- Plays the recovered OPL2 event stream through a native synthesizer and mixes
  sound effects without interrupting the music.
- Includes a visual built-in level editor with mouse painting, exact recovered
  cell materials, editable physics/resources, immediate play-testing, and top,
  straight, left-isometric, and right-isometric views.
- Imports all 30 base-game roads into an `ORIGINAL LEVELS` editor folder as
  `1-1` through `10-3` while preserving edited copies.
- Restores the original finish-tube coast before the level-complete result.
- Supports optional `Ctrl+F9` through `Ctrl+F12` in-level shortcuts for 200%
  speed, zero gravity, resource refill, and mid-air jumping/steering.

## Installation

1. Download the Windows x64 package from this GitHub release.
2. Obtain SkyRoads or SkyRoads Xmas Special from
   [BlueMoon's official SkyRoads page](http://www.bluemoon.ee/history/skyroads/).
3. Extract the original game and place `skyroads_native.exe` beside
   `SKYROADS.EXE` or `SKYXMAS.EXE`.
4. Run `skyroads_native.exe`.

BlueMoon also provides direct downloads for the
[full original SkyRoads](http://www.bluemoon.ee/history/skyroads/skyroads.zip)
and [full Xmas Special](http://www.bluemoon.ee/history/skyroads/skyxmas.zip).
No loose `.LZS`, `.DAT`, `.SND`, or `.REC` files are required at runtime.

The Windows x64 executable statically links the compiler, C++, threading, and
audio-synthesis runtimes. Its PE import table contains only Windows system
libraries, so users do not need Visual C++, MinGW, or another runtime package.

## Fidelity and verification

- All 8 native integration suites pass under both MinGW and MSVC x64.
- All 29 reconstructed-core suites pass.
- All 1,702 captured DOS VGA gameplay frames match byte-for-byte.
- All 635 DOS-observed changed VGA palette states match byte-for-byte and in
  their original order.
- Original demo input, physics, collisions, menu timing, palette transitions,
  finish timing, music scheduling, and rendering remain covered by deterministic
  executable-authoritative regressions.

## Documentation and credits

- [Level editor manual](https://github.com/kandowontu/skyroads-3D-native/blob/v1.0.2/EDITOR_README.md)
- [Complete original and native project credits](https://github.com/kandowontu/skyroads-3D-native/blob/v1.0.2/CREDITS.md)
- [Third-party notices](https://github.com/kandowontu/skyroads-3D-native/blob/v1.0.2/THIRD_PARTY_NOTICES.md)

Original SkyRoads credits:

- Programming: Ahti Heinla, Priit Kasesalu, and Jaan Tallinn
- Graphics and artwork: Kaspar P. Loit
- Music: Ott M. Aaloe
- Intro music: Hasso Brück
- Special thanks: Priit Kull and Paul Varney
- Original publisher: Creative Dimensions

SkyRoads and its original assets are the work of BlueMoon Software/Bluemoon
Interactive and the original team above. BlueMoon's
[official history page](http://www.bluemoon.ee/history/skyroads/) is the source
for the original game downloads and historical context. SkyRoads Native is an
unofficial preservation project and is not endorsed by the original developer.
