//
// booted BOOT.hex BOOT.tables MEMORY DISK.img WHOLE.hex WHOLE.tables
//
// Compares what a machine compiled when it booted with the same words as
// cross.fs compiles them.
//
// BOOT.hex is the boot image (BOOT=on).  MEMORY is the machine's memory
// after it booted from DISK.img, before anything was typed at its prompt
// (test/deskvm.c writes it to the file BOOTED names).  DISK.img is that
// disk as the machine left it, which has the private names of the words
// the machine compiled (forth/files.fs, util/words.h).  WHOLE.hex is the
// same system with the applications compiled by cross.fs.
//
// A word the machine compiled is one whose code is at addresses where the
// boot image has nothing.  It is matched with cross.fs's word of the same
// name, and all the words with one name are reported on one line.  A word
// of cross.fs's that the boot image also has, with the same name at the
// same address, is not an application's word, so it is not matched with a
// word of that name that the machine compiled.  The machine also compiles
// code that cross.fs keeps no word for.  That code is totalled by kind:
//
//   a :: word       the body of a macro.  cross.fs runs it while
//                   compiling, and the machine runs it while reading the
//                   file.  It is immediate
//   a number        a word that only pushes a number and has no header.
//                   cross.fs keeps no word for it
//   the rest        a name for which cross.fs has no word
//
// Some of what the machine compiles is not reported at all.  A word a file
// defines between meta and target, and a private word that only pushes a
// number (which join.fs inlines), are placed in the machine's scratch and
// zeroed when the file has been read (placement.fs, files.fs).  The part
// of a definition that was compiled on a page it then ran off is zeroed
// too (blocks.fs).
//
// A call to a word on another page is the word's address pushed and a
// call of execute.  For the words both builds have, the report gives how
// many such calls each build has, and how many more instructions they
// take than one-instruction calls.
//
// The decompiler shows an ALU instruction as the words its table has for
// it (forth/fused.fs, which is in the machine's memory), or as a number if
// the table has no entry.  The report also lists the instructions the
// machine compiled that have no entry, each with the first word it is in.
//
// It fails if no word is in both builds, because then nothing was
// compared.  It also fails if none of cross.fs's words is in the boot
// image, because then the two images are not the same system.
//

#include <stdbool.h>
#include "../util/image.h"
#include "../util/words.h"
#include "../util/named.h"

typedef struct {
    char name[NAME_ROOM];
    long here, there;                   // the instructions the machine compiled for it, and the instructions cross.fs did
    long far_here, far_there;           // the calls to another page in each build
    long more_here, more_there;         // how many more instructions those take than one-instruction calls
    bool immediate, number;             // for the machine's word: it is a :: word; it only pushes a number
} line;

static line* LINES;
static long lines, lines_room;

// Returns the line for a name, creating it if there is none.
static line* line_named(const char* name) {
    for(long n=0; n < lines; n++) if (!strcmp(LINES[n].name, name)) return(&LINES[n]);
    ONE_MORE(LINES, lines, lines_room);
    line* is = &LINES[lines++];
    memset(is, 0, sizeof(line));
    snprintf(is->name, sizeof(is->name), "%s", name);
    return(is);
}

// is_push() is true for a literal that pushes its number, and is_add() for
// one that adds it.  Neither is true for a literal that fetches or stores
// a variable.
static bool is_literal(uint16_t w) { return(w & 0x8000); }
static bool is_push(uint16_t w) { return((w & 0xc000) == 0x8000 && (w & 0x3000) != 0x3000); }
static bool is_add(uint16_t w) { return((w & 0xc000) == 0xc000 && (w & 0x3000) != 0x3000); }

// Returns true if the code only pushes a number and returns.
static bool pushes_only(const uint16_t* code, long at, long size) {
    if (size < 2 || size > 9 || !is_push(code[at])) return(false);
    for(long idx=1; idx < size - 1; idx++) {
        if (!is_literal(code[at + idx]) && code[at + idx] != 0x60dc) return(false);     // a wide number also has a shift instruction
    }
    return(code[at + size - 1] == 0x7403);
}

