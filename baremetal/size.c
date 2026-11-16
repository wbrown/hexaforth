//
// kernel_size KERNEL.symbols KERNEL.bin
//
// How big a kernel is: its own code, which is from its symbol _start to
// its symbol native_end (symbols.c), and the whole of it with the image it
// has in it, which is the file.
//

#include <stdio.h>
#include "symbols.c"

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s KERNEL.symbols KERNEL.bin\n", argv[0]);
        return(1);
    }
    unsigned char* bytes;
    size_t whole = file_bytes(argv[2], &bytes);
    if (!whole) {
        fprintf(stderr, "%s is not a kernel: there is nothing in it\n", argv[2]);
        return(1);
    }
    printf("native code: %llu bytes\n",
           (unsigned long long)(symbol_at(argv[1], "native_end") - symbol_at(argv[1], "_start")));
    printf("whole image: %zu bytes\n", whole);
    return(0);
}
