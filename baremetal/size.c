//
// kernel_size KERNEL.symbols KERNEL.bin
//
// Prints the size of a kernel: its own code, which runs from its symbol
// _start to its symbol native_end (symbols.h), and the whole file, which
// also has the image in it.
//
// Then it prints the parts of the kernel's own code: one line for each
// label, with the bytes from that label to the next.  The kernel's source
// says at each label what the part is (hexaforth.S).  The program fails if
// the parts do not add up to the whole.
//

#include <stdio.h>
#include "symbols.h"

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
    uint64_t from = symbol_at(argv[1], "_start"), to = symbol_at(argv[1], "native_end");
    printf("native code: %llu bytes\n", (unsigned long long)(to - from));
    printf("whole image: %zu bytes\n", whole);

    symbol* parts;
    long n = symbols_between(argv[1], from, to, &parts);
    uint64_t all = 0;
    printf("\nits parts      bytes\n");
    for(long idx=0; idx < n; idx++) {
        uint64_t next = idx + 1 < n ? parts[idx + 1].at : to;
        if (next == parts[idx].at) continue;            // a second name for the same address
        printf("  %-12s %5llu\n", parts[idx].name, (unsigned long long)(next - parts[idx].at));
        all += next - parts[idx].at;
    }
    return(all == to - from ? 0 : 1);
}
