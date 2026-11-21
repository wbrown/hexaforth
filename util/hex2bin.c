//
// hex2bin [--rom] IMAGE.hex IMAGE.bin
//
// An image as its bytes, for a kernel that is assembled round one: up to
// the last byte that is not zero, and then to the end of that cell.
//
// With --rom it is the kernel's ROM alone, the first KROM_BYTES of the
// image: what a machine has in memory when it starts with nothing else,
// and reads the rest from its disk (doc/boot.md).
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../vm_constants.h"
#include "image.h"

int main(int argc, char *argv[]) {
    int rom = argc == 4 && !strcmp(argv[1], "--rom");
    if (argc != 3 + rom) {
        fprintf(stderr, "usage: hex2bin [--rom] IMAGE.hex IMAGE.bin\n");
        return(1);
    }
    uint8_t* bytes;
    size_t len = load_image(argv[1 + rom], &bytes);
    if (rom) {
        if (len < KROM_BYTES) {
            fprintf(stderr, "%s is %zu bytes, and a ROM is %d\n", argv[1 + rom], len, KROM_BYTES);
            return(1);
        }
        len = KROM_BYTES;
    } else {
        while (len && !bytes[len - 1]) len--;
        while (len % CELL_BYTES) len++;
    }

    FILE* out = fopen(argv[2 + rom], "wb");
    if (out == NULL) {
        perror(argv[2 + rom]);
        exit(EXIT_FAILURE);
    }
    fwrite(bytes, 1, len, out);
    fclose(out);
    free(bytes);
    return(0);
}
