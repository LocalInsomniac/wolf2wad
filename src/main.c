#include <stdlib.h>

#include "config.h"
#include "map.h"

int main(int argc, char** argv) {
    printf("wolf2wad for WolfenDOOM Redux\n");
    printf("Usage: wolf2wad [-c <file>] [-i <maphead> <gamemaps>] [-l <level>] [-o <file>]\n");

    char *config_name = NULL, *maphead_name = NULL, *gamemaps_name = NULL;
    char* output_name = NULL;
    int level = 0;

    for (int i = 0; i < argc; i++)
        if (strcmp(argv[i], "-c") == 0) {
            config_name = argv[++i];
        } else if (strcmp(argv[i], "-i") == 0) {
            maphead_name = argv[++i];
            gamemaps_name = argv[++i];
        } else if (strcmp(argv[i], "-l") == 0) {
            level = strtoul(argv[++i], NULL, 0);
        } else if (strcmp(argv[i], "-o") == 0) {
            output_name = argv[++i];
        }

    if (config_name == NULL) {
        printf("! Config file not specified, defaulting to \"config.json\"\n");
        config_name = "config.json";
    }

    if (maphead_name == NULL || gamemaps_name == NULL) {
        printf("! MAPHEAD.* and GAMEMAPS.* not specified, defaulting to \"MAPHEAD.wl6\" and \"GAMEMAPS.wl6\"\n");
        maphead_name = "MAPHEAD.wl6";
        gamemaps_name = "GAMEMAPS.wl6";
    }

    if (output_name == NULL) {
        printf("! Output file not specified, defaulting to \"output.wad\"\n");
        output_name = "output.wad";
    }

    config_init(config_name);
    map_init(maphead_name, gamemaps_name, level);
    map_to_wad(output_name);

    map_teardown();
    config_teardown();

    return EXIT_SUCCESS;
}
