//
// booted BOOT.hex BOOT.tables MEMORY DISK.img WHOLE.hex WHOLE.tables
//
// What a machine compiled at its boot, beside the same words as cross.fs
// lays them.
//
// BOOT.hex is the image a machine boots with (BOOT=on), MEMORY its memory
// when it had booted from DISK.img and stopped (the reference VM writes it:
// main.c, test/desktop_test.c), and DISK.img that disk as the machine left
// it, which has the names of the words it compiled that are each file's own
// (forth/files.fs, util/words.h).  WHOLE.hex is the same system with the
// applications laid by cross.fs.
//
// A word the machine compiled is one whose code is where the image it
// booted with has nothing.  It is the same word as one of cross.fs's by its
// name, and words of one name are one line.  The machine also lays what
// cross.fs lays no word for, and those are added up by what they are:
//
//   a :: word       its body, which cross.fs runs as it compiles and the
//                   machine runs as it reads the file; it is immediate
//   a number        a word that only pushes one, which the kernel lays for
//                   every create, variable and constant, and cross.fs lays
//                   only for one that has a header
//   the rest        a name cross.fs has no word of: what a file defines
//                   between meta and target, which cross.fs runs itself
//
// A call to a word on another page is the word's place pushed and a call of
// execute: of the words both have, how many such calls each has, and how
// many instructions they are more than calls of one.
//
// The last word of a page is as long as to the end of what the page has: a
// definition that ran off the page and was compiled again on the next
// (blocks.fs) left what it had laid there, and that is counted in it.  Such
// a word is marked.
//
// It fails if no word is in both, since then nothing was compared.
//

#include <stdbool.h>
#include "../util/image.h"
#include "../util/words.h"

typedef struct {
    char name[NAME_ROOM];
    long here, there;                   // instructions the machine laid for it, and cross.fs
    long far_here, far_there;           // calls to another page in each,
    long more_here, more_there;         // and the instructions those are more than calls of one
    bool immediate, number, last;       // of the machine's: a :: word, one that only pushes, the last of its page
} line;

static line* LINES;
static long lines, lines_room;

static line* line_named(const char* name) {
    for(long n=0; n < lines; n++) if (!strcmp(LINES[n].name, name)) return(&LINES[n]);
    ONE_MORE(LINES, lines, lines_room);
    line* is = &LINES[lines++];
    memset(is, 0, sizeof(line));
    snprintf(is->name, sizeof(is->name), "%s", name);
    return(is);
}

// A literal that pushes its number, and one that adds it: not one that
// fetches or stores a variable.
static bool is_literal(uint16_t w) { return(w & 0x8000); }
static bool is_push(uint16_t w) { return((w & 0xc000) == 0x8000 && (w & 0x3000) != 0x3000); }
static bool is_add(uint16_t w) { return((w & 0xc000) == 0xc000 && (w & 0x3000) != 0x3000); }

// Does the code only push a number and return?
static bool pushes_only(const uint16_t* code, long at, long size) {
    if (size < 2 || size > 9 || !is_push(code[at])) return(false);
    for(long idx=1; idx < size - 1; idx++) {
        if (!is_literal(code[at + idx]) && code[at + idx] != 0x60dc) return(false);     // or a shift, in a wide one
    }
    return(code[at + size - 1] == 0x7403);
}

// The calls to another page in a word: a call or a jump of execute after
// what pushes a place.  How many, and the instructions before each that
// push it, which a call of one instruction would not have.
static void far_calls(const uint16_t* code, long at, long size, long execute, long* calls, long* more) {
    for(long idx=1; idx < size; idx++) {
        uint16_t w = code[at + idx];
        bool goes = (w & 0xe000) == 0x4000 || (w & 0xe000) == 0x0000;   // a call, or a jump
        if (!goes || (w & 0x1fff) != execute) continue;                 // of execute, which is in the kernel
        long pushes = 0;
        for(long back = idx - 1; back >= 0 && pushes < 2; back--) {
            uint16_t b = code[at + back];
            if (is_add(b)) { pushes++; continue; }
            if (is_push(b)) pushes++;
            break;
        }
        if (!pushes) continue;
        (*calls)++;
        *more += pushes;
    }
}

static int by_more(const void* a, const void* b) {
    const line* x = a;
    const line* y = b;
    long dx = x->here - x->there, dy = y->here - y->there;
    if (dx != dy) return(dx < dy ? 1 : -1);
    return(strcmp(x->name, y->name));
}

static uint8_t* whole_of(const char* path, size_t* len) {
    uint8_t* bytes;
    *len = file_bytes(path, &bytes);
    if (!*len) {
        fprintf(stderr, "booted: %s has nothing in it\n", path);
        exit(EXIT_FAILURE);
    }
    return(bytes);
}

