#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "map.h"

static struct WolfMap wolfmap = {0};
static struct DoomMap doommap = {0};

void map_init(const char* maphead_name, const char* gamemaps_name, int level) {
    if (level < 0 || level >= MAX_LEVELS) {
        printf("!!! map_init: Level ID must range from 0 to 99\n");
        exit(EXIT_FAILURE);
    }

    // Open MAPHEAD
    FILE* maphead = fopen(maphead_name, "rb");
    if (maphead == NULL) {
        printf("!!! map_init: Failed to open MAPHEAD \"%s\"\n", maphead_name);
        perror("!!! map_init");
        exit(EXIT_FAILURE);
    }

    uint16_t magic;
    fread(&magic, sizeof(uint16_t), 1, maphead);
    magic = u16le(magic);

    int32_t level_offset;
    if (fseek(maphead, level * sizeof(int32_t), SEEK_CUR) != 0) {
        printf("!!! map_init: Failed to seek in MAPHEAD\n");
        exit(EXIT_FAILURE);
    }
    fread(&level_offset, sizeof(int32_t), 1, maphead);
    if ((level_offset = s32le(level_offset)) <= 0) {
        printf("!!! map_init: No data found for level %d\n", level);
        exit(EXIT_FAILURE);
    }

    fclose(maphead);

    // Open GAMEMAPS
    FILE* gamemaps = fopen(gamemaps_name, "rb");
    if (gamemaps == NULL) {
        printf("!!! map_init: Failed to open GAMEMAPS \"%s\"\n", gamemaps_name);
        perror("!!! map_init");
        exit(EXIT_FAILURE);
    }

    char header[8];
    fread(header, sizeof(uint8_t), sizeof(header), gamemaps);
    if (strncmp(header, "TED5v1.0", sizeof(header)) != 0) {
        printf("!!! map_init: Invalid GAMEMAPS header (%s =/= TED5v1.0)\n", header);
        exit(EXIT_FAILURE);
    }

    if (fseek(gamemaps, level_offset, SEEK_SET) != 0) {
        printf("!!! map_init: Failed to seek in GAMEMAPS\n");
        exit(EXIT_FAILURE);
    }

    wolfmap.id = level;
    fread(wolfmap.offsets, sizeof(int32_t), MAX_PLANES, gamemaps);
    fread(wolfmap.sizes, sizeof(uint16_t), MAX_PLANES, gamemaps);
    fread(&wolfmap.width, sizeof(uint16_t), 1, gamemaps);
    fread(&wolfmap.height, sizeof(uint16_t), 1, gamemaps);
    fread(wolfmap.name, sizeof(uint8_t), LEVEL_NAME_MAX, gamemaps);

    for (int i = 0; i < MAX_PLANES; i++) {
        wolfmap.offsets[i] = s32le(wolfmap.offsets[i]);
        wolfmap.sizes[i] = u16le(wolfmap.sizes[i]);
    }
    wolfmap.width = u16le(wolfmap.width);
    wolfmap.height = u16le(wolfmap.height);

    printf("map_init: Loading level %d (%s)\n", wolfmap.id, wolfmap.name);
    const size_t bufsize = wolfmap.width * wolfmap.height * sizeof(uint16_t);
    for (int i = 0; i < MAX_PLANES; i++) {
        if (wolfmap.sizes[i] <= 0) {
            printf("! map_init: No data in plane %u\n", i);
            wolfmap.planes[i] = NULL;
            continue;
        }

        uint8_t* rlew = malloc(bufsize);
        if (rlew == NULL) {
            printf("!!! map_init: Out of memory\n");
            exit(EXIT_FAILURE);
        }
        read_carmack(gamemaps, wolfmap.offsets[i], wolfmap.sizes[i], rlew);

        wolfmap.planes[i] = malloc(bufsize);
        if (wolfmap.planes[i] == NULL) {
            printf("!!! map_init: Out of memory\n");
            exit(EXIT_FAILURE);
        }
        read_rlew(rlew, (uint8_t*)wolfmap.planes[i], magic);
        free(rlew);
    }

    fclose(gamemaps);
}

void map_teardown() {
    for (int i = 0; i < MAX_PLANES; i++)
        if (wolfmap.planes[i] != NULL)
            free(wolfmap.planes[i]);

    if (doommap.things != NULL)
        free(doommap.things);
    if (doommap.lines != NULL)
        free(doommap.lines);
    if (doommap.sides != NULL)
        free(doommap.sides);
    if (doommap.vertices != NULL)
        free(doommap.vertices);
    if (doommap.sectors != NULL)
        free(doommap.sectors);
    if (doommap.linemap != NULL)
        free(doommap.linemap);
    if (doommap.sectormap != NULL)
        free(doommap.sectormap);
}

uint16_t read_u16le(const uint8_t* ptr) {
    return (uint16_t)((uint8_t)*ptr) | ((uint16_t)(uint8_t)(*(ptr + 1)) << 8);
}

void read_carmack(FILE* stream, size_t offset, size_t size, uint8_t* out) {
    if (fseek(stream, offset, SEEK_SET) != 0) {
        printf("!!! read_carmack: Seek failed\n");
        exit(EXIT_FAILURE);
    }

    uint8_t* john = malloc(size);
    if (john == NULL) {
        printf("!!! read_carmack: Out of memory\n");
        exit(EXIT_FAILURE);
    }
    fread(john, sizeof(uint8_t), size, stream);

    // https://github.com/cxong/cwolfmap/blob/a641ad1dd4f3f84ee826b561cfc6cebe9872e936/cwolfmap/expand.c#L51
    const uint8_t* start = out;
    const uint8_t* end = out + read_u16le(john);
    uint8_t* in = john + 2;

    const uint8_t* copy;
    uint8_t length;
    while (out < end) {
        length = *in++;
        if (length <= 0 && (*in == CARMACK_NEAR || *in == CARMACK_FAR)) {
            *out++ = in[1];
            *out++ = in[0];
            in += 2;
            continue;
        } else if (*in == CARMACK_NEAR) {
            copy = out - (in[1] * 2);
            in += 2;
        } else if (*in == CARMACK_FAR) {
            copy = start + (read_u16le(in + 1) * 2);
            in += 3;
        } else {
            *out++ = length;
            *out++ = *in++;
            continue;
        }

        if (out + (length * 2) > end)
            break;

        while (length-- > 0) {
            *out++ = *copy++;
            *out++ = *copy++;
        }
    }

    free(john);
}

