#pragma once

#ifdef __BIG_ENDIAN__
#define u16le(x) ((x >> 8) | (x << 8))
#define u32le(x) ((x >> 24) | ((x >> 8) & 0xFF00) | ((x >> 8) & 0xFF0000) | (x << 24))
#define s16le(x) u16le(x)
#define s32le(x) u32le(x)
#else
#define u16le(x) (x)
#define u32le(x) (x)
#define s16le(x) (x)
#define s32le(x) (x)
#endif

#define MAX_LEVELS 100
#define MAX_PLANES 3
#define LEVEL_NAME_MAX 16

#define CARMACK_NEAR 0xA7
#define CARMACK_FAR 0xA8

#define PLANE_WALLS 0
#define PLANE_OBJECTS 1
#define PLANE_MISC 2

#define SIDE_UPPER 0
#define SIDE_LOWER 1
#define SIDE_MIDDLE 2

#define NO_SIDEDEF 0xFFFF
#define NO_SECTOR 0xFFFF

#define LF_BLOCKING 0x0001
#define LF_TWO_SIDED 0x0004
#define LF_UNPEG_LOW 0x0010
#define LF_SECRET 0x0020
#define LF_BLOCK_SOUND 0x0040

#define ST_NORMAL 0
#define ST_SLIME5 7
#define ST_SLIME10 5
#define ST_SLIME20 16
#define ST_SECRET 9

#define LT_NORMAL 0
#define LT_DOOR 1
#define LT_DOOR_FAST 117
#define LT_DOOR_RED 28
#define LT_DOOR_YELLOW 27
#define LT_DOOR_BLUE 26
#define LT_DOOR_RED_CARD 14407
#define LT_DOOR_YELLOW_CARD 14535
#define LT_DOOR_BLUE_CARD 14471
#define LT_DOOR_RED_SKULL 14599
#define LT_DOOR_YELLOW_SKULL 14727
#define LT_DOOR_BLUE_SKULL 14663
#define LT_SWITCH 103
#define LT_TELEPORT 97
#define LT_SECRET 118
#define LT_EXIT 11
#define LT_SECRET_EXIT 51

struct WolfMap {
    uint8_t id;
    char name[LEVEL_NAME_MAX];
    uint16_t width, height;

    int32_t offsets[MAX_PLANES];
    uint16_t sizes[MAX_PLANES];
    uint16_t* planes[MAX_PLANES];
};

struct DoomThing {
    int16_t x, y;
    uint16_t angle;
    uint16_t ednum;
    uint16_t flags;
};

struct DoomLine {
    uint16_t start, end;
    uint16_t flags;
    uint16_t special, tag;
    uint16_t front, back;
};

struct DoomSide {
    int16_t x_offset, y_offset;
    char textures[3][LUMP_NAME_MAX];
    uint16_t sector;
};

struct DoomVertex {
    int16_t x, y;
};

struct DoomSector {
    int16_t floor, ceiling;
    char flats[2][LUMP_NAME_MAX];
    uint16_t brightness;
    uint16_t special, tag;
};

struct LineCell {
    uint16_t tile;
    const struct AreaInfo* area;
    const struct WallInfo* wall;
    const struct DoorInfo* door;

    bool secret;
    uint16_t sector;
    bool fright, ftop, fleft, fbottom;
    bool sright, stop, sleft, sbottom;
    int16_t right, top, left, bottom;
};

struct DoomMap {
    struct DoomThing* things;
    struct DoomLine* lines;
    struct DoomSide* sides;
    struct DoomVertex* vertices;
    struct DoomSector* sectors;

    struct LineCell* linemap;
    uint16_t* sectormap;
    uint16_t last_asector;

    size_t num_things, num_lines, num_sides, num_vertices, num_sectors;
};

void map_init(const char*, const char*, int);
void map_teardown();

uint16_t read_u16le(const uint8_t*);
void read_carmack(FILE*, size_t, size_t, uint8_t*);
void read_rlew(uint8_t*, uint8_t*, uint16_t);

void map_to_wad(const char*);
bool place_free(struct LineCell*, int, int);
bool floor_free(struct LineCell*, int, int);

void write_string(FILE*, const char*, size_t);
void write_u16le(FILE*, uint16_t);
void write_s16le(FILE*, int16_t);
void write_u32le(FILE*, uint32_t);

uint16_t add_vertex(int16_t, int16_t);
uint16_t add_side(const char*, const char*, const char*, uint16_t, int16_t, int16_t);
uint16_t add_line(
    uint16_t, uint16_t, const char*, const char*, const char*, const char*, const char*, const char*, uint16_t,
    uint16_t, uint16_t, uint16_t, uint16_t, int16_t, int16_t
);
uint16_t add_sector(struct AreaInfo*);
uint16_t add_custom_sector(uint16_t, int16_t, int16_t, const char*, const char*, uint16_t, uint16_t, uint16_t);
