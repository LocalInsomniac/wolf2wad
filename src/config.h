#pragma once

#include "yyjson.h"

#define JSON_FLAGS (YYJSON_READ_ALLOW_COMMENTS | YYJSON_READ_ALLOW_TRAILING_COMMAS)

#define NAME_MAX 128
#define LUMP_NAME_MAX 8

#define FLAT_FLOOR 0
#define FLAT_CEILING 1

#define SIDE_X 0
#define SIDE_Y 1
#define SIDE_LEFT 0
#define SIDE_RIGHT 1

enum MapFormats {
    MAPF_DOOM,
    MAPF_BOOM,
    MAPF_MBF,
    MAPF_MBF21,
};

struct Config {
    char name[NAME_MAX];
    enum MapFormats format;

    char flats[2][LUMP_NAME_MAX];
    uint8_t brightness;

    struct WallInfo* walls;
    struct DoorInfo* doors;
    struct ObjectInfo* objects;
    struct AreaInfo* areas;
    size_t num_walls, num_doors, num_objects, num_areas;
};

enum WallActions {
    WACT_NONE,
    WACT_SWITCH,
    WACT_EXIT,
};

struct WallInfo {
    int id;
    char name[NAME_MAX];

    char textures[2][LUMP_NAME_MAX];

    enum WallActions actions[2];
    uint16_t tag;
};

enum DoorTypes {
    DOOR_NORMAL,
    DOOR_FAST,
    DOOR_SWITCH,
    DOOR_RED,
    DOOR_YELLOW,
    DOOR_BLUE,
};

enum DoorAxes {
    DAX_X,
    DAX_Y,
};

struct DoorInfo {
    int id;
    char name[NAME_MAX];

    enum DoorTypes type;
    enum DoorAxes axis;

    char flats[2][LUMP_NAME_MAX];
    char sides[2][LUMP_NAME_MAX];
    char track[LUMP_NAME_MAX];

    uint16_t tag;
};

enum ObjectTypes {
    OBJ_MARKER,
    OBJ_THING,
    OBJ_PUSHWALL,
};

enum ThingFlags {
    TF_NONE = 0x0000,
    TF_EASY = 0x0001,
    TF_NORMAL = 0x0002,
    TF_HARD = 0x0004,
    TF_AMBUSH = 0x0008,
    TF_MULTIPLAYER = 0x0010,
    TF_NO_DEATHMATCH = 0x0020,
    TF_NO_COOP = 0x0040,
    TF_FRIENDLY = 0x0080,
};

struct ObjectInfo {
    int id;
    char name[NAME_MAX];
    enum ObjectTypes type;

    uint16_t ednum, angle;
    enum ThingFlags flags;
};

enum AreaTypes {
    AREA_NORMAL,
    AREA_AMBUSH,
    AREA_SECRET_EXIT,
};

struct AreaInfo {
    int id;
    char name[NAME_MAX];
    enum AreaTypes type;

    char flats[2][LUMP_NAME_MAX];
    uint8_t brightness;
};

void config_init(const char*);
void config_teardown();

void parse_name(char*, size_t, yyjson_val*, const char*);
void parse_uint8(uint8_t*, yyjson_val*, uint8_t);
void parse_uint16(uint16_t*, yyjson_val*, uint16_t);

void parse_map_format(enum MapFormats*, yyjson_val*);

void parse_walls(struct WallInfo**, size_t*, yyjson_val*);
void parse_wall_action(enum WallActions*, yyjson_val*);

void parse_doors(struct DoorInfo**, size_t*, yyjson_val*);
void parse_door_type(enum DoorTypes*, yyjson_val*);
void parse_door_axis(enum DoorAxes*, yyjson_val*);

void parse_objects(struct ObjectInfo**, size_t*, yyjson_val*);
void parse_object_type(enum ObjectTypes*, yyjson_val*);
void parse_object_flags(enum ThingFlags*, yyjson_val*);

void parse_areas(struct AreaInfo**, size_t*, yyjson_val*);
void parse_area_type(enum AreaTypes*, yyjson_val*);

const struct Config* get_config();
const struct WallInfo* get_wall_info(int);
const struct DoorInfo* get_door_info(int);
const struct ObjectInfo* get_object_info(int);
const struct AreaInfo* get_area_info(int);
bool oid_is_pushwall(int);
bool aid_is_secret_exit(int);
bool aid_is_ambush(int);
