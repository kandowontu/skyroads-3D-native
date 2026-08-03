# SkyRoads Native 1.0.1

SkyRoads Native 1.0.1 is a compatibility hotfix for the first stable Windows
release of the executable-authoritative SkyRoads reconstruction. The original
DOS executable, data formats, fixed-point behavior, timing, and captured VGA
output remain the reference; native features are layered around that recovered
core.

## 1.0.1 compatibility fix

The original 1.0 package unintentionally depended on `libgcc_s_seh-1.dll`,
`libstdc++-6.dll`, and `libwinpthread-1.dll`. Version 1.0.1 statically links the
compiler, C++, threading, and OPL synthesis runtimes. The official executable's
PE import table now contains only `KERNEL32.dll`, `USER32.dll`, `GDI32.dll`, and
`WINMM.dll`, all provided by Windows. No runtime installer or extra DLL is
needed.

## Installation

1. Download the Windows x64 package from the GitHub release.
2. Obtain SkyRoads or SkyRoads Xmas Special from
   [BlueMoon's official SkyRoads page](http://www.bluemoon.ee/history/skyroads/).
3. Extract the original game and place `skyroads_native.exe` beside
   `SKYROADS.EXE` or `SKYXMAS.EXE`.
4. Run `skyroads_native.exe`.

BlueMoon also provides direct downloads for the
[full original SkyRoads](http://www.bluemoon.ee/history/skyroads/skyroads.zip)
and [full Xmas Special](http://www.bluemoon.ee/history/skyroads/skyxmas.zip).
No loose `.LZS`, `.DAT`, `.SND`, or `.REC` files are required by the native
executable at runtime. The compiler, C++, threading, and audio-synthesis
runtimes are statically linked; no Visual C++ or MinGW runtime installation is
needed.

## Native enhancements

- Combines the 30 original roads and 30 Xmas Special roads in a four-column,
  60-level selector with each campaign's original world artwork.
- Adds optional high-definition presentation using smoothly shaded native
  polygons while retaining the original 320x200 indexed simulation beneath it.
- Plays the recovered OPL2 event stream through a native synthesizer and mixes
  sound effects without interrupting the music.
- Adds a visual built-in level editor with mouse painting, exact recovered cell
  materials, editable physics/resources, and immediate play-testing.
- Adds top, straight, left-isometric, and right-isometric editor views so block
  heights, walls, and ramps can be inspected spatially.
- Imports all 30 base-game roads into an `ORIGINAL LEVELS` editor folder as
  `1-1` through `10-3`, while preserving edited copies.
- Includes two custom demonstration roads with safe fuel and oxygen budgets.
- Restores the original finish-tube coast before the level-complete result.
- Preserves the original menu background and styling while integrating the
  editor and high-definition option.
- Adds optional `Ctrl+F9` through `Ctrl+F12` in-level shortcuts for 200% speed,
  zero gravity, resource refill, and mid-air jumping/steering.
- Ships as one self-contained executable with no third-party runtime DLLs.

## Fidelity and verification

- All 7 native integration suites and all 29 reconstructed-core suites pass.
- All 1,702 captured DOS VGA gameplay frames match byte-for-byte.
- All 635 DOS-observed changed VGA palette states match byte-for-byte and in
  their original order.
- Original demo input, physics, collisions, menu timing, palette transitions,
  finish timing, music scheduling, and rendering are covered by deterministic
  executable-authoritative regressions.

## Documentation and credits

- [Level editor manual](https://github.com/kandowontu/skyroads-3D-native/blob/v1.0.1/EDITOR_README.md)
- [Complete original and native project credits](https://github.com/kandowontu/skyroads-3D-native/blob/v1.0.1/CREDITS.md)
- [Third-party notices](https://github.com/kandowontu/skyroads-3D-native/blob/v1.0.1/THIRD_PARTY_NOTICES.md)

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
