# wolf2wad

A configurable command line tool for converting Wolfenstein 3D maps into a Doom
WAD.

## Usage

```
wolf2wad [-c <file>] [-i <maphead> <gamemaps>] [-l <level>] [-o <file>]
```

## Details

Rushed in 3 days, so some of it is ugly, repetitive and/or redundant. Only
tested on Windows and on .WL6 maps.

See `config.json` for an example of how you can configure what each tile number
will correspond to (walls, doors, objects, areas/floorcodes, etc.). The
definitions are based on ChaosEdit's Wolfenstein 3D mapobjects list.

Also included is `WOLFDOOM.wad`, a MBF21+DSDHacked resource WAD that includes
definitions for the DoomEdNums seen in `config.json` (Use the `DECOHACK` lump
as a glossary).

Thanks to [cwolfmap](https://github.com/cxong/cwolfmap) for RLEW+Carmackize
decompression.

## Configuration

### Main

| Property     | Description                                                                                                                                                                              |
| ------------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `name`       | Display name of the config.                                                                                                                                                              |
| `format`     | The map format to use for the output. Default is `mbf21`.<br><br>**Values:**<br>- `doom` Vanilla. Unique key doors aren't possible with this format.<br>- `boom`<br>- `mbf`<br>- `mbf21` |
| `floor`      | Default flat to use for the floor. Default is `FLAT5_4`.                                                                                                                                 |
| `ceiling`    | Default flat to use for the ceiling. Default is `FWOLF31` (from `WOLFDOOM.wad`).                                                                                                         |
| `brightness` | Default brightness. Default is `160`.                                                                                                                                                    |

### `walls`

| Property | Description                                                                                                                                                                                                                                                                                             |
| -------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `name`   | Display name of the wall.                                                                                                                                                                                                                                                                               |
| `type`   | Type of wall. Default is `normal`.<br><br>**Values:**<br>- `normal` Uses single-sided linedefs.<br>- `midtex` Uses double-sided ((sound) blocking) linedefs. This will also generate a sector that all other instances of this wall will share (can be customized in `areas`).                          |
| `xtex`   | Texture of the wall along the X-axis. Default is `-`.                                                                                                                                                                                                                                                   |
| `ytex`   | Texture of the wall along the Y-axis. Default is `xtex`'s value.                                                                                                                                                                                                                                        |
| `xback`  | Back texture of the wall along the X-axis. Default is `xtex`'s value.                                                                                                                                                                                                                                   |
| `yback`  | Back texture of the wall along the Y-axis. Default is `ytex`'s value.                                                                                                                                                                                                                                   |
| `xact`   | Special action for each line along the X-axis. Default is `none`.<br><br>**Values:**<br>- `none`<br>- `switch` Open all doors matching the wall's tag when pressed once.<br>- `exit` Exit the level. If the area in front of this linedef is the type `secret_exit`, then it will act as a secret exit. |
| `yact`   | Special action for each line along the Y-axis. Default is `none`.<br><br>See `xact` for values.                                                                                                                                                                                                         |
| `tag`    | Tag of each linedef of the wall. Default is `0`.                                                                                                                                                                                                                                                        |

## Oddities

The output may contain the following oddities:

| Oddity                                                                                                                                                                         | Example                                       |
| ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | --------------------------------------------- |
| Output won't contain nodes. Get a node builder for that.                                                                                                                       | Running the output WAD directly in DSDA-Doom. |
| Pushwalls won't work properly when there is another pushwall next to it.                                                                                                       | Wolfenstein 3D, E1M10                         |
| If a map's border isn't covered with a tile, it may cause a HOM.                                                                                                               | Wolfenstein 3D, E1M3                          |
| Wolfenstein 3D uses floor codes for sound propagation, so all sectors based on the same floor code are joined and separated between other floors with sound blocking linedefs. | Duhhhhh i dunno duuuuuuhhhhh                  |

wolf2wad always assumes the level design to be sane, so things like doors
placed next to each other don't have a failsafe.