void read_rlew(uint8_t* in, uint8_t* out, uint16_t magic) {
    // https://github.com/cxong/cwolfmap/blob/a641ad1dd4f3f84ee826b561cfc6cebe9872e936/cwolfmap/expand.c#L96
    uint16_t length = read_u16le(in);
    in += 2;

    const uint8_t* end = out + length;
    while (out < end)
        if (read_u16le(in) != magic) {
            *out++ = *in++;
            *out++ = *in++;
        } else {
            uint16_t count = read_u16le(in + 2);
            uint16_t input = read_u16le(in + 4);
            in += 6;
            for (uint16_t i = 0; i < count; i++) {
                out[0] = input & 0xFF;
                out[1] = (input >> 8) & 0xFF;
                out += 2;
            }
        }
}

void map_to_wad(const char* output_name) {
    if (wolfmap.planes[PLANE_OBJECTS] != NULL) {
        for (int16_t x = 0; x < wolfmap.width; x++) {
            for (int16_t y = 0; y < wolfmap.height; y++) {
                size_t pos = y * wolfmap.width + x;
                const struct ObjectInfo* obj = get_object_info(wolfmap.planes[PLANE_OBJECTS][pos]);
                if (obj == NULL || obj->type != OBJ_THING)
                    continue;

                ++doommap.num_things;
                doommap.things = (doommap.things == NULL)
                                     ? calloc(doommap.num_things, sizeof(struct DoomThing))
                                     : realloc(doommap.things, doommap.num_things * sizeof(struct DoomThing));
                if (doommap.things == NULL) {
                    printf("!!! map_to_wad: Out of memory\n");
                    exit(EXIT_FAILURE);
                }

                struct DoomThing* mobj = &doommap.things[doommap.num_things - 1];
                mobj->x = (x * 64) + 32;
                mobj->y = (y * -64) - 32;
                mobj->angle = obj->angle;
                mobj->ednum = obj->ednum;
                mobj->flags = (uint16_t)obj->flags;
                if (wolfmap.planes[PLANE_WALLS] != NULL && aid_is_ambush(wolfmap.planes[PLANE_WALLS][pos]))
                    mobj->flags |= TF_AMBUSH;
            }
        }

        if (doommap.num_things)
            printf("map_to_wad: Placed %zu thing(s)\n", doommap.num_things);
    }

    if (wolfmap.planes[PLANE_WALLS] != NULL) {
        if (doommap.linemap == NULL) {
            doommap.linemap = calloc(wolfmap.width * wolfmap.height, sizeof(struct LineCell));
            if (doommap.linemap == NULL) {
                printf("!!! map_to_wad: Out of memory\n");
                exit(EXIT_FAILURE);
            }
            memset(doommap.linemap, 0, wolfmap.width * wolfmap.height * sizeof(struct LineCell));
        }

        // First pass: Make sectors
        doommap.last_asector = 0xFFFE;
        for (int16_t x = 0; x < wolfmap.width; x++) {
            for (int16_t y = 0; y < wolfmap.height; y++) {
                size_t pos = y * wolfmap.width + x;
                uint16_t id = wolfmap.planes[PLANE_WALLS][pos];
                struct LineCell* cell = &doommap.linemap[pos];

                cell->tile = id;
                cell->wall = get_wall_info(id);
                cell->door = cell->wall == NULL ? get_door_info(id) : NULL;
                cell->area = ((cell->wall == NULL || cell->wall->type == WALL_MIDTEX) && cell->door == NULL)
                                 ? get_area_info(id)
                                 : NULL;
                cell->secret =
                    wolfmap.planes[PLANE_OBJECTS] != NULL && oid_is_pushwall(wolfmap.planes[PLANE_OBJECTS][pos]);

                uint16_t sector_id, sector_special = ST_NORMAL;
                if (cell->door != NULL || cell->secret) {
                    sector_special = cell->secret ? ST_SECRET : ST_NORMAL;
                    sector_id = doommap.last_asector--;
                } else if (cell->wall != NULL) {
                    sector_id = cell->wall->type == WALL_MIDTEX ? id : NO_SECTOR;
                } else if (cell->area != NULL) {
                    switch (cell->area->type) {
                        default:
                            sector_id = id;
                            break;

                        case AREA_SLIME5: {
                            sector_special = ST_SLIME5;
                            sector_id = id;
                            break;
                        }

                        case AREA_SLIME10: {
                            sector_special = ST_SLIME10;
                            sector_id = id;
                            break;
                        }

                        case AREA_SLIME20: {
                            sector_special = ST_SLIME20;
                            sector_id = id;
                            break;
                        }

                        case AREA_AMBUSH: {
                            struct LineCell* neighbor;
                            if ((y > 0 && (neighbor = &doommap.linemap[(y - 1) * wolfmap.width + x])->wall == NULL &&
                                 neighbor->door == NULL) ||
                                (x > 0 && (neighbor = &doommap.linemap[y * wolfmap.width + (x - 1)])->wall == NULL &&
                                 neighbor->door == NULL)) {
                                sector_id = cell->tile = neighbor->tile;
                                cell->area = neighbor->area;
                            } else if ((y < (wolfmap.height - 1) &&
                                        get_wall_info(
                                            id = (wolfmap.planes[PLANE_WALLS][(y + 1) * wolfmap.width + x])
                                        ) == NULL &&
                                        get_door_info(id) == NULL && !aid_is_ambush(id)) ||
                                       (x < (wolfmap.width - 1) &&
                                        get_wall_info(
                                            id = (wolfmap.planes[PLANE_WALLS][y * wolfmap.width + (x + 1)])
                                        ) == NULL &&
                                        get_door_info(id) == NULL && !aid_is_ambush(id))) {
                                sector_id = cell->tile = id;
                                cell->area = get_area_info(id);
                            } else {
                                sector_id = doommap.last_asector--;
                            }

                            break;
                        }
                    }
                } else {
                    sector_id = id;
                }

                cell->sector =
                    sector_id == NO_SECTOR
                        ? NO_SECTOR
                        : add_custom_sector(
                              sector_id, 0, (cell->door == NULL && !cell->secret) ? 64 : 0,
                              cell->door == NULL ? (cell->area == NULL ? get_config()->flats[FLAT_FLOOR]
                                                                       : cell->area->flats[FLAT_FLOOR])
                                                 : cell->door->flats[FLAT_FLOOR],
                              cell->door == NULL ? (cell->area == NULL ? get_config()->flats[FLAT_CEILING]
                                                                       : cell->area->flats[FLAT_CEILING])
                                                 : cell->door->flats[FLAT_CEILING],
                              cell->area == NULL ? get_config()->brightness : cell->area->brightness, sector_special,
                              cell->door != NULL ? cell->door->tag : (cell->area != NULL ? cell->area->tag : 0)
                          );
            }
        }

        // Second pass: Check space
        for (int16_t x = 0; x < wolfmap.width; x++) {
            for (int16_t y = 0; y < wolfmap.height; y++) {
                struct LineCell* cell = &doommap.linemap[y * wolfmap.width + x];

                if (cell->wall != NULL) {
                    cell->fright = place_free(cell, x + 1, y);
                    cell->ftop = place_free(cell, x, y - 1);
                    cell->fleft = place_free(cell, x - 1, y);
                    cell->fbottom = place_free(cell, x, y + 1);
                }

                if (cell->sector != NO_SECTOR) {
                    cell->sright = (!cell->fright && floor_free(cell, x + 1, y));
                    cell->stop = (!cell->ftop && floor_free(cell, x, y - 1));
                    cell->sleft = (!cell->fleft && floor_free(cell, x - 1, y));
                    cell->sbottom = (!cell->fbottom && floor_free(cell, x, y + 1));
                }
            }
        }

        // Third pass: Make linedefs
        for (int16_t x = 0; x < wolfmap.width; x++) {
            for (int16_t y = 0; y < wolfmap.height; y++) {
                size_t pos = y * wolfmap.width + x;
                struct LineCell* cell = &doommap.linemap[pos];

                if (cell->door != NULL) {
                    uint16_t ltrack_sector = add_custom_sector(
                        doommap.last_asector--, 0, 64, get_config()->flats[FLAT_FLOOR],
                        get_config()->flats[FLAT_CEILING], get_config()->brightness, ST_NORMAL, 0
                    );

                    uint16_t rtrack_sector = add_custom_sector(
                        doommap.last_asector--, 0, 64, get_config()->flats[FLAT_FLOOR],
                        get_config()->flats[FLAT_CEILING], get_config()->brightness, ST_NORMAL, 0
                    );

                    uint16_t action;
                    switch (cell->door->type) {
                        default:
                        case DOOR_NORMAL:
                            action = LT_DOOR;
                            break;
                        case DOOR_FAST:
                            action = LT_DOOR_FAST;
                            break;
                        case DOOR_RED:
                            action = LT_DOOR_RED;
                            break;
                        case DOOR_YELLOW:
                            action = LT_DOOR_YELLOW;
                            break;
                        case DOOR_BLUE:
                            action = LT_DOOR_BLUE;
                            break;
                        case DOOR_RED_CARD:
                            action = LT_DOOR_RED_CARD;
                            break;
                        case DOOR_YELLOW_CARD:
                            action = LT_DOOR_YELLOW_CARD;
                            break;
                        case DOOR_BLUE_CARD:
                            action = LT_DOOR_BLUE_CARD;
                            break;
                        case DOOR_RED_SKULL:
                            action = LT_DOOR_RED_SKULL;
                            break;
                        case DOOR_YELLOW_SKULL:
                            action = LT_DOOR_YELLOW_SKULL;
                            break;
                        case DOOR_BLUE_SKULL:
                            action = LT_DOOR_BLUE_SKULL;
                            break;
                    }

                    if (cell->door->axis == DAX_Y) {
                        // Entrance
                        add_line(
                            add_vertex((x + 0) * 64, (y + 0) * -64), add_vertex((x + 0) * 64, (y + 1) * -64), "-", "-",
                            "-", "-", "-", "-", doommap.linemap[y * wolfmap.width + (x - 1)].sector, ltrack_sector,
                            LF_TWO_SIDED, 0, 0, 0, 0
                        );
                        add_line(
                            add_vertex((x + 1) * 64, (y + 1) * -64), add_vertex((x + 1) * 64, (y + 0) * -64), "-", "-",
                            "-", "-", "-", "-", doommap.linemap[y * wolfmap.width + (x + 1)].sector, rtrack_sector,
                            LF_TWO_SIDED, 0, 0, 0, 0
                        );

                        // Side
                        add_line(
                            add_vertex((x + 0) * 64, (y + 0) * -64), add_vertex((x + 0) * 64 + 29, (y + 0) * -64), "-",
                            cell->door->track, "-", "-", "-", "-", ltrack_sector, NO_SECTOR, LF_BLOCKING | LF_UNPEG_LOW,
                            0, 0, 0, 0
                        );
                        add_line(
                            add_vertex((x + 0) * 64 + 35, (y + 0) * -64), add_vertex((x + 1) * 64, (y + 0) * -64), "-",
                            cell->door->track, "-", "-", "-", "-", rtrack_sector, NO_SECTOR, LF_BLOCKING | LF_UNPEG_LOW,
                            0, 0, 35, 0
                        );
                        add_line(
                            add_vertex((x + 1) * 64, (y + 1) * -64), add_vertex((x + 0) * 64 + 35, (y + 1) * -64), "-",
                            cell->door->track, "-", "-", "-", "-", rtrack_sector, NO_SECTOR, LF_BLOCKING | LF_UNPEG_LOW,
                            0, 0, 0, 0
                        );
                        add_line(
                            add_vertex((x + 0) * 64 + 29, (y + 1) * -64), add_vertex((x + 0) * 64, (y + 1) * -64), "-",
                            cell->door->track, "-", "-", "-", "-", ltrack_sector, NO_SECTOR, LF_BLOCKING | LF_UNPEG_LOW,
                            0, 0, 35, 0
                        );

                        // Door
                        add_line(
                            add_vertex((x + 0) * 64 + 29, (y + 0) * -64), add_vertex((x + 0) * 64 + 29, (y + 1) * -64),
                            cell->door->sides[SIDE_LEFT], "-", "-", "-", "-", "-", ltrack_sector, cell->sector,
                            LF_TWO_SIDED, action, 0, 0, 0
                        );
                        add_line(
                            add_vertex((x + 0) * 64 + 35, (y + 1) * -64), add_vertex((x + 0) * 64 + 35, (y + 0) * -64),
                            cell->door->sides[SIDE_RIGHT], "-", "-", "-", "-", "-", rtrack_sector, cell->sector,
                            LF_TWO_SIDED, action, 0, 0, 0
                        );
                        add_line(
                            add_vertex((x + 0) * 64 + 29, (y + 0) * -64), add_vertex((x + 0) * 64 + 35, (y + 0) * -64),
                            "-", cell->door->track, "-", "-", "-", "-", cell->sector, NO_SECTOR,
                            LF_BLOCKING | LF_UNPEG_LOW, LT_NORMAL, 0, 29, 0
                        );
                        add_line(
                            add_vertex((x + 0) * 64 + 35, (y + 1) * -64), add_vertex((x + 0) * 64 + 29, (y + 1) * -64),
                            "-", cell->door->track, "-", "-", "-", "-", cell->sector, NO_SECTOR,
                            LF_BLOCKING | LF_UNPEG_LOW, LT_NORMAL, 0, 29, 0
                        );
                    } else if (cell->door->axis == DAX_X) {
                        // Entrance
                        add_line(
                            add_vertex((x + 1) * 64, (y + 0) * -64), add_vertex((x + 0) * 64, (y + 0) * -64), "-", "-",
                            "-", "-", "-", "-", doommap.linemap[(y - 1) * wolfmap.width + x].sector, ltrack_sector,
                            LF_TWO_SIDED, 0, 0, 0, 0
                        );
                        add_line(
                            add_vertex((x + 0) * 64, (y + 1) * -64), add_vertex((x + 1) * 64, (y + 1) * -64), "-", "-",
                            "-", "-", "-", "-", doommap.linemap[(y + 1) * wolfmap.width + x].sector, rtrack_sector,
                            LF_TWO_SIDED, 0, 0, 0, 0
                        );

                        // Side
                        add_line(
                            add_vertex((x + 0) * 64, (y + 1) * -64), add_vertex((x + 0) * 64, (y + 0) * -64 - 35), "-",
                            cell->door->track, "-", "-", "-", "-", rtrack_sector, NO_SECTOR, LF_BLOCKING | LF_UNPEG_LOW,
                            0, 0, 0, 0
                        );
                        add_line(
                            add_vertex((x + 0) * 64, (y + 0) * -64 - 29), add_vertex((x + 0) * 64, (y + 0) * -64), "-",
                            cell->door->track, "-", "-", "-", "-", ltrack_sector, NO_SECTOR, LF_BLOCKING | LF_UNPEG_LOW,
                            0, 0, 35, 0
                        );
                        add_line(
                            add_vertex((x + 1) * 64, (y + 0) * -64 - 35), add_vertex((x + 1) * 64, (y + 1) * -64), "-",
                            cell->door->track, "-", "-", "-", "-", rtrack_sector, NO_SECTOR, LF_BLOCKING | LF_UNPEG_LOW,
                            0, 0, 35, 0
                        );
                        add_line(
                            add_vertex((x + 1) * 64, (y + 0) * -64), add_vertex((x + 1) * 64, (y + 0) * -64 - 29), "-",
                            cell->door->track, "-", "-", "-", "-", ltrack_sector, NO_SECTOR, LF_BLOCKING | LF_UNPEG_LOW,
                            0, 0, 0, 0
                        );

                        // Door
                        add_line(
                            add_vertex((x + 1) * 64, (y + 0) * -64 - 29), add_vertex((x + 0) * 64, (y + 0) * -64 - 29),
                            cell->door->sides[SIDE_RIGHT], "-", "-", "-", "-", "-", ltrack_sector, cell->sector,
                            LF_TWO_SIDED, action, 0, 0, 0
                        );
                        add_line(
                            add_vertex((x + 0) * 64, (y + 0) * -64 - 35), add_vertex((x + 1) * 64, (y + 0) * -64 - 35),
                            cell->door->sides[SIDE_LEFT], "-", "-", "-", "-", "-", rtrack_sector, cell->sector,
                            LF_TWO_SIDED, action, 0, 0, 0
                        );
                        add_line(
                            add_vertex((x + 0) * 64, (y + 0) * -64 - 35), add_vertex((x + 0) * 64, (y + 0) * -64 - 29),
                            "-", cell->door->track, "-", "-", "-", "-", cell->sector, NO_SECTOR,
                            LF_BLOCKING | LF_UNPEG_LOW, LT_NORMAL, 0, 29, 0
                        );
                        add_line(
                            add_vertex((x + 1) * 64, (y + 0) * -64 - 29), add_vertex((x + 1) * 64, (y + 0) * -64 - 35),
                            "-", cell->door->track, "-", "-", "-", "-", cell->sector, NO_SECTOR,
                            LF_BLOCKING | LF_UNPEG_LOW, LT_NORMAL, 0, 29, 0
                        );
                    }

                    continue;
                }

                struct LineCell* neighbor;
                if (cell->sright) {
                    neighbor = y <= 0 ? NULL : &doommap.linemap[(y - 1) * wolfmap.width + x];

                    if (neighbor != NULL && neighbor->sright && neighbor->sector == cell->sector &&
                        (x >= (wolfmap.width - 1) || doommap.linemap[y * wolfmap.width + (x + 1)].tile ==
                                                         doommap.linemap[(y - 1) * wolfmap.width + (x + 1)].tile)) {
                        cell->right = neighbor->right;
                        doommap.lines[cell->right].start = add_vertex((x + 1) * 64, (y + 1) * -64);
                    } else {
                        cell->right = add_line(
                            add_vertex((x + 1) * 64, (y + 1) * -64), add_vertex((x + 1) * 64, (y + 0) * -64), "-", "-",
                            "-", "-", "-", "-",
                            (x + 1) >= wolfmap.width ? NO_SECTOR : doommap.linemap[y * wolfmap.width + (x + 1)].sector,
                            cell->sector, LF_TWO_SIDED | LF_BLOCK_SOUND,
                            (cell->area != NULL && cell->area->type == AREA_TELEPORT) ? LT_TELEPORT : LT_NORMAL,
                            (cell->area != NULL && cell->area->type == AREA_TELEPORT) ? cell->area->tag : 0, 0, 0
                        );
                    }
                }

                if (cell->stop) {
                    neighbor = x <= 0 ? NULL : &doommap.linemap[y * wolfmap.width + (x - 1)];

                    if (neighbor != NULL && neighbor->stop && neighbor->sector == cell->sector &&
                        (y <= 0 || doommap.linemap[(y - 1) * wolfmap.width + x].tile ==
                                       doommap.linemap[(y - 1) * wolfmap.width + (x - 1)].tile)) {
                        cell->top = neighbor->top;
                        doommap.lines[cell->top].start = add_vertex((x + 1) * 64, (y + 0) * -64);
                    } else {
                        cell->top = add_line(
                            add_vertex((x + 1) * 64, (y + 0) * -64), add_vertex((x + 0) * 64, (y + 0) * -64), "-", "-",
                            "-", "-", "-", "-",
                            (y - 1) < 0 ? NO_SECTOR : doommap.linemap[(y - 1) * wolfmap.width + x].sector, cell->sector,
                            LF_TWO_SIDED | LF_BLOCK_SOUND,
                            (cell->area != NULL && cell->area->type == AREA_TELEPORT) ? LT_TELEPORT : LT_NORMAL,
                            (cell->area != NULL && cell->area->type == AREA_TELEPORT) ? cell->area->tag : 0, 0, 0
                        );
                    }
                }

                if (cell->sleft) {
                    neighbor = y <= 0 ? NULL : &doommap.linemap[(y - 1) * wolfmap.width + x];

                    if (neighbor != NULL && neighbor->sleft && neighbor->sector == cell->sector &&
                        (x <= 0 || doommap.linemap[y * wolfmap.width + (x - 1)].tile ==
                                       doommap.linemap[(y - 1) * wolfmap.width + (x - 1)].tile)) {
                        cell->left = neighbor->left;
                        doommap.lines[cell->left].end = add_vertex((x + 0) * 64, (y + 1) * -64);
                    } else {
                        cell->left = add_line(
                            add_vertex((x + 0) * 64, (y + 0) * -64), add_vertex((x + 0) * 64, (y + 1) * -64), "-", "-",
                            "-", "-", "-", "-",
                            (x - 1) < 0 ? NO_SECTOR : doommap.linemap[y * wolfmap.width + (x - 1)].sector, cell->sector,
                            LF_TWO_SIDED | LF_BLOCK_SOUND,
                            (cell->area != NULL && cell->area->type == AREA_TELEPORT) ? LT_TELEPORT : LT_NORMAL,
                            (cell->area != NULL && cell->area->type == AREA_TELEPORT) ? cell->area->tag : 0, 0, 0
                        );
                    }
                }

                if (cell->sbottom) {
                    neighbor = x <= 0 ? NULL : &doommap.linemap[y * wolfmap.width + (x - 1)];

                    if (neighbor != NULL && neighbor->sbottom && neighbor->sector == cell->sector &&
                        (y >= (wolfmap.height - 1) || doommap.linemap[(y + 1) * wolfmap.width + x].tile ==
                                                          doommap.linemap[(y + 1) * wolfmap.width + (x - 1)].tile)) {
                        cell->bottom = neighbor->bottom;
                        doommap.lines[cell->bottom].end = add_vertex((x + 1) * 64, (y + 1) * -64);
                    } else {
                        cell->bottom = add_line(
                            add_vertex((x + 0) * 64, (y + 1) * -64), add_vertex((x + 1) * 64, (y + 1) * -64), "-", "-",
                            "-", "-", "-", "-",
                            (y + 1) >= wolfmap.height ? NO_SECTOR : doommap.linemap[(y + 1) * wolfmap.width + x].sector,
                            cell->sector, LF_TWO_SIDED | LF_BLOCK_SOUND,
                            (cell->area != NULL && cell->area->type == AREA_TELEPORT) ? LT_TELEPORT : LT_NORMAL,
                            (cell->area != NULL && cell->area->type == AREA_TELEPORT) ? cell->area->tag : 0, 0, 0
                        );
                    }
                }

                if (cell->fright) {
                    neighbor = y <= 0 ? NULL : &doommap.linemap[(y - 1) * wolfmap.width + x];

                    if (neighbor != NULL && neighbor->wall == cell->wall && neighbor->fright &&
                        neighbor->sector == cell->sector &&
                        doommap.linemap[y * wolfmap.width + (x + 1)].tile ==
                            doommap.linemap[(y - 1) * wolfmap.width + (x + 1)].tile) {
                        cell->right = neighbor->right;
                        doommap.lines[cell->right].start = add_vertex((x + 1) * 64, (y + 1) * -64);
                    } else {
                        neighbor = &doommap.linemap[y * wolfmap.width + (x + 1)];
                        cell->right = add_line(
                            add_vertex((x + 1) * 64, (y + 1) * -64), add_vertex((x + 1) * 64, (y + 0) * -64),
                            (cell->secret && cell->wall->type != WALL_MIDTEX) ? cell->wall->textures[SIDE_Y] : "-",
                            (!cell->secret || cell->wall->type == WALL_MIDTEX) ? cell->wall->textures[SIDE_Y] : "-",
                            "-",
                            (cell->secret && cell->wall->type != WALL_MIDTEX) ? cell->wall->textures[SIDE_BACK_Y] : "-",
                            (!cell->secret || cell->wall->type == WALL_MIDTEX) ? cell->wall->textures[SIDE_BACK_Y]
                                                                               : "-",
                            "-", neighbor->sector, cell->sector,
                            cell->wall->type == WALL_MIDTEX
                                ? (cell->secret ? (LF_TWO_SIDED | LF_UNPEG_LOW)
                                                : (LF_TWO_SIDED | LF_UNPEG_LOW | LF_BLOCKING | LF_BLOCK_SOUND))
                                : (cell->sector == NO_SECTOR ? (LF_BLOCKING | LF_UNPEG_LOW)
                                                             : (LF_TWO_SIDED | LF_SECRET)),
                            cell->sector == NO_SECTOR
                                ? (cell->wall->actions[SIDE_Y] == WACT_EXIT
                                       ? ((neighbor->area != NULL && neighbor->area->type == AREA_SECRET_EXIT)
                                              ? LT_SECRET_EXIT
                                              : LT_EXIT)
                                       : LT_NORMAL)
                                : ((cell->secret && cell->wall->type != WALL_MIDTEX) ? LT_SECRET : LT_NORMAL),
                            cell->wall->tag, 0, 0
                        );
                    }
                }

                if (cell->ftop) {
                    neighbor = x <= 0 ? NULL : &doommap.linemap[y * wolfmap.width + (x - 1)];

                    if (neighbor != NULL && neighbor->wall == cell->wall && neighbor->ftop &&
                        neighbor->sector == cell->sector &&
                        doommap.linemap[(y - 1) * wolfmap.width + x].tile ==
                            doommap.linemap[(y - 1) * wolfmap.width + (x - 1)].tile) {
                        cell->top = neighbor->top;
                        doommap.lines[cell->top].start = add_vertex((x + 1) * 64, (y + 0) * -64);
                    } else {
                        neighbor = &doommap.linemap[(y - 1) * wolfmap.width + x];
                        cell->top = add_line(
                            add_vertex((x + 1) * 64, (y + 0) * -64), add_vertex((x + 0) * 64, (y + 0) * -64),
                            (cell->secret && cell->wall->type != WALL_MIDTEX) ? cell->wall->textures[SIDE_X] : "-",
                            (!cell->secret || cell->wall->type == WALL_MIDTEX) ? cell->wall->textures[SIDE_X] : "-",
                            "-",
                            (cell->secret && cell->wall->type != WALL_MIDTEX) ? cell->wall->textures[SIDE_BACK_X] : "-",
                            (!cell->secret || cell->wall->type == WALL_MIDTEX) ? cell->wall->textures[SIDE_BACK_X]
                                                                               : "-",
                            "-", neighbor->sector, cell->sector,
                            cell->wall->type == WALL_MIDTEX
                                ? (cell->secret ? (LF_TWO_SIDED | LF_UNPEG_LOW)
                                                : (LF_TWO_SIDED | LF_UNPEG_LOW | LF_BLOCKING | LF_BLOCK_SOUND))
                                : (cell->sector == NO_SECTOR ? (LF_BLOCKING | LF_UNPEG_LOW)
                                                             : (LF_TWO_SIDED | LF_SECRET)),
                            cell->sector == NO_SECTOR
                                ? (cell->wall->actions[SIDE_X] == WACT_EXIT
                                       ? ((neighbor->area != NULL && neighbor->area->type == AREA_SECRET_EXIT)
                                              ? LT_SECRET_EXIT
                                              : LT_EXIT)
                                       : LT_NORMAL)
                                : ((cell->secret && cell->wall->type != WALL_MIDTEX) ? LT_SECRET : LT_NORMAL),
                            cell->wall->tag, 0, 0
                        );
                    }
                }

                if (cell->fleft) {
                    neighbor = y <= 0 ? NULL : &doommap.linemap[(y - 1) * wolfmap.width + x];

                    if (neighbor != NULL && neighbor->wall == cell->wall && neighbor->fleft &&
                        neighbor->sector == cell->sector &&
                        doommap.linemap[y * wolfmap.width + (x - 1)].tile ==
                            doommap.linemap[(y - 1) * wolfmap.width + (x - 1)].tile) {
                        cell->left = neighbor->left;
                        doommap.lines[cell->left].end = add_vertex((x + 0) * 64, (y + 1) * -64);
                    } else {
                        neighbor = &doommap.linemap[y * wolfmap.width + (x - 1)];
                        cell->left = add_line(
                            add_vertex((x + 0) * 64, (y + 0) * -64), add_vertex((x + 0) * 64, (y + 1) * -64),
                            (cell->secret && cell->wall->type != WALL_MIDTEX) ? cell->wall->textures[SIDE_Y] : "-",
                            (!cell->secret || cell->wall->type == WALL_MIDTEX) ? cell->wall->textures[SIDE_Y] : "-",
                            "-",
                            (cell->secret && cell->wall->type != WALL_MIDTEX) ? cell->wall->textures[SIDE_BACK_Y] : "-",
                            (!cell->secret || cell->wall->type == WALL_MIDTEX) ? cell->wall->textures[SIDE_BACK_Y]
                                                                               : "-",
                            "-", neighbor->sector, cell->sector,
                            cell->wall->type == WALL_MIDTEX
                                ? (cell->secret ? (LF_TWO_SIDED | LF_UNPEG_LOW)
                                                : (LF_TWO_SIDED | LF_UNPEG_LOW | LF_BLOCKING | LF_BLOCK_SOUND))
                                : (cell->sector == NO_SECTOR ? (LF_BLOCKING | LF_UNPEG_LOW)
                                                             : (LF_TWO_SIDED | LF_SECRET)),
                            cell->sector == NO_SECTOR
                                ? (cell->wall->actions[SIDE_Y] == WACT_EXIT
                                       ? ((neighbor->area != NULL && neighbor->area->type == AREA_SECRET_EXIT)
                                              ? LT_SECRET_EXIT
                                              : LT_EXIT)
                                       : LT_NORMAL)
                                : ((cell->secret && cell->wall->type != WALL_MIDTEX) ? LT_SECRET : LT_NORMAL),
                            cell->wall->tag, 0, 0
                        );
                    }
                }

                if (cell->fbottom) {
                    neighbor = x <= 0 ? NULL : &doommap.linemap[y * wolfmap.width + (x - 1)];

                    if (neighbor != NULL && neighbor->wall == cell->wall && neighbor->fbottom &&
                        neighbor->sector == cell->sector &&
                        doommap.linemap[(y + 1) * wolfmap.width + x].tile ==
                            doommap.linemap[(y + 1) * wolfmap.width + (x - 1)].tile) {
                        cell->bottom = neighbor->bottom;
                        doommap.lines[cell->bottom].end = add_vertex((x + 1) * 64, (y + 1) * -64);
                    } else {
                        neighbor = &doommap.linemap[(y + 1) * wolfmap.width + x];
                        cell->bottom = add_line(
                            add_vertex((x + 0) * 64, (y + 1) * -64), add_vertex((x + 1) * 64, (y + 1) * -64),
                            (cell->secret && cell->wall->type != WALL_MIDTEX) ? cell->wall->textures[SIDE_X] : "-",
                            (!cell->secret || cell->wall->type == WALL_MIDTEX) ? cell->wall->textures[SIDE_X] : "-",
                            "-",
                            (cell->secret && cell->wall->type != WALL_MIDTEX) ? cell->wall->textures[SIDE_BACK_X] : "-",
                            (!cell->secret || cell->wall->type == WALL_MIDTEX) ? cell->wall->textures[SIDE_BACK_X]
                                                                               : "-",
                            "-", neighbor->sector, cell->sector,
                            cell->wall->type == WALL_MIDTEX
                                ? (cell->secret ? (LF_TWO_SIDED | LF_UNPEG_LOW)
                                                : (LF_TWO_SIDED | LF_UNPEG_LOW | LF_BLOCKING | LF_BLOCK_SOUND))
                                : (cell->sector == NO_SECTOR ? (LF_BLOCKING | LF_UNPEG_LOW)
                                                             : (LF_TWO_SIDED | LF_SECRET)),
                            cell->sector == NO_SECTOR
                                ? (cell->wall->actions[SIDE_X] == WACT_EXIT
                                       ? ((neighbor->area != NULL && neighbor->area->type == AREA_SECRET_EXIT)
                                              ? LT_SECRET_EXIT
                                              : LT_EXIT)
                                       : LT_NORMAL)
                                : ((cell->secret && cell->wall->type != WALL_MIDTEX) ? LT_SECRET : LT_NORMAL),
                            cell->wall->tag, 0, 0
                        );
                    }
                }
            }
        }

        printf("map_to_wad: Placed %zu line(s), %zu sector(s)\n", doommap.num_lines, doommap.num_sectors);
    }

    FILE* output = fopen(output_name, "wb");
    if (output == NULL) {
        printf("!!! map_to_wad: Failed to open output \"%s\"\n", output_name);
        perror("!!! map_to_wad");
        exit(EXIT_FAILURE);
    }

    write_string(output, "PWAD", 4); // identification @ 0 -> 4
    write_u32le(output, 11);         // numlumps @ 4 -> 8
    write_u32le(output, 12);         // infotableofs @ 8 -> 12

    // MAPxx
    char map_name[LUMP_NAME_MAX];
    snprintf(map_name, LUMP_NAME_MAX, "MAP%02u", wolfmap.id + 1);
    write_u32le(output, 0);                        // filepos @ 12 -> 16
    write_u32le(output, 0);                        // size @ 16 -> 20
    write_string(output, map_name, LUMP_NAME_MAX); // name @ 20 -> 28

    // THINGS
    size_t things_size = doommap.num_things * sizeof(struct DoomThing);
    write_u32le(output, 188);                      // filepos @ 28 -> 32
    write_u32le(output, things_size);              // size @ 32 -> 36
    write_string(output, "THINGS", LUMP_NAME_MAX); // name @ 36 -> 44

    // LINEDEFS
    size_t linedefs_size = doommap.num_lines * sizeof(struct DoomLine);
    write_u32le(output, 188 + things_size);          // filepos @ 44 - 48
    write_u32le(output, linedefs_size);              // size @ 48 -> 52
    write_string(output, "LINEDEFS", LUMP_NAME_MAX); // name @ 52 -> 60

    // SIDEDEFS
    size_t sidedefs_size = doommap.num_sides * sizeof(struct DoomSide);
    write_u32le(output, 188 + things_size + linedefs_size); // filepos @ 60 -> 64
    write_u32le(output, sidedefs_size);                     // size @ 64 -> 68
    write_string(output, "SIDEDEFS", LUMP_NAME_MAX);        // name @ 68 -> 76

    // VERTEXES
    size_t vertexes_size = doommap.num_vertices * sizeof(struct DoomVertex);
    write_u32le(output, 188 + things_size + linedefs_size + sidedefs_size); // filepos @ 76 -> 80
    write_u32le(output, vertexes_size);                                     // size @ 80 -> 84
    write_string(output, "VERTEXES", LUMP_NAME_MAX);                        // name @ 84 -> 92

    // SEGS
    write_u32le(output, 0);                      // filepos @ 92 -> 96
    write_u32le(output, 0);                      // size @ 96 -> 100
    write_string(output, "SEGS", LUMP_NAME_MAX); // name @ 100 -> 108

    // SSECTORS
    write_u32le(output, 0);                          // filepos @ 108 -> 112
    write_u32le(output, 0);                          // size @ 112 -> 116
    write_string(output, "SSECTORS", LUMP_NAME_MAX); // name @ 116 -> 124

    // NODES
    write_u32le(output, 0);                       // filepos @ 124 -> 128
    write_u32le(output, 0);                       // size @ 128 -> 132
    write_string(output, "NODES", LUMP_NAME_MAX); // name @ 132 -> 140

    // SECTORS
    size_t sectors_size = doommap.num_sectors * sizeof(struct DoomSector);
    write_u32le(output, 188 + things_size + linedefs_size + sidedefs_size + vertexes_size); // filepos @ 140 -> 144
    write_u32le(output, sectors_size);                                                      // size @ 144 -> 148
    write_string(output, "SECTORS", LUMP_NAME_MAX);                                         // name @ 148 -> 156

    // REJECT
    write_u32le(output, 0);                        // filepos @ 156 -> 160
    write_u32le(output, 0);                        // size @ 160 -> 164
    write_string(output, "REJECT", LUMP_NAME_MAX); // name @ 164 -> 172

    // BLOCKMAP
    write_u32le(output, 0);                          // filepos @ 172 -> 176
    write_u32le(output, 0);                          // size @ 176 -> 180
    write_string(output, "BLOCKMAP", LUMP_NAME_MAX); // name @ 180 -> 188

    if (doommap.things != NULL)
        fwrite(doommap.things, sizeof(struct DoomThing), doommap.num_things, output);
    if (doommap.lines != NULL)
        fwrite(doommap.lines, sizeof(struct DoomLine), doommap.num_lines, output);
    if (doommap.sides != NULL)
        fwrite(doommap.sides, sizeof(struct DoomSide), doommap.num_sides, output);
    if (doommap.vertices != NULL)
        fwrite(doommap.vertices, sizeof(struct DoomVertex), doommap.num_vertices, output);
    if (doommap.sectors != NULL)
        fwrite(doommap.sectors, sizeof(struct DoomSector), doommap.num_sectors, output);

    fclose(output);
    printf("map_to_wad: Saved as \"%s\" in \"%s\"\n", map_name, output_name);
}

