#include <stdlib.h>

#include "config.h"

static struct Config config = {0};

void config_init(const char* config_name) {
    // Open file
    yyjson_read_err error;
    yyjson_doc* json = yyjson_read_file(config_name, JSON_FLAGS, NULL, &error);
    if (json == NULL) {
        printf("!!! config_init: Failed to read \"%s\" (%s)\n", config_name, error.msg);
        exit(EXIT_FAILURE);
    }

    yyjson_val* root = yyjson_doc_get_root(json);
    if (!yyjson_is_obj(root)) {
        printf("!!! config_init: Expected root object in \"%s\", got %s\n", config_name, yyjson_get_type_desc(root));
        exit(EXIT_FAILURE);
    }

    // Information
    parse_name(config.name, NAME_MAX, yyjson_obj_get(root, "name"), "Untitled");
    parse_map_format(&config.format, yyjson_obj_get(root, "format"));
    printf("config_init: Using config \"%s\" (format: %u)\n", config.name, config.format);

    // Defaults
    parse_name(config.flats[FLAT_FLOOR], LUMP_NAME_MAX, yyjson_obj_get(root, "floor"), "FLAT5_4");
    parse_name(config.flats[FLAT_CEILING], LUMP_NAME_MAX, yyjson_obj_get(root, "ceiling"), "CEIL5_1");
    parse_uint8(&config.brightness, yyjson_obj_get(root, "brightness"), 160);
    /*printf(
        "config_init: Set defaults (tex: %s/%s, light: %u)\n", config.flats[FLAT_FLOOR], config.flats[FLAT_CEILING],
        config.brightness
    );*/

    // Definitions
    parse_walls(&config.walls, &config.num_walls, yyjson_obj_get(root, "walls"));
    parse_doors(&config.doors, &config.num_doors, yyjson_obj_get(root, "doors"));
    parse_objects(&config.objects, &config.num_objects, yyjson_obj_get(root, "objects"));
    parse_areas(&config.areas, &config.num_areas, yyjson_obj_get(root, "areas"));

    // Close file
    yyjson_doc_free(json);
}

void config_teardown() {
    if (config.walls != NULL)
        free(config.walls);
    if (config.doors != NULL)
        free(config.doors);
    if (config.objects != NULL)
        free(config.objects);
    if (config.areas != NULL)
        free(config.areas);
}

void parse_name(char* string, size_t size, yyjson_val* value, const char* default_value) {
    strncpy(string, yyjson_is_str(value) ? yyjson_get_str(value) : default_value, size);
}

void parse_uint8(uint8_t* ptr, yyjson_val* value, uint8_t default_value) {
    *ptr = (value == NULL || !yyjson_is_num(value)) ? default_value : (uint8_t)yyjson_get_uint(value);
}

void parse_uint16(uint16_t* ptr, yyjson_val* value, uint16_t default_value) {
    *ptr = (value == NULL || !yyjson_is_num(value)) ? default_value : (uint16_t)yyjson_get_uint(value);
}

void parse_map_format(enum MapFormats* ptr, yyjson_val* value) {
    if (value == NULL || !yyjson_is_str(value)) {
        *ptr = MAPF_MBF21;
        return;
    }

    const char* format = yyjson_get_str(value);
    if (strcmp(format, "doom") == 0)
        *ptr = MAPF_DOOM;
    else if (strcmp(format, "boom") == 0)
        *ptr = MAPF_BOOM;
    else if (strcmp(format, "mbf") == 0)
        *ptr = MAPF_MBF;
    else
        *ptr = MAPF_MBF21;
}

