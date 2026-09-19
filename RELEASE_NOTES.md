# SkyRoads Native 1.1.0

Xbox controller support, complete widescreen road geometry, and the native
Kosmonaut campaign join SkyRoads and Xmas in one self-contained Windows build.

## New in 1.1.0

- **XInput controller support:** automatic detection and reconnection, left-stick
  dead zone, D-pad navigation, held-direction menu repeat, and controls for the
  game, menus, and editor. Windows' built-in XInput is loaded dynamically; no
  DirectX runtime download or extra DLL is required.
- **Complete widescreen geometry for SkyRoads and Xmas:** roads, tile edges,
  low/high blocks, arches, and tunnel openings use full meshes calibrated from
  the original perspective tables. The wider camera clips complete faces and
  depth-tests the ship instead of exposing the old 320-pixel cutoffs.
- **Options menu:** Hi-Def polygons and original 8:5, 16:9, or 21:9 presentation.
  Classic widescreen retains the 200-line look and original player sprites.
- **Kosmonaut:** native title, tutorial, road selector, 26-road campaign, ship
  graphics, animated roads, starfield, HUD, music, and effects. Its resources
  are embedded; no separate Kosmonaut executable is needed.
- More authentic lettering for the added main-menu entries, corrected ship
  visibility while falling, and improved near-camera rendering.
- Short Escape/controller menu presses are retained until the next gameplay
  update, preventing missed exit requests.

## Controller quick reference

| Context | Controls |
| --- | --- |
| Menus | D-pad/left stick: navigate; A/Start: confirm; B/Back: return |
| SkyRoads / Xmas | D-pad/left stick: steer and accelerate/brake; A: jump; B/Back/Start: leave road |
| Kosmonaut | D-pad/left stick: drive; A: jump/confirm; Start: pause/resume; B/Back: return |
| Editor | A: paint; X: play-test; Y: shape; Start: save; LB/RB: page; left-stick click: view |

Controllers work automatically with the existing keyboard controls. The first
connected XInput controller is used. Menu and editor navigation use the D-pad
or left stick. The editor still supports mouse painting and keyboard shortcuts.

## Installation

1. Download and extract the Windows x64 ZIP below, or download the standalone EXE.
2. Obtain SkyRoads or SkyRoads Xmas Special from
   [BlueMoon's official history and downloads page](http://www.bluemoon.ee/history/skyroads/).
3. Place an original `SKYROADS.EXE` or `SKYXMAS.EXE` beside the native executable.
4. Run `skyroads_native.exe` (or the versioned standalone executable).

Official downloads: [SkyRoads](http://www.bluemoon.ee/history/skyroads/skyroads.zip)
and [Xmas Special](http://www.bluemoon.ee/history/skyroads/skyxmas.zip).
No loose DOS data files, Visual C++ redistributable, MinGW libraries, or
Kosmonaut executable are required. Keep existing configuration files and
`custom_levels` beside the new executable to retain settings and creations.

## Included enhancements

Both 30-road SkyRoads campaigns, original area artwork, saved completion counts,
native OPL music and mixed sound effects, Alt+Enter fullscreen, and a visual
level editor with top/straight/isometric views. Original SkyRoads levels are
available in the editor's `ORIGINAL LEVELS` folder. Optional Ctrl+F9 through
Ctrl+F12 gameplay shortcuts remain available.

## Verification

The release is checked with native integration, controller mapping, rendering,
editor, embedded-resource, audio, and Windows DLL-dependency tests, plus the
29 recovered DOS-core suites. Controller tests cover dead zones, menu repeats,
held buttons, disconnection, keyboard coexistence, and editor shortcuts. No
physical controller was attached during this release's automated validation.

## Documentation and original credits

- [Editor manual](https://github.com/kandowontu/skyroads-3D-native/blob/v1.1.0/EDITOR_README.md)
- [Complete credits](https://github.com/kandowontu/skyroads-3D-native/blob/v1.1.0/CREDITS.md)
- [Third-party notices](https://github.com/kandowontu/skyroads-3D-native/blob/v1.1.0/THIRD_PARTY_NOTICES.md)

Original SkyRoads programming: Ahti Heinla, Priit Kasesalu, and Jaan Tallinn.
Graphics: Kaspar P. Loit. Music: Ott M. Aaloe. Intro music: Hasso Brück.
Special thanks: Priit Kull and Paul Varney. Original publisher: Creative Dimensions.

Original Kosmonaut: TIW Systems, Inc. (1990). Programming: Jaan Tallinn,
Ahti Heinla, and Priit Kasesalu. Graphics: Kaspar P. Loit. Music: Jüri Tallinn.

SkyRoads and the original assets remain the work of BlueMoon Software/Bluemoon
Interactive and their original creators. This is an unofficial native port
and preservation project, not endorsed by the original developer.
