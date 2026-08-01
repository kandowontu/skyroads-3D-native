# SkyRoads Native level editor

The built-in editor works directly with the recovered seven-column SkyRoads
road model. Its cells are the same 16-bit descriptors consumed by the original
renderer, collision code, and fixed-point physics.

## Opening the editor

Choose `EDITOR` on the main menu. The creation browser contains:

- `CREATE NEW`, which creates a 96-row road with a safe starting platform and
  terminal section;
- `ORIGINAL LEVELS`, containing editable imports named `1-1` through `10-3`;
- every `.srlevel` creation saved directly under `custom_levels`.

Use the arrow keys to select an entry, `Page Up` and `Page Down` to move a page,
`Enter` to open a folder or edit a road, and `P` to play-test the selected road.
`Escape` returns to the previous screen.

## Editor controls

| Control | Action |
| --- | --- |
| Arrow keys | Move the selected cell through the seven columns and all rows |
| `Page Up` / `Page Down` | Move 15 rows |
| `Space` | Paint the selected cell with the current material and shape |
| Left click | Select and paint a cell in top view, or select a material swatch |
| `0`–`9` | Select a material tool |
| `T` | Cycle flat, arch, wall, ramp, high, and high-ramp shapes |
| `V` | Cycle top, left isometric, straight, and right isometric views |
| `W` | Cycle the original world theme |
| `G` | Cycle gravity |
| `F` | Cycle the fuel budget |
| `O` | Cycle the oxygen budget |
| `Insert` | Duplicate the current row after it |
| `Delete` | Delete the current row |
| `S` or `Enter` | Save |
| `P` | Save and play-test through the recovered game loop |
| `Escape` | Save pending changes and return to the browser |

The top view is the direct painting surface. The three spatial views expose the
height and slope encoded in each selected cell; use `Space` to paint while one
of those views is active.

## Material tools

| Key | Tool | Recovered cell material |
| --- | --- | --- |
| `0` | Erase | Empty space |
| `1` | Road | Standard road |
| `2` | Slow | Speed-reducing road |
| `3` | Blue Road | Blue road surface |
| `4` | Gold Road | Gold road surface |
| `5` | Slide | Low-friction road |
| `6` | Refill | Resource refill |
| `7` | Boost | Acceleration boost |
| `8` | Terminal | End-tube/terminal material |
| `9` | White Road | White road surface |

Roads must remain between 16 and 2,048 rows. A playable road should begin with
a broad safe platform and end with a reachable terminal section. Use play-test
frequently to check jumps, gravity, fuel, oxygen, and finish-tube alignment.

## Files and original levels

Custom roads use the `.srlevel` extension and are saved under the
`custom_levels` folder beside the running game. The two demo roads and new
creations live at the root of that folder.

At startup, the game creates `custom_levels/ORIGINAL LEVELS` and imports any
missing base-game roads from the decoded `ROADS.LZS` records embedded in the
native executable. Existing valid files are preserved, so edits to an original
road remain saved. To restore one road to its shipped layout, close the game,
delete only that road's `.srlevel` file from `ORIGINAL LEVELS`, and relaunch.
The missing file will be imported again.

Back up the `custom_levels` folder to move creations to another installation.
Do not place unrelated files in it, and do not edit `.srlevel` files with a text
editor because the format is binary.