void parse_walls(struct WallInfo** walls, size_t* num_walls, yyjson_val* value) {
    if (value == NULL || !yyjson_is_obj(value)) {
        *walls = NULL;
        *num_walls = 0;
        return;
    }

    *num_walls = yyjson_obj_size(value);
    *walls = calloc(*num_walls, sizeof(struct WallInfo));
    if (*walls == NULL) {
        printf("!!! parse_walls: Out of memory for wall map\n");
        exit(EXIT_FAILURE);
    }

    size_t i, n;
    yyjson_val *key, *val;
    yyjson_obj_foreach(value, i, n, key, val) {
        struct WallInfo* wall = &(*walls)[i];
        if ((wall->id = atoi(yyjson_get_str(key))) == 0) {
            printf("!!! parse_walls: Expected wall ID as non-zero integer\n");
            exit(EXIT_FAILURE);
        }

        if (!yyjson_is_obj(val)) {
            printf("!!! parse_walls: Expected wall %d info as object, got %s\n", wall->id, yyjson_get_type_desc(val));
            exit(EXIT_FAILURE);
        }

        parse_name(wall->name, NAME_MAX, yyjson_obj_get(val, "name"), "Untitled");
        parse_name(wall->textures[SIDE_X], LUMP_NAME_MAX, yyjson_obj_get(val, "xtex"), "");
        parse_name(wall->textures[SIDE_Y], LUMP_NAME_MAX, yyjson_obj_get(val, "ytex"), wall->textures[SIDE_X]);
        parse_wall_action(&wall->actions[SIDE_X], yyjson_obj_get(val, "xact"));
        parse_wall_action(&wall->actions[SIDE_Y], yyjson_obj_get(val, "yact"));
        parse_uint16(&wall->tag, yyjson_obj_get(val, "tag"), 0);

        /*printf(
            "parse_walls: Wall %d is \"%s\" (tex: %s/%s, act: %u/%u, tag: %u)\n", wall->id, wall->name,
            wall->textures[SIDE_X], wall->textures[SIDE_Y], wall->actions[SIDE_X], wall->actions[SIDE_Y], wall->tag
        );*/
    }

    // printf("parse_walls: Found %zu wall(s)\n", *num_walls);
}

void parse_wall_action(enum WallActions* ptr, yyjson_val* value) {
    if (value == NULL || !yyjson_is_str(value)) {
        *ptr = WACT_NONE;
        return;
    }

    const char* action = yyjson_get_str(value);
    if (strcmp(action, "switch") == 0)
        *ptr = WACT_SWITCH;
    else if (strcmp(action, "exit") == 0)
        *ptr = WACT_EXIT;
    else
        *ptr = WACT_NONE;
}

void parse_doors(struct DoorInfo** doors, size_t* num_doors, yyjson_val* value) {
    if (value == NULL || !yyjson_is_obj(value)) {
        *doors = NULL;
        *num_doors = 0;
        return;
    }

    *num_doors = yyjson_obj_size(value);
    *doors = calloc(*num_doors, sizeof(struct DoorInfo));
    if (*doors == NULL) {
        printf("!!! parse_doors: Out of memory for door map\n");
        exit(EXIT_FAILURE);
    }

    size_t i, n;
    yyjson_val *key, *val;
    yyjson_obj_foreach(value, i, n, key, val) {
        struct DoorInfo* door = &(*doors)[i];
        if ((door->id = atoi(yyjson_get_str(key))) == 0) {
            printf("!!! parse_doors: Expected door ID as non-zero integer\n");
            exit(EXIT_FAILURE);
        }

        if (!yyjson_is_obj(val)) {
            printf("!!! parse_doors: Expected door %d info as object, got %s\n", door->id, yyjson_get_type_desc(val));
            exit(EXIT_FAILURE);
        }

        parse_name(door->name, NAME_MAX, yyjson_obj_get(val, "name"), "Untitled");
        parse_door_type(&door->type, yyjson_obj_get(val, "type"));
        parse_door_axis(&door->axis, yyjson_obj_get(val, "axis"));
        parse_name(door->flats[FLAT_FLOOR], LUMP_NAME_MAX, yyjson_obj_get(val, "floor"), config.flats[FLAT_FLOOR]);
        parse_name(
            door->flats[FLAT_CEILING], LUMP_NAME_MAX, yyjson_obj_get(val, "ceiling"), config.flats[FLAT_CEILING]
        );
        parse_name(door->sides[SIDE_LEFT], LUMP_NAME_MAX, yyjson_obj_get(val, "ltex"), "");
        parse_name(door->sides[SIDE_RIGHT], LUMP_NAME_MAX, yyjson_obj_get(val, "rtex"), door->sides[SIDE_LEFT]);
        parse_name(door->track, LUMP_NAME_MAX, yyjson_obj_get(val, "track"), "");
        parse_uint16(&door->tag, yyjson_obj_get(val, "tag"), 0);

        /*printf(
            "parse_doors: Door %d is \"%s\" (type: %u, axis: %u, flat: %s/%s, side: %s/%s, track: %s, tag: %u)\n",
            door->id, door->name, door->type, door->axis, door->flats[FLAT_FLOOR], door->flats[FLAT_CEILING],
            door->sides[SIDE_LEFT], door->sides[SIDE_RIGHT], door->track, door->tag
        );*/
    }

    // printf("parse_doors: Found %zu door(s)\n", *num_doors);
}