bool place_free(struct LineCell* from, int x, int y) {
    if (x < 0 || x >= wolfmap.width || y < 0 || y >= wolfmap.height)
        return false;

    size_t pos = y * wolfmap.width + x;
    struct LineCell* cell = &doommap.linemap[pos];

    if (cell->door != NULL ||
        (cell->wall != NULL &&
         (cell->wall->type != WALL_MIDTEX || (from->wall != NULL && from->wall->type == WALL_MIDTEX)) &&
         (wolfmap.planes[PLANE_OBJECTS] == NULL || !oid_is_pushwall(wolfmap.planes[PLANE_OBJECTS][pos]))))
        return false;

    return true;
}

bool floor_free(struct LineCell* from, int x, int y) {
    if (x < 0 || x >= wolfmap.width || y < 0 || y >= wolfmap.height)
        return true;

    struct LineCell* cell = &doommap.linemap[y * wolfmap.width + x];
    if (cell->sector != from->sector && cell->sector != NO_SECTOR)
        if (from->wall != NULL && from->wall->type == WALL_MIDTEX) {
            if (cell->wall != NULL && cell->wall->type == WALL_MIDTEX)
                return true;
        } else if (cell->wall == NULL && cell->door == NULL) {
            return true;
        }

    return false;
}

void write_string(FILE* stream, const char* string, size_t size) {
    fwrite(string, size, 1, stream);
}

