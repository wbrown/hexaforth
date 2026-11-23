//
// examine IMAGE.hex IMAGE.tables IMAGE.sizes [FILE.fs]
//
// Reports what an image is made of: its pages of code, the source files
// its code comes from, and the rest of its memory.  layout.h defines and
// finds each of those.  This prints what it found, in bytes:
//
//   The kernel: its number of words, its length, and the space left in its
//   ROM.
//
//   One line per page that has code: the number of words that start on it,
//   its code, and the space left on the page after it.
//
//   One line per source file: its words and code as they are in the
//   image, and the pages they are on, which are its own page and the
//   kernel's if some of its words were placed there.  Then the data and
//   the dictionary headers that cross.fs counted for the file (.sizes).
//   For a file that includes others, those figures include theirs.  The
//   nucleus's own line is whatever belongs to none of the files it
//   includes.
//
//   The dictionary, the data up to the data pointer, and what lies past
//   it.
//
// With a file named, it also lists that file's words, each with its
// address and length.  It fails if the image has no words, because then it
// examined nothing.
//

#include "layout.h"

static layout IMAGE;

// Converts a number of instructions to bytes.
static long bytes(long instructions) { return(instructions * (long)sizeof(instruction)); }

static void pages_said(const held* has) {
    for(int page=0; page < (int)(8 * sizeof(has->pages)); page++) if (has->pages >> page & 1) printf(" %d", page);
}

int main(int argc, char *argv[]) {
    if (argc < 4 || argc > 5) {
        fprintf(stderr, "usage: examine IMAGE.hex IMAGE.tables IMAGE.sizes [FILE.fs]\n");
        return(1);
    }
    lay_out(&IMAGE, argv[1], argv[2]);
    span* FILES;
    int files = every_file_of(argv[3], &FILES);
    held* HAS = room_for(files + 1, sizeof(held));      // one per file, plus one for the nucleus itself
    held_by(&IMAGE, FILES, files, HAS);
    const held* own = &HAS[files];

    long words = 0, code = 0;
    for(long page=0; page < IMAGE.npages; page++) {
        words += IMAGE.pages[page].words;
        code += IMAGE.pages[page].code;
    }
    printf("%s, cells of %d bits: %ld bytes; %ld words, %ld bytes of code\n",
           argv[1], CELL_BITS, (long)IMAGE.len, words, bytes(code));
    printf("the kernel: %ld words, %ld bytes of the %d of its ROM, %ld left\n",
           own->words, bytes(IMAGE.kernel), KROM_BYTES, (long)KROM_BYTES - bytes(IMAGE.kernel));

    printf("\npages of code   words    code    left\n");
    for(long page=0; page < IMAGE.npages; page++) {
        const code_page* has = &IMAGE.pages[page];
        if (!has->words) continue;
        printf("  page %-6ld %7ld %7ld %7ld\n", page, has->words, bytes(has->code), bytes(PAGE - has->end));
    }

    // The data and headers cross.fs counted for the nucleus itself: the
    // whole image's figures, minus those of the files that no other file
    // includes.
    long own_data = 0, own_headers = 0;
    int whole = 0;
    for(int idx=0; idx < files; idx++) {
        if (is_all_of_it(&FILES[idx])) {
            own_data += FILES[idx].data, own_headers += FILES[idx].headers, whole = 1;
        } else if (!is_included(FILES, files, idx)) {
            own_data -= FILES[idx].data, own_headers -= FILES[idx].headers;
        }
    }
    printf("\nsource files       words    code    data headers  pages\n");
    printf("  %-16s %7ld %7ld", "(the nucleus)", own->words, bytes(own->code));
    if (whole) printf(" %7ld %7ld ", own_data, own_headers); else printf("       -       - ");
    pages_said(own);
    printf("\n");
    for(int idx=0; idx < files; idx++) {
        if (is_all_of_it(&FILES[idx]) || is_laid_near(&FILES[idx])) continue;   // code placed on the kernel's page is in its file's line
        printf("  %-16s %7ld %7ld %7ld %7ld ", FILES[idx].file, HAS[idx].words, bytes(HAS[idx].code),
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
        for(int idx=0; idx < files; idx++) if (!is_laid_near(&FILES[idx]) && !strcmp(FILES[idx].file, argv[4])) which = idx;
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