void parse_door_type(enum DoorTypes* ptr, yyjson_val* value) {
    if (value == NULL || !yyjson_is_str(value)) {
        *ptr = DOOR_NORMAL;
        return;
    }

    const char* type = yyjson_get_str(value);
    if (strcmp(type, "fast") == 0)
        *ptr = DOOR_FAST;
    else if (strcmp(type, "switch") == 0)
        *ptr = DOOR_SWITCH;
    else if (strcmp(type, "red") == 0)
        *ptr = DOOR_RED;
    else if (strcmp(type, "yellow") == 0)
        *ptr = DOOR_YELLOW;
    else if (strcmp(type, "blue") == 0)
        *ptr = DOOR_BLUE;
    else
        *ptr = DOOR_NORMAL;
}

void parse_door_axis(enum DoorAxes* ptr, yyjson_val* value) {
    *ptr = (value == NULL || !yyjson_is_str(value) || strcmp(yyjson_get_str(value), "y") != 0) ? DAX_X : DAX_Y;
}

void parse_objects(struct ObjectInfo** objects, size_t* num_objects, yyjson_val* value) {
    if (value == NULL || !yyjson_is_obj(value)) {
        *objects = NULL;
        *num_objects = 0;
        return;
    }

    *num_objects = yyjson_obj_size(value);
    *objects = calloc(*num_objects, sizeof(struct ObjectInfo));
    if (*objects == NULL) {
        printf("!!! parse_objects: Out of memory for object map\n");
        exit(EXIT_FAILURE);
    }

    size_t i, n;
    yyjson_val *key, *val;
    yyjson_obj_foreach(value, i, n, key, val) {
        struct ObjectInfo* object = &(*objects)[i];
        if ((object->id = atoi(yyjson_get_str(key))) == 0) {
            printf("!!! parse_objects: Expected object ID as non-zero integer\n");
            exit(EXIT_FAILURE);
        }

        if (!yyjson_is_obj(val)) {
            printf(
                "!!! parse_objects: Expected object %d info as object, got %s\n", object->id, yyjson_get_type_desc(val)
            );
            exit(EXIT_FAILURE);
        }

        parse_name(object->name, NAME_MAX, yyjson_obj_get(val, "name"), "Untitled");
        parse_object_type(&object->type, yyjson_obj_get(val, "type"));

        if (object->type == OBJ_THING) {
            parse_uint16(&object->ednum, yyjson_obj_get(val, "ednum"), 0);
            if (object->ednum <= 0) {
                printf(
                    "!!! parse_objects: Expected object %d DoomEdNum as non-zero unsigned 16-bit integer\n", object->id
                );
                exit(EXIT_FAILURE);
            }

            parse_uint16(&object->angle, yyjson_obj_get(val, "angle"), 0);
            parse_object_flags(&object->flags, val);
        } else {
            object->ednum = 0;
            object->angle = 0;
            object->flags = TF_NONE;
        }

        /*printf(
            "parse_objects: Object %d is \"%s\" (type: %u, ednum: %u, angle: %u, flags: %u)\n", object->id,
            object->name, object->type, object->ednum, object->angle, object->flags
        );*/
    }

    // printf("parse_objects: Found %zu object(s)\n", *num_objects);
}

void parse_object_type(enum ObjectTypes* ptr, yyjson_val* value) {
    if (value == NULL || !yyjson_is_str(value)) {
        *ptr = OBJ_MARKER;
        return;
    }

    const char* type = yyjson_get_str(value);
    if (strcmp(type, "thing") == 0)
        *ptr = OBJ_THING;
    else if (strcmp(type, "pushwall") == 0)
        *ptr = OBJ_PUSHWALL;
    else
        *ptr = OBJ_MARKER;
}

void parse_object_flags(enum ThingFlags* ptr, yyjson_val* value) {
    *ptr = TF_NONE;

    yyjson_val* flag;
    if ((flag = yyjson_obj_get(value, "easy")) == NULL || !yyjson_is_bool(flag) || yyjson_get_bool(flag))
        *ptr |= TF_EASY;
    if ((flag = yyjson_obj_get(value, "normal")) == NULL || !yyjson_is_bool(flag) || yyjson_get_bool(flag))
        *ptr |= TF_NORMAL;
    if ((flag = yyjson_obj_get(value, "hard")) == NULL || !yyjson_is_bool(flag) || yyjson_get_bool(flag))
        *ptr |= TF_HARD;
    if ((flag = yyjson_obj_get(value, "ambush")) != NULL && yyjson_is_bool(flag) && yyjson_get_bool(flag))
        *ptr |= TF_AMBUSH;
    if ((flag = yyjson_obj_get(value, "multiplayer")) != NULL && yyjson_is_bool(flag) && yyjson_get_bool(flag))
        *ptr |= TF_MULTIPLAYER;
    if ((flag = yyjson_obj_get(value, "no_deathmatch")) != NULL && yyjson_is_bool(flag) && yyjson_get_bool(flag))
        *ptr |= TF_NO_DEATHMATCH;
    if ((flag = yyjson_obj_get(value, "no_coop")) != NULL && yyjson_is_bool(flag) && yyjson_get_bool(flag))
        *ptr |= TF_NO_COOP;
    if ((flag = yyjson_obj_get(value, "friendly")) != NULL && yyjson_is_bool(flag) && yyjson_get_bool(flag))
        *ptr |= TF_FRIENDLY;
}