void write_u16le(FILE* stream, uint16_t uint16) {
    uint16 = u16le(uint16);
    fwrite(&uint16, sizeof(uint16), 1, stream);
}

void write_s16le(FILE* stream, int16_t int16) {
    int16 = s16le(int16);
    fwrite(&int16, sizeof(int16), 1, stream);
}

void write_u32le(FILE* stream, uint32_t uint32) {
    uint32 = u32le(uint32);
    fwrite(&uint32, sizeof(uint32), 1, stream);
}

uint16_t add_vertex(int16_t x, int16_t y) {
    if (doommap.vertices == NULL) {
        doommap.num_vertices = 1;
        doommap.vertices = malloc(sizeof(struct DoomVertex));
        if (doommap.vertices == NULL) {
            printf("!!! add_vertex: Out of memory\n");
            exit(EXIT_FAILURE);
        }

        doommap.vertices[0].x = x;
        doommap.vertices[0].y = y;
        return 0;
    }

    size_t i;
    for (i = 0; i < doommap.num_vertices; i++)
        if (doommap.vertices[i].x == x && doommap.vertices[i].y == y)
            return i;

    doommap.vertices = realloc(doommap.vertices, ++doommap.num_vertices * sizeof(struct DoomVertex));
    if (doommap.vertices == NULL) {
        printf("!!! add_vertex: Out of memory\n");
        exit(EXIT_FAILURE);
    }

    doommap.vertices[i].x = x;
    doommap.vertices[i].y = y;
    return i;
}

