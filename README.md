# wolf2wad

A configurable command line tool for converting Wolfenstein 3D maps into a Doom
WAD.

## Usage

```
wolf2wad [-c <file>] [-i <maphead> <gamemaps>] [-l <level>] [-o <file>]
```

## Details

Rushed in 3 days, so some of it is ugly, repetitive and/or redundant. Only
tested on Windows.

For config properties, see either [`CONFIG.md`](https://github.com/LocalInsomniac/wolf2wad/blob/main/CONFIG.md)
for a glossary or [`config.json`](https://github.com/LocalInsomniac/wolf2wad/blob/main/config.json)
for a more complete example. Basically you can configure what each tile number
will correspond to (walls, doors, objects, areas/floorcodes, etc.).
The definitions are based on ChaosEdit's Wolfenstein 3D mapobjects list.

Also included is [`WOLFDOOM.wad`](https://github.com/LocalInsomniac/wolf2wad/raw/refs/heads/main/WOLFDOOM.wad),
a MBF21+DSDHacked resource WAD that includes definitions for the DoomEdNums
seen in `config.json` (check the `DECOHACK` lump!)

Thanks to [cwolfmap](https://github.com/cxong/cwolfmap) for RLEW+Carmackize
decompression.

## Oddities

The output may contain the following oddities:

| Oddity                                                                                                                                                                                             | Example                                       |
| -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------- |
| Output won't contain nodes. Get a node builder for that.                                                                                                                                           | Running the output WAD directly in DSDA-Doom. |
| Pushwalls won't work properly when there is another pushwall next to it.                                                                                                                           | Wolfenstein 3D, E1M10                         |
| If a map's border isn't covered with a wall, it may cause a HOM.                                                                                                                                   | Wolfenstein 3D, E1M3                          |
| Wolfenstein 3D uses floor codes for sound propagation, so all sectors based on the same floor code (or tile ID) are joined and may be separated between other floors with sound blocking linedefs. | Duhhhhh i dunno duuuuuuhhhhh                  |
