//
// examine IMAGE.hex IMAGE.tables IMAGE.sizes [FILE.fs]
//
// What an image is made of: its pages of code, the source files its code
// is from, and the rest of its memory.  layout.h says what each of those
// is and finds them; this prints what it found, in bytes.
//
//   The kernel: its words, how long it is, and what is left of its ROM.
//
//   A line a page that has code: the words that start on it, its code, its
//   tables, which are not code, and what of the page is left after them.
//
//   A line a source file: its words, its code and its tables as they are
//   in the image, and the pages they are on; then the data and the
//   dictionary headers that cross.fs counted for it (.sizes), which for a
//   file that includes others have theirs in them.  The nucleus's own is
//   what no file it includes has.
//
//   The dictionary, the data as far as the data pointer, and what there is
//   past it.
//
// With a file named, that file's words too, each with where it is and how
// long.  It fails if the image has no words: then it looked at nothing.
//

#include "layout.h"

static layout IMAGE;

// A number of instructions, as the bytes they are.
static long bytes(long instructions) { return(instructions * (long)sizeof(instruction)); }

static void pages_said(const held* has) {
    if (has->first_page < 0) return;
    printf(" %ld", has->first_page);
    if (has->last_page != has->first_page) printf("-%ld", has->last_page);
}

int main(int argc, char *argv[]) {
    if (argc < 4 || argc > 5) {
        fprintf(stderr, "usage: examine IMAGE.hex IMAGE.tables IMAGE.sizes [FILE.fs]\n");
        return(1);
    }
    lay_out(&IMAGE, argv[1], argv[2]);
    span* FILES;
    int files = every_file_of(argv[3], &FILES);
    held* HAS = room_for(files + 1, sizeof(held));      // and one for the nucleus's own
    held_by(&IMAGE, FILES, files, HAS);
    const held* own = &HAS[files];

    long words = 0, code = 0, tables = 0;
    for(long page=0; page < IMAGE.npages; page++) {
        words += IMAGE.pages[page].words;
        code += IMAGE.pages[page].code;
        tables += IMAGE.pages[page].tables;
    }
    printf("%s, cells of %d bits: %ld bytes; %ld words, %ld bytes of code and %ld of tables\n",
           argv[1], CELL_BITS, (long)IMAGE.len, words, bytes(code), bytes(tables));
    printf("the kernel: %ld words, %ld bytes of the %d of its ROM, %ld left\n",
           own->words, bytes(IMAGE.kernel), KROM_BYTES, (long)KROM_BYTES - bytes(IMAGE.kernel));

    printf("\npages of code   words    code  tables    left\n");
    for(long page=0; page < IMAGE.npages; page++) {
        const code_page* has = &IMAGE.pages[page];
        if (!has->words) continue;
        printf("  page %-6ld %7ld %7ld %7ld %7ld\n", page, has->words, bytes(has->code), bytes(has->tables), bytes(PAGE - has->end));
    }

    // What cross.fs counted for the nucleus's own: the whole image's, less
    // that of the files that no other file includes.
    long own_data = 0, own_headers = 0;
    int whole = 0;
    for(int idx=0; idx < files; idx++) {
        if (is_all_of_it(&FILES[idx])) {
            own_data += FILES[idx].data, own_headers += FILES[idx].headers, whole = 1;
        } else if (!is_included(FILES, files, idx)) {
            own_data -= FILES[idx].data, own_headers -= FILES[idx].headers;
        }
    }
    printf("\nsource files       words    code  tables    data headers  pages\n");
    printf("  %-16s %7ld %7ld %7ld", "(the nucleus)", own->words, bytes(own->code), bytes(own->tables));
    if (whole) printf(" %7ld %7ld ", own_data, own_headers); else printf("       -       - ");
    pages_said(own);
    printf("\n");
    for(int idx=0; idx < files; idx++) {
        if (is_all_of_it(&FILES[idx])) continue;
        printf("  %-16s %7ld %7ld %7ld %7ld %7ld ", FILES[idx].file, HAS[idx].words, bytes(HAS[idx].code), bytes(HAS[idx].tables),
               FILES[idx].data, FILES[idx].headers);
        pages_said(&HAS[idx]);
        printf("\n");
    }

    printf("\nthe rest\n");
    printf("  the dictionary: %d headers, %ld bytes from byte %ld\n",
           IMAGE.headers, (long)(IMAGE.headers_to - IMAGE.headers_from), (long)IMAGE.headers_from);
    printf("  the data: %ld bytes from byte %ld, which is cell %ld, to the data pointer\n",
           (long)(IMAGE.data_to - IMAGE.data_from), (long)IMAGE.data_from, (long)VAR_BASE);
    printf("  past the data pointer: %ld bytes that are not nothing\n", (long)IMAGE.past);

    if (argc == 5) {
        int which = -1;
        for(int idx=0; idx < files; idx++) if (!strcmp(FILES[idx].file, argv[4])) which = idx;
        if (which < 0) {
            fprintf(stderr, "examine: the image's .sizes has no %s\n", argv[4]);
            return(1);
        }
        printf("\nthe words of %s   place   bytes\n", argv[4]);
        for(int idx=0; idx < IMAGE.n; idx++) {
            const word* w = &IMAGE.words[idx];
            if (w->size && file_holding(FILES, files, w->at) == which) printf("  %-20s %7ld %7ld\n", w->name, w->at, bytes(w->size));
        }
    }
    return(!words);
}