uint16_t add_side(
    const char* upper, const char* middle, const char* lower, uint16_t sector, int16_t x_offset, int16_t y_offset
) {
    size_t i = doommap.num_sides++;
    doommap.sides = (doommap.sides == NULL) ? calloc(doommap.num_sides, sizeof(struct DoomSide))
                                            : realloc(doommap.sides, doommap.num_sides * sizeof(struct DoomSide));
    if (doommap.sides == NULL) {
        printf("!!! add_side: Out of memory\n");
        exit(EXIT_FAILURE);
    }

    strncpy(doommap.sides[i].textures[SIDE_UPPER], upper, LUMP_NAME_MAX);
    strncpy(doommap.sides[i].textures[SIDE_MIDDLE], middle, LUMP_NAME_MAX);
    strncpy(doommap.sides[i].textures[SIDE_LOWER], lower, LUMP_NAME_MAX);
    doommap.sides[i].x_offset = x_offset;
    doommap.sides[i].y_offset = y_offset;
    doommap.sides[i].sector = sector;
    return i;
}

uint16_t add_line(
    uint16_t start, uint16_t end, const char* upper, const char* middle, const char* lower, const char* back_upper,
    const char* back_middle, const char* back_lower, uint16_t sector, uint16_t back_sector, uint16_t flags,
    uint16_t special, uint16_t tag, int16_t x_offset, int16_t y_offset
) {
    if (doommap.lines == NULL) {
        doommap.num_lines = 1;
        doommap.lines = malloc(sizeof(struct DoomLine));
        if (doommap.lines == NULL) {
            printf("!!! add_line: Out of memory\n");
            exit(EXIT_FAILURE);
        }

        doommap.lines[0].start = start;
        doommap.lines[0].end = end;
        doommap.lines[0].flags = flags;
        doommap.lines[0].special = special;
        doommap.lines[0].tag = tag;
        doommap.lines[0].front = add_side(upper, middle, lower, sector, x_offset, y_offset);
        doommap.lines[0].back = add_side(back_upper, back_middle, back_lower, back_sector, x_offset, y_offset);
        return 0;
    }

    size_t i;
    for (i = 0; i < doommap.num_lines; i++)
        if ((doommap.lines[i].start == start && doommap.lines[i].end == end) ||
            (doommap.lines[i].start == end && doommap.lines[i].end == start && doommap.lines[i].flags == LF_TWO_SIDED))
            return i;

    doommap.lines = realloc(doommap.lines, ++doommap.num_lines * sizeof(struct DoomLine));
    if (doommap.lines == NULL) {
        printf("!!! add_line: Out of memory\n");
        exit(EXIT_FAILURE);
    }

    doommap.lines[i].start = start;
    doommap.lines[i].end = end;
    doommap.lines[i].flags = flags;
    doommap.lines[i].special = special;
    doommap.lines[i].tag = tag;
    doommap.lines[i].front = add_side(upper, middle, lower, sector, x_offset, y_offset);
    doommap.lines[i].back = add_side(back_upper, back_middle, back_lower, back_sector, x_offset, y_offset);
    return i;
}

