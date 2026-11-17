#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "image.h"

size_t load_image(const char* path, uint8_t** bytes) {
    FILE* hexfile = fopen(path, "r");
    if (hexfile == NULL) {
        perror(path);
        exit(EXIT_FAILURE);
    }

    char line[100];
    size_t len = 0;
    size_t room = 1 << 16;
    *bytes = malloc(room);
    while (fgets(line, sizeof(line), hexfile)) {
        if (!isxdigit(line[0])) continue;
        if (len + 4 + 8 > room) {
            room *= 2;
            *bytes = realloc(*bytes, room);
        }
        uint32_t four = (uint32_t)strtoul(line, NULL, 16);
        for(int idx=0; idx < 4; idx++) {
            (*bytes)[len++] = four >> (8 * idx);
        }
    }
    fclose(hexfile);
    memset(*bytes + len, 0, 8);
    return(len);
}