int main(int argc, char *argv[]) {
    if (argc != 7) {
        fprintf(stderr, "usage: booted BOOT.hex BOOT.tables MEMORY DISK.img WHOLE.hex WHOLE.tables\n");
        return(1);
    }
    uint8_t* image;
    size_t len = load_image(argv[1], &image);
    size_t memory_len, disk_len;
    uint8_t* memory = whole_of(argv[3], &memory_len);
    uint8_t* disk = whole_of(argv[4], &disk_len);
    uint8_t* whole;
    size_t whole_len = load_image(argv[5], &whole);
    if (!len || !whole_len || memory_len < len) {
        fprintf(stderr, "booted: %s and %s are to be images, and %s the memory of a machine of the first\n",
                argv[1], argv[5], argv[3]);
        return(1);
    }

    // The machine's words: its dictionary, the image's labels, and the
    // names it kept of what it compiled.
    word* of_image;
    int in_image = words_of(image, len, argv[2], &of_image);
    word* kept = NULL;
    long kept_room = 0, nkept = kept_of(memory, len, of_image, in_image, disk, disk_len, &kept, 0, &kept_room);
    word* here;
    int nhere = words_and(memory, len, argv[2], kept, nkept, &here);
    word* there;
    int nthere = words_of(whole, whole_len, argv[6], &there);
    long execute_here = place_of(here, nhere, "execute"), execute_there = place_of(there, nthere, "execute");

    const uint16_t* booted_with = (const uint16_t*)image;
    const uint16_t* code = (const uint16_t*)memory;
    const uint16_t* laid = (const uint16_t*)whole;
    long compiled = 0, instructions = 0;
    for(int w=0; w < nhere; w++) {
        long at = here[w].at;
        if (!here[w].size || booted_with[at]) continue;         // a second name, or a word of the image's
        line* l = line_named(here[w].name);
        l->here += here[w].size;
        for(long k=0; k < nkept; k++) if (kept[k].at == at && kept[k].size) l->immediate = true;
        if (pushes_only(code, at, here[w].size)) l->number = true;
        int next = w + 1;
        while (next < nhere && !here[next].size) next++;
        if (next == nhere || here[next].at / PAGE != at / PAGE) l->last = true;
        far_calls(code, at, here[w].size, execute_here, &l->far_here, &l->more_here);
        compiled++, instructions += here[w].size;
    }
    for(int w=0; w < nthere; w++) {
        if (!there[w].size) continue;
        line* l = NULL;
        for(long n=0; n < lines && !l; n++) if (!strcmp(LINES[n].name, there[w].name)) l = &LINES[n];
        if (!l) continue;                                       // a word the machine did not compile
        l->there += there[w].size;
        far_calls(laid, there[w].at, there[w].size, execute_there, &l->far_there, &l->more_there);
    }

    long both = 0, both_here = 0, both_there = 0, far_here = 0, far_there = 0, more_here = 0, more_there = 0;
    long macros = 0, macro_code = 0, numbers = 0, number_code = 0, rest = 0, rest_code = 0;
    for(long n=0; n < lines; n++) {
        const line* l = &LINES[n];
        if (l->there) {
            both++, both_here += l->here, both_there += l->there;
            far_here += l->far_here, far_there += l->far_there, more_here += l->more_here, more_there += l->more_there;
        } else if (l->immediate) {
            macros++, macro_code += l->here;
        } else if (l->number) {
            numbers++, number_code += l->here;
        } else {
            rest++, rest_code += l->here;
        }
    }
    if (!both) {
        fprintf(stderr, "booted: no word the machine compiled is one %s has: nothing was compared\n", argv[5]);
        return(1);
    }

    printf("%s, booted from %s: %ld words compiled, of %ld names it kept and its dictionary; %ld instructions, %ld bytes\n\n",
           argv[3], argv[4], compiled, nkept, instructions, 2 * instructions);
    printf("%-52s %6s %10s %10s %8s\n", "", "names", "machine", "cross.fs", "more");
    printf("%-52s %6ld %10ld %10ld %+8ld\n", "words cross.fs lays too", both, both_here, both_there, both_here - both_there);
    printf("%-52s %6s %10ld %10ld %+8ld\n", "  their calls to another page", "", far_here, far_there, far_here - far_there);
    printf("%-52s %6s %10ld %10ld %+8ld\n", "  and what those are more than calls of one", "", more_here, more_there,
           more_here - more_there);
    printf("%-52s %6ld %10ld %10s %+8ld\n", "a :: word's body", macros, macro_code, "", macro_code);
    printf("%-52s %6ld %10ld %10s %+8ld\n", "a word that only pushes a number", numbers, number_code, "", number_code);
    printf("%-52s %6ld %10ld %10s %+8ld\n", "the rest that cross.fs lays no word of", rest, rest_code, "", rest_code);
    printf("%-52s %6ld %10ld %10ld %+8ld\n\n", "all of it", lines, instructions, both_there, instructions - both_there);

    qsort(LINES, lines, sizeof(line), by_more);
    printf("the words both have, by how many instructions more the machine's is (* the last of its page)\n");
    printf("%-28s %8s %8s %6s   %s\n", "word", "machine", "cross.fs", "more", "calls to another page: machine, cross.fs");
    for(long n=0; n < lines; n++) {
        const line* l = &LINES[n];
        if (!l->there || l->here == l->there) continue;
        printf("%-28s %8ld %8ld %+6ld   %ld, %ld%s\n", l->name, l->here, l->there, l->here - l->there,
               l->far_here, l->far_there, l->last ? " *" : "");
    }
    printf("\nthe words cross.fs lays none of, by how long (: a :: word, # one that only pushes, * the last of its page)\n");
    for(long n=0; n < lines; n++) {
        const line* l = &LINES[n];
        if (l->there) continue;
        printf("%-28s %8ld %s%s%s\n", l->name, l->here, l->immediate ? ":" : "", l->number ? "#" : "", l->last ? " *" : "");
    }
    return(0);
}