// Counts the calls to another page in a word.  Such a call is a call of
// execute, or a jump to it, after the instructions that push an address.
// It adds the number of such calls to `*calls`, and the number of pushing
// instructions to `*more`: a one-instruction call would not need those.
static void far_calls(const uint16_t* code, long at, long size, long execute, long* calls, long* more) {
    for(long idx=1; idx < size; idx++) {
        uint16_t w = code[at + idx];
        bool goes = (w & 0xe000) == 0x4000 || (w & 0xe000) == 0x0000;   // a call, or a jump
        if (!goes || (w & 0x1fff) != execute) continue;                 // only one that goes to execute, which is in the kernel's page
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

// Returns true if some word of that name has code of its own.
static bool has_code(const word* words, int n, const char* name) {
    for(int idx=0; idx < n; idx++) if (words[idx].size && !strcmp(words[idx].name, name)) return(true);
    return(false);
}

static int by_more(const void* a, const void* b) {
    const line* x = a;
    const line* y = b;
    long dx = x->here - x->there, dy = y->here - y->there;
    if (dx != dy) return(dx < dy ? 1 : -1);
    return(strcmp(x->name, y->name));
}

// Reads a whole file, and exits if it is empty or cannot be read.
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
    init_opcodes(FORTH_WORDS);
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

    // The machine's words come from its dictionary, the image's labels,
    // and the names it saved for the words it compiled.
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
    // The machine's table `pages` (nuc.fs) records how far the code on
    // each page has got.
    uint64_t pages = data_called(argv[2], "pages");
    if (!pages) {
        fprintf(stderr, "booted: %s does not say where pages is, which has how far each page's code has got\n", argv[2]);
        return(1);
    }

    static long laid_times[1 << 16];                            // the number of times the machine compiled each ALU instruction
    static const char* laid_in[1 << 16];                        // the first word each of those is in
    long compiled = 0, instructions = 0;
    for(int w=0; w < nhere; w++) {
        long at = here[w].at, size = compiled_of(image, memory, pages, &here[w]);
        if (!size) continue;                                    // the machine did not compile this word
        // Of the word's names, use one that cross.fs has a word for.  A
        // definition can have a header with a different name (header
        // F-BOLD : code-bold), and cross.fs's word is under one of the two.
        const char* name = here[w].name;
        for(int k=w; k < nhere && here[k].at == at; k++) {
            if (has_code(there, nthere, here[k].name)) { name = here[k].name; break; }
        }
        line* l = line_named(name);
        l->here += size;
        for(long k=0; k < nkept; k++) if (kept[k].at == at && kept[k].size) l->immediate = true;
        if (pushes_only(code, at, size)) l->number = true;
        far_calls(code, at, size, execute_here, &l->far_here, &l->more_here);
        compiled++, instructions += size;
        for(long k=0; k < size; k++) {
            uint16_t ins = as_tabled(code[at + k]);
            if (ins && !laid_times[ins]++) laid_in[ins] = name;
        }
    }
    long of_both = 0;                                           // the number of cross.fs's words that the boot image also has
    for(int w=0; w < nthere; w++) {
        if (!there[w].size) continue;
        bool booted_with_it = false;
        for(int k=0; k < in_image && of_image[k].at <= there[w].at && !booted_with_it; k++) {
            booted_with_it = of_image[k].at == there[w].at && booted_with[there[w].at]
                             && !strcmp(of_image[k].name, there[w].name);
        }
        if (booted_with_it) { of_both++; continue; }
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
    if (!of_both) {
        fprintf(stderr, "booted: no word of %s is where %s has it: they are not one system\n", argv[5], argv[1]);
        return(1);
    }

    printf("%s, booted from %s: %ld words compiled, of %ld names it kept and its dictionary; %ld instructions, %ld bytes\n",
           argv[3], argv[4], compiled, nkept, instructions, 2 * instructions);
    printf("%s has %ld words that %s has too, which are not the machine's to compile\n\n", argv[5], of_both, argv[1]);
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
    printf("the words both have, by how many instructions more the machine's is\n");
    printf("%-28s %8s %8s %6s   %s\n", "word", "machine", "cross.fs", "more", "calls to another page: machine, cross.fs");
    for(long n=0; n < lines; n++) {
        const line* l = &LINES[n];
        if (!l->there || l->here == l->there) continue;
        printf("%-28s %8ld %8ld %+6ld   %ld, %ld\n", l->name, l->here, l->there, l->here - l->there,
               l->far_here, l->far_there);
    }
    printf("\nthe words cross.fs lays none of, by how long (: a :: word, # one that only pushes)\n");
    for(long n=0; n < lines; n++) {
        const line* l = &LINES[n];
        if (l->there) continue;
        printf("%-28s %8ld %s%s\n", l->name, l->here, l->immediate ? ":" : "", l->number ? "#" : "");
    }

    // List the compiled instructions that are missing from the
    // decompiler's table.
    static unsigned char has[1 << 16];
    fused_entry* table;
    long entries = fused_of(memory, len, here, nhere, &table);
    for(long e=0; e < entries; e++) has[table[e].ins] = 1;
    long different = 0, without = 0;
    for(long ins=0; ins < 1 << 16; ins++) different += laid_times[ins] != 0;
    printf("\nthe machine compiled %ld different ALU instructions; these are missing from the decompiler's table of %ld entries\n",
           different, entries);
    for(long ins=0; ins < 1 << 16; ins++) {
        if (!laid_times[ins] || has[ins]) continue;
        without++;
        printf("$%04lx %8ld times, first in %s\n", ins, laid_times[ins], laid_in[ins]);
    }
    printf("%ld in all\n", without);
    if (!entries) {
        fprintf(stderr, "booted: %s has no decompiler table, so no instruction was looked up in it\n", argv[3]);
        return(1);
    }
    return(0);
}