uint16_t add_sector(struct AreaInfo* area) {
    return add_custom_sector(
        area->id, 0, 64, area->flats[FLAT_FLOOR], area->flats[FLAT_CEILING], area->brightness, ST_NORMAL, 0
    );
}

uint16_t add_custom_sector(
    uint16_t id, int16_t floorz, int16_t ceilingz, const char* floor, const char* ceiling, uint16_t brightness,
    uint16_t special, uint16_t tag
) {
    if (doommap.sectors == NULL) {
        doommap.num_sectors = 1;
        doommap.sectors = malloc(sizeof(struct DoomSector));
        doommap.sectormap = malloc(sizeof(uint16_t));
        if (doommap.sectors == NULL || doommap.sectormap == NULL) {
            printf("!!! add_custom_sector: Out of memory\n");
            exit(EXIT_FAILURE);
        }

        doommap.sectormap[0] = id;
        doommap.sectors[0].floor = floorz;
        doommap.sectors[0].ceiling = ceilingz;
        strncpy(doommap.sectors[0].flats[FLAT_FLOOR], floor, LUMP_NAME_MAX);
        strncpy(doommap.sectors[0].flats[FLAT_CEILING], ceiling, LUMP_NAME_MAX);
        doommap.sectors[0].brightness = brightness;
        doommap.sectors[0].special = special;
        doommap.sectors[0].tag = tag;
        return 0;
    }

    size_t i;
    for (i = 0; i < doommap.num_sectors; i++)
        if (doommap.sectormap[i] == id)
            return i;

    ++doommap.num_sectors;
    doommap.sectors = realloc(doommap.sectors, doommap.num_sectors * sizeof(struct DoomSector));
    doommap.sectormap = realloc(doommap.sectormap, doommap.num_sectors * sizeof(uint16_t));
    if (doommap.sectors == NULL || doommap.sectormap == NULL) {
        printf("!!! add_custom_sector: Out of memory\n");
        exit(EXIT_FAILURE);
    }

    doommap.sectormap[i] = id;
    doommap.sectors[i].floor = floorz;
    doommap.sectors[i].ceiling = ceilingz;
    strncpy(doommap.sectors[i].flats[FLAT_FLOOR], floor, LUMP_NAME_MAX);
    strncpy(doommap.sectors[i].flats[FLAT_CEILING], ceiling, LUMP_NAME_MAX);
    doommap.sectors[i].brightness = brightness;
    doommap.sectors[i].special = special;
    doommap.sectors[i].tag = tag;
    return i;
}
