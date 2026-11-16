//
// kernel_size KERNEL.symbols KERNEL.bin
//
// How big a kernel is: its own code, which is from its symbol _start to
// its symbol native_end (symbols.c), and the whole of it with the image it
// has in it, which is the file.
//
// Then the parts of its own code: a line for each label it has, with the
// bytes from that label to the next.  What a part is is the kernel's to
// say, at the label (hexaforth.S).  It fails if the parts do not come to
// the whole.
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
    uint64_t from = symbol_at(argv[1], "_start"), to = symbol_at(argv[1], "native_end");
    printf("native code: %llu bytes\n", (unsigned long long)(to - from));
    printf("whole image: %zu bytes\n", whole);

    symbol* parts;
    long n = symbols_between(argv[1], from, to, &parts);
    uint64_t all = 0;
    printf("\nits parts      bytes\n");
    for(long idx=0; idx < n; idx++) {
        uint64_t next = idx + 1 < n ? parts[idx + 1].at : to;
        if (next == parts[idx].at) continue;            // a second name for the same place
        printf("  %-12s %5llu\n", parts[idx].name, (unsigned long long)(next - parts[idx].at));
        all += next - parts[idx].at;
    }
    return(all == to - from ? 0 : 1);
}
