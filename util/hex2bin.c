//
// hex2bin IMAGE.hex IMAGE.bin
//
// An image as its bytes, for a kernel that is assembled round one: up to
// the last byte that is not zero, and then to the end of that cell.
//

#include <stdio.h>
#include <stdlib.h>
#include "../vm_constants.h"
#include "image.h"

int main(int argc, char *argv[]) {
    uint8_t* bytes;
    size_t len = load_image(argv[1], &bytes);
    while (len && !bytes[len - 1]) len--;
    while (len % CELL_BYTES) len++;

    FILE* out = fopen(argv[2], "wb");
    if (out == NULL) {
        perror(argv[2]);
        exit(EXIT_FAILURE);
    }
    fwrite(bytes, 1, len, out);
    fclose(out);
    free(bytes);
    return(0);
}