void parse_areas(struct AreaInfo** areas, size_t* num_areas, yyjson_val* value) {
    if (value == NULL || !yyjson_is_obj(value)) {
        *areas = NULL;
        *num_areas = 0;
        return;
    }

    *num_areas = yyjson_obj_size(value);
    *areas = calloc(*num_areas, sizeof(struct AreaInfo));
    if (*areas == NULL) {
        printf("!!! parse_areas: Out of memory for area map\n");
        exit(EXIT_FAILURE);
    }

    size_t i, n;
    yyjson_val *key, *val;
    yyjson_obj_foreach(value, i, n, key, val) {
        struct AreaInfo* area = &(*areas)[i];
        if ((area->id = atoi(yyjson_get_str(key))) == 0) {
            printf("!!! parse_areas: Expected area ID as non-zero integer\n");
            exit(EXIT_FAILURE);
        }

        if (!yyjson_is_obj(val)) {
            printf("!!! parse_areas: Expected area %d info as object, got %s\n", area->id, yyjson_get_type_desc(val));
            exit(EXIT_FAILURE);
        }

        parse_name(area->name, NAME_MAX, yyjson_obj_get(val, "name"), "Untitled");
        parse_area_type(&area->type, yyjson_obj_get(val, "type"));
        parse_name(area->flats[FLAT_FLOOR], LUMP_NAME_MAX, yyjson_obj_get(val, "floor"), config.flats[FLAT_FLOOR]);
        parse_name(
            area->flats[FLAT_CEILING], LUMP_NAME_MAX, yyjson_obj_get(val, "ceiling"), config.flats[FLAT_CEILING]
        );
        parse_uint8(&area->brightness, yyjson_obj_get(val, "brightness"), config.brightness);

        /*printf(
            "parse_areas: Area %d is \"%s\" (type: %u, flat: %s/%s, light: %u)\n", area->id, area->name, area->type,
            area->flats[FLAT_FLOOR], area->flats[FLAT_CEILING], area->brightness
        );*/
    }

    // printf("parse_areas: Found %zu area(s)\n", *num_areas);
}

void parse_area_type(enum AreaTypes* ptr, yyjson_val* value) {
    if (value == NULL || !yyjson_is_str(value)) {
        *ptr = AREA_NORMAL;
        return;
    }

    const char* type = yyjson_get_str(value);
    if (strcmp(type, "ambush") == 0)
        *ptr = AREA_AMBUSH;
    else if (strcmp(type, "secret_exit") == 0)
        *ptr = AREA_SECRET_EXIT;
    else
        *ptr = AREA_NORMAL;
}

const struct Config* get_config() {
    return &config;
}

const struct DoorInfo* get_door_info(int id) {
    if (id <= 0)
        return NULL;
    for (size_t i = 0; i < config.num_doors; i++)
        if (config.doors[i].id == id)
            return &config.doors[i];
    return NULL;
}

const struct WallInfo* get_wall_info(int id) {
    if (id <= 0)
        return NULL;
    for (size_t i = 0; i < config.num_walls; i++)
        if (config.walls[i].id == id)
            return &config.walls[i];
    return NULL;
}

const struct ObjectInfo* get_object_info(int id) {
    if (id <= 0)
        return NULL;
    for (size_t i = 0; i < config.num_objects; i++)
        if (config.objects[i].id == id)
            return &config.objects[i];
    return NULL;
}

const struct AreaInfo* get_area_info(int id) {
    if (id <= 0)
        return NULL;
    for (size_t i = 0; i < config.num_areas; i++)
        if (config.areas[i].id == id)
            return &config.areas[i];
    return NULL;
}

bool oid_is_pushwall(int id) {
    const struct ObjectInfo* obj = get_object_info(id);
    return obj != NULL && obj->type == OBJ_PUSHWALL;
}

bool aid_is_secret_exit(int id) {
    const struct AreaInfo* area = get_area_info(id);
    return area != NULL && area->type == AREA_SECRET_EXIT;
}

bool aid_is_ambush(int id) {
    const struct AreaInfo* area = get_area_info(id);
    return area != NULL && area->type == AREA_AMBUSH;
}
