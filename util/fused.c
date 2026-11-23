//
// fused IMAGE.hex IMAGE.tables forth/fused.fs [--made TYPED]
//       [--booted BOOT.hex BOOT.tables MEMORY DISK.img] [--same]
// fused TABLE.fs
//
// Makes the table see.fs uses to show an instruction as words
// (forth/fused.fs), from a built image.  The table is part of the image,
// so it cannot be made from the image that contains it.  An image is first
// built with an empty table, which `fused TABLE.fs` writes, and the table
// is made from that image (cmake/default-machine.cmake).  The table is the
// last data in an image (forth/system.fs), so its length moves nothing
// else.  A table placed before a variable could push the variable out of
// the cells one instruction reaches, and the code that fetches it would
// then be different instructions.  So the image that is built with the
// table has the same instructions as the first.  The tests tables-64 and
// the like check that: with --same, fused writes nothing, and fails if the
// table it makes from the image differs from the file, listing the entries
// the file lacks.
//
// The nucleus's own compiler makes some instructions that cross.fs does
// not make from the same source.  The table covers those too, from two
// inputs:
//
//   --made TYPED   the instructions the nucleus compiled when an image's
//                  code was typed at it, a run at a time (bench/typed.c,
//                  `typed read`)
//   --booted ...   the instructions a machine compiled when it booted and
//                  compiled its applications from its disk.  BOOT.hex is
//                  the image it booted with (BOOT=on), MEMORY is its memory
//                  after the boot, and DISK.img is its disk after the boot,
//                  which has the names it kept for what it compiled
//                  (words.h)
//
// The decompiler's output is meant to be read, so the table gives the
// plain words a programmer would write, not the machine's name for
// whatever the instruction happens to be.  For every ALU instruction in
// the image's code, every one from --made and --booted, and every one that
// is a word of vm_opcodes.h in the dictionary, the table has the first of
// these that exists:
//
//   the standard word that is the instruction (dup, +, @)
//   two standard words (2dup xor, over +, dup @)
//   the machine's own name for it, if that is in the dictionary (or!)
//   three standard words, then two or three of any in the dictionary,
//     then four
//
// A word is standard if gforth has it, and this program runs gforth to
// find out.  A dictionary word that is not in vm_opcodes.h is never used,
// even if it is a single instruction.  The table refers to each word by
// its header, so only words that have a header are used.  A return folded
// into an instruction is removed before the instruction is looked up, as
// see.fs does.
//
// The machine decides whether a phrase is equivalent to an instruction:
// the step on terms gives the final state of each (fuse.h), and the two
// are equivalent if the states are the same.  This file does not encode
// what any instruction does.
//

#include "fuse.h"
#include "image.h"
#include "words.h"
#include "typed.h"

#define MOST 4                         // the most words an instruction is shown as; a choice made here, not a property of the machine

// The format of one table entry in the generated file: the instruction
// and its number of words, each compiled as a sixteen-bit field (`w,`),
// and then the words.  Each word is compiled as the address of its header,
// also in sixteen bits (`hdr,`, which precedes each name in the text that
// replaces %s).
#define ENTRY "$%04x w, $%x w, %s\n"

typedef struct {
    const char* name;
    uint16_t code[COUNT(((word_node*)0)->ins)];     // room for the longest word vm_opcodes.h can have
    int len;
    int standard;                       // 1 if gforth has the word
} vocable;
static vocable TYPED[COUNT(FORTH_WORDS)];       // the words of vm_opcodes.h that have a header and that the step
static int typed;                               // models; there are at most as many as vm_opcodes.h has words

typedef struct {
    uint16_t ins;
    uint64_t leaves;                    // the hash of its final state, or 0 if the step does not model it
    long uses;                          // how many times the image's code has it
    long at;                            // the address where the image first has it
    const char* in;                     // the word that address is in
    long made;                          // how many times the nucleus compiled it from typed code
    const char* made_in;                // the first run it was in
    long laid;                          // how many times a booted machine compiled it
    const char* laid_in;                // the first word it was in
    const char* own;                    // the machine's name for it, if the dictionary has that name
    int said[MOST], said_n;             // the words it is shown as, as indexes into TYPED, and how many there are
    long rank;                          // the rank of those words: fewer words, then fewer instructions, then its own operation, then fewer letters
    const char* shown;                  // the words as text for its table entry, or NULL until they are settled
} wanted;
static wanted* WANT;                    // one for each instruction to show; the list grows as instructions are found
static long wants, wants_room;
static int want_at[EVERY_WORD];

static int has_header(const word* words, int n, const char* name) {
    return(place_of(words, n, name) >= 0);
}

// Returns the record for an instruction, creating it the first time the
// instruction is requested.  The pointer is valid only until the next
// call, which may move the list.
static wanted* want(uint16_t ins) {
    if (want_at[ins] < 0) {
        ONE_MORE(WANT, wants, wants_room);
        want_at[ins] = wants;
        memset(&WANT[wants], 0, sizeof(wanted));
        WANT[wants].ins = ins;
        named m;
        forget();
        WANT[wants].leaves = names(&ins, 1, &m);
        wants++;
    }
    return(&WANT[want_at[ins]]);
}

// Returns the number of memory fetches in a run, counting each
// instruction on its own.  The step reports how many fetches an
// instruction makes.
static int fetching(const uint16_t* code, int len) {
    int n = 0;
    for(int idx=0; idx < len; idx++) {
        named m;
        forget();
        names(code + idx, 1, &m);
        n += m.fetches;
    }
    return(n);
}

// Returns 1 if a phrase can stand for an instruction.  The phrase must end
// in the same final state as the instruction, and must fetch from memory
// the same number of times.  The second condition is needed because a
// cell stored and then fetched ends in the same state as the store alone.
// Without it, `tuck ! @` would be accepted for an instruction that stores
// a value and keeps it, which is a misleading way to show that
// instruction.
static int is_the_instruction(const uint16_t* code, int len, uint16_t ins) {
    named a, b;
    forget();
    uint64_t left = names(code, len, &a);
    return(left && names(&ins, 1, &b) == left && same_named(&a, &b) && fetching(code, len) == fetching(&ins, 1));
}

// Runs gforth to find which of the TYPED words it has, and sets `standard`
// for each.  The script it writes looks up one word per line.
static void ask_gforth(const char* beside) {
    static const char ask[] = ".ask", run[] = GFORTH " ";       // GFORTH is the path to gforth, which the build defines
    char* path = room_for(strlen(beside) + sizeof(ask), 1);
    char* command = room_for(strlen(beside) + sizeof(ask) + sizeof(run), 1);
    sprintf(path, "%s%s", beside, ask);
    FILE* script = fopen(path, "w");
    if (!script) { perror(path); exit(1); }
    fprintf(script, ": t parse-name find-name 0<> . cr ;\n");
    for(int idx=0; idx < typed; idx++) fprintf(script, "t %s\n", TYPED[idx].name);
    fprintf(script, "bye\n");
    fclose(script);
    sprintf(command, "%s%s", run, path);
    FILE* answers = popen(command, "r");
    int answered = 0, has;
    while (answers && answered < typed && fscanf(answers, "%d", &has) == 1) TYPED[answered++].standard = has != 0;
    if (!answers || pclose(answers) || answered != typed) {
        fprintf(stderr, "fused: gforth did not say which of the %d words it has\n", typed);
        exit(1);
    }
    remove(path);
}

// Returns 1 if any instruction of a phrase uses the same ALU operation as
// `ins`.  An operation that only passes its input through belongs to no
// word in particular, so every phrase counts as having it.
static int does_as(const uint16_t* code, int len, uint16_t ins) {
    const instruction is = *(const instruction*)&ins;
    if (is.alu.alu_op == ALU_IN || is.alu.alu_op == ALU_SWAP_IN) return(1);
    for(int idx=0; idx < len; idx++) {
        const instruction has = *(const instruction*)&code[idx];
        if (!has.lit.lit_f && has.alu.op_type == OP_TYPE_ALU && has.alu.alu_op == is.alu.alu_op) return(1);
    }
    return(0);
}

// Tries every phrase of `count` words, drawn from the standard words only
// or from all of them.  Where a phrase is equivalent to an instruction
// that needs words, and ranks better than the phrase that instruction
// already has, it becomes the instruction's phrase.  Phrases are tried in
// the order of vm_opcodes.h, and of two that rank the same the first is
// kept.  Between phrases of the same length, one that uses the
// instruction's own operation ranks better: the instruction that zeroes
// the top by xoring a cell with itself is shown as `dup xor`, not as
// `dup <`, which leaves the same result.
static void phrases(int count, int standard_only, int at, int* phrase, uint16_t* code, int len, long letters) {
    if (at == count) {
        named m;
        forget();
        uint64_t leaves = names(code, len, &m);
        if (!leaves) return;
        for(int w=0; w < wants; w++) {
            wanted* x = &WANT[w];
            if (x->shown || x->leaves != leaves) continue;
            long rank = ((long)count << 40) | ((long)len << 20) | ((long)!does_as(code, len, x->ins) << 19) | letters;
            if (x->said_n && x->rank <= rank) continue;
            if (!is_the_instruction(code, len, x->ins)) continue;
            memcpy(x->said, phrase, count * sizeof(int));
            x->said_n = count;
            x->rank = rank;
        }
        return;
    }
    for(int idx=0; idx < typed; idx++) {
        if (standard_only && !TYPED[idx].standard) continue;
        phrase[at] = idx;
        memcpy(code + len, TYPED[idx].code, TYPED[idx].len * sizeof(uint16_t));
        phrases(count, standard_only, at + 1, phrase, code, len + TYPED[idx].len, letters + strlen(TYPED[idx].name));
    }
}

// Settles the words of every instruction that has a phrase by now: builds
// the text of the words for its table entry.  An instruction that is
// settled keeps its words, whatever is tried later.
static void settle(int standard) {
    for(int w=0; w < wants; w++) {
        wanted* x = &WANT[w];
        if (x->shown || !x->said_n) continue;
        static const char before[] = " hdr, ";          // the text before each name; the first name omits the leading space
        size_t room = 1;
        for(int idx=0; idx < x->said_n; idx++) room += sizeof(before) + strlen(TYPED[x->said[idx]].name);
        char* text = room_for(room, 1);
        for(int idx=0; idx < x->said_n; idx++) {
            strcat(text, idx ? before : before + 1);
            strcat(text, TYPED[x->said[idx]].name);
        }
        x->shown = text;
        (void)standard;
    }
}

// Searches the four-word phrases for the instructions that still have no
// words.  The search is breadth first: each phrase of one, two or three
// words that reached a final state no earlier phrase reached is extended
// by one more word.
#define LONGEST 48                      // the most instructions in a phrase; a choice made here
typedef struct { int phrase[MOST]; int n; uint16_t code[LONGEST]; int len; } reached;

static number_set REACHED;              // the final states reached so far, by hash (room.h)

static void four_words(void) {
    int* left = room_for(wants, sizeof(int));   // the instructions that still have no words
    int lefts = 0;
    for(int w=0; w < wants; w++) if (!WANT[w].shown && WANT[w].leaves) left[lefts++] = w;
    reached* frontier = calloc(1, sizeof(reached));
    long fronts = 1;
    for(int depth=1; depth <= MOST; depth++) {
        reached* next = depth < MOST ? malloc(sizeof(reached) * (fronts * typed + 1)) : NULL;
        long nexts = 0;
        for(long f=0; f < fronts; f++) {
            for(int idx=0; idx < typed; idx++) {
                reached r = frontier[f];
                if (r.len + TYPED[idx].len > LONGEST) continue;
                r.phrase[r.n++] = idx;
                memcpy(r.code + r.len, TYPED[idx].code, TYPED[idx].len * sizeof(uint16_t));
                r.len += TYPED[idx].len;
                named m;
                forget();
                uint64_t leaves = names(r.code, r.len, &m);
                if (!leaves) continue;
                if (depth == MOST) {
                    for(int l=0; l < lefts; l++) {
                        wanted* x = &WANT[left[l]];
                        if (x->said_n || x->leaves != leaves || !is_the_instruction(r.code, r.len, x->ins)) continue;
                        memcpy(x->said, r.phrase, sizeof(r.phrase));
                        x->said_n = MOST;
                    }
                    continue;
                }
                if (!already_in(&REACHED, leaves)) next[nexts++] = r;
            }
        }
        free(frontier);
        frontier = next;
        fronts = nexts;
    }
    free(left);
}

// Prints where an instruction comes from: the image, the nucleus compiling
// typed code, or a boot; or, if no code has it, that it is a word of the
// dictionary.
static void say_from(const wanted* x) {
    if (x->uses) {
        printf("the image has it %ld times, the first at $%lx in %s", x->uses, x->at, x->in ? x->in : "no word");
    } else if (x->made) {
        printf("the nucleus compiled it %ld times from typed code, first in %s", x->made, x->made_in);
    } else if (x->laid) {
        printf("a boot compiled it %ld times, first in %s", x->laid, x->laid_in);
    } else {
        printf("no code has it; it is a word of the dictionary");
    }
}

static int by_uses(const void* a, const void* b) {
    const wanted* x = a;
    const wanted* y = b;
    if (x->uses != y->uses) return(x->uses < y->uses ? 1 : -1);
    return((int)x->ins - (int)y->ins);
}

// Counts the instructions a machine compiled when it booted (words.h,
// compiled_of).  `booted` is four files: the image it booted with, that
// image's names, its memory after the boot, and its disk after the boot.
// Exits if the machine compiled nothing.
static void laid_at_boot(const char* booted[4]) {
    uint8_t* image;
    size_t len = load_image(booted[0], &image);
    uint8_t* memory;
    uint8_t* disk;
    size_t memory_len = file_bytes(booted[2], &memory), disk_len = file_bytes(booted[3], &disk);
    uint64_t pages = data_called(booted[1], "pages");
    if (!len || memory_len < len || !disk_len || !pages) {
        fprintf(stderr, "fused: %s is to be an image, %s its names, %s the memory of a machine that booted with it, and %s its disk\n",
                booted[0], booted[1], booted[2], booted[3]);
        exit(1);
    }
    word* of_image;
    int in_image = words_of(image, len, booted[1], &of_image);
    word* kept = NULL;
    long kept_room = 0, nkept = kept_of(memory, len, of_image, in_image, disk, disk_len, &kept, 0, &kept_room);
    word* here;
    int nhere = words_and(memory, len, booted[1], kept, nkept, &here);
    const uint16_t* code = (const uint16_t*)memory;
    long compiled = 0;
    for(int w=0; w < nhere; w++) {
        long size = compiled_of(image, memory, pages, &here[w]);
        compiled += size;
        for(long k=0; k < size; k++) {
            uint16_t x = as_tabled(code[here[w].at + k]);
            if (x && !want(x)->laid++) want(x)->laid_in = here[w].name;
        }
    }
    if (!compiled) {
        fprintf(stderr, "fused: the machine of %s compiled nothing at its boot\n", booted[2]);
        exit(1);
    }
}

// Counts the instructions the nucleus compiled from typed code, from the
// output of `typed read`.  A run is typed without its final exit, so none
// of these instructions has a return in it.  Exits if there are none.
static void made_as_typed(const char* typed_path) {
    static made_typed the;
    if (!made_of_typed(typed_path, &the)) {
        fprintf(stderr, "fused: %s has no instruction the nucleus made\n", typed_path);
        exit(1);
    }
    for(long ins=0; ins < EVERY_MADE; ins++) {
        uint16_t x = the.times[ins] ? as_tabled((uint16_t)ins) : 0;
        if (!x) continue;
        want(x)->made = the.times[ins];
        want(x)->made_in = the.in_run[ins];
    }
}

// Collects the instructions to show: those in the image, those from
// `typed_path` if it is given, and those from `booted` if it is given.
// Finds the words each is shown as, and returns how many needed four
// words.  `beside` names a file; the question for gforth is written next
// to it.
static int find_words(const char* image_path, const char* tables_path, const char* beside, const char* typed_path,
                      const char** booted) {
    uint8_t* image;
    size_t len = load_image(image_path, &image);
    word* words;
    int n = words_of(image, len, tables_path, &words);

    // List the words a phrase can contain: the words of vm_opcodes.h that
    // have a header and that the step models.  Also record the machine's
    // own name for each instruction, which can be an instruction the step
    // does not model (io@).  A phrase in vm_opcodes.h is not a word, and
    // has no header.
    for(word_node* w = FORTH_WORDS; w->repr && strlen(w->repr); w++) {
        if (!w->num_ins || !has_header(words, n, w->repr)) continue;
        if (!strcmp(w->repr, "noop") || !strcmp(w->repr, "halt") || !strcmp(w->repr, "exit")) continue;
        uint16_t first = *(uint16_t*)&w->ins[0];
        instruction ins = w->ins[0];
        if (w->num_ins == 1 && !ins.lit.lit_f && ins.alu.op_type == OP_TYPE_ALU && !ins.alu.r_eip && !want(first)->own) {
            want(first)->own = w->repr;
        }
        named m;
        forget();
        if (!names((const uint16_t*)w->ins, w->num_ins, &m)) continue;
        TYPED[typed].name = w->repr;
        TYPED[typed].len = w->num_ins;
        memcpy(TYPED[typed].code, w->ins, w->num_ins * sizeof(uint16_t));
        typed++;
    }
    ask_gforth(beside);

    // Collect the instructions in the image's code.  The code is every
    // page that has a word on it.
    const size_t instructions = len / sizeof(uint16_t);
    unsigned char* has_code = room_for(instructions / PAGE + 1, 1);
    for(int idx=0; idx < n; idx++) if ((size_t)words[idx].at < instructions) has_code[words[idx].at / PAGE] = 1;
    const uint16_t* code = (const uint16_t*)image;
    for(size_t at=0; at < instructions; at++) {
        if (!has_code[at / PAGE]) continue;
        uint16_t x = as_tabled(code[at]);
        if (!x) continue;
        if (!want(x)->uses++) want(x)->at = (long)at, want(x)->in = word_at(words, n, (long)at);
    }
    if (typed_path) made_as_typed(typed_path);
    if (booted) laid_at_boot(booted);

    // Find words in the order of preference: one standard word, or two;
    // then the machine's own name; then three standard words; then two or
    // three of any words; then four.
    int phrase[MOST];
    static uint16_t seq[MOST * COUNT(((word_node*)0)->ins)];    // room for MOST words of the greatest length
    for(int count=1; count <= 2; count++) phrases(count, 1, 0, phrase, seq, 0, 0);
    settle(1);
    for(int w=0; w < wants; w++) if (!WANT[w].shown && WANT[w].own) {
        static const char before[] = "hdr, ";
        char* own = room_for(sizeof(before) + strlen(WANT[w].own), 1);
        sprintf(own, "%s%s", before, WANT[w].own);
        WANT[w].shown = own;
        WANT[w].said_n = 1;
    }
    phrases(3, 1, 0, phrase, seq, 0, 0);
    settle(1);
    for(int count=2; count <= 3; count++) phrases(count, 0, 0, phrase, seq, 0, 0);
    settle(0);
    int needed_four = 0;
    for(int w=0; w < wants; w++) needed_four += !WANT[w].shown && WANT[w].leaves;
    if (needed_four) four_words();
    settle(0);
    return(needed_four);
}

int main(int argc, char *argv[]) {
    const char* said[3] = { NULL, NULL, NULL }; // the image, its names file and the table; or the table alone
    const char* typed_path = NULL;              // the output of `typed read`
    const char* booted_said[4];                 // a machine that booted: its image, its names, its memory, its disk
    const char** booted = NULL;
    int files = 0, same = 0, wrong = argc < 2;
    for(int a=1; a < argc; a++) {
        if (!strcmp(argv[a], "--same")) {
            same = 1;
        } else if (!strcmp(argv[a], "--made") && a + 1 < argc) {
            typed_path = argv[++a];
        } else if (!strcmp(argv[a], "--booted") && a + 4 < argc) {
            for(int k=0; k < 4; k++) booted_said[k] = argv[++a];
            booted = booted_said;
        } else if (files < 3) {
            said[files++] = argv[a];
        } else {
            wrong = 1;
        }
    }
    if (wrong || (files != 3 && files != 1) || (files == 1 && (same || typed_path || booted))) {
        fprintf(stderr, "usage: fused IMAGE.hex IMAGE.tables forth/fused.fs [--made TYPED]\n"
                        "             [--booted BOOT.hex BOOT.tables MEMORY DISK.img] [--same]\n"
                        "       fused TABLE.fs       the table of no image\n");
        return(1);
    }
    const int of_image = files == 3;
    const char* table = said[of_image ? 2 : 0];
    if (of_image) machine_from(said[0]);        // take the cell size from the image before the step is used
    get_ready();
    memset(want_at, -1, sizeof(want_at));
    int needed_four = of_image ? find_words(said[0], said[1], table, typed_path, booted) : 0;

    qsort(WANT, wants, sizeof(wanted), by_uses);
    char* made = NULL;                  // the generated table's text
    size_t made_len = 0;
    FILE* out = open_memstream(&made, &made_len);
    if (!out) { perror(table); return(1); }
    fprintf(out, "\\ fused.fs - instructions as words, for see.fs: those the image\n"
                 "\\ has, and those the nucleus makes of the same source.\n"
                 "\\ GENERATED by util/fused.c.\n"
                 "\\ An entry is the instruction, how many words it is, and each\n"
                 "\\ word's header: fields of sixteen bits, one after another in\n"
                 "\\ cells.  A field that is zero ends the table.\n\n"
                 "high\n"
                 "create fused\n");
    // The table is source that goes on a disk, so a blank line is inserted
    // between two entries before the table has more lines than a block
    // holds (doc/formatting.md).
    int run = 2;                        // lines since the last blank line: `high` and `create fused` so far
    int shown = 0, one = 0, not_shown = 0;
    for(int w=0; w < wants; w++) {
        if (!WANT[w].shown) { not_shown++; continue; }
        if (run == DISK_BLOCK / DISK_LINE) {
            fprintf(out, "\n");
            run = 0;
        }
        fprintf(out, ENTRY, WANT[w].ins, WANT[w].said_n, WANT[w].shown);
        run++;
        shown++;
        one += WANT[w].said_n == 1;
    }
    // The closing lines end the table with a zero field and align the data
    // pointer to a whole cell again.
    if (run + 4 > DISK_BLOCK / DISK_LINE) fprintf(out, "\n");
    fprintf(out, "0 w,\nmeta tcalign target\nunhigh\nmeta t' fused t' fused-at tvar! target\n");
    fclose(out);
    if (same) {
        // --same writes nothing.  It compares the table made from the image
        // with the file.
        unsigned char* had;
        size_t have = file_bytes(table, &had);
        int is = had && have == made_len && !memcmp(had, made, made_len);
        printf("the table of %s is %s%s\n", said[0], is ? "" : "not ", table);
        // Print each entry of the image's table that the file lacks, with
        // where its instruction comes from.
        for(int w=0; w < wants && !is; w++) {
            if (!WANT[w].shown) continue;
            char* line = NULL;
            text_more(&line, ENTRY, WANT[w].ins, WANT[w].said_n, WANT[w].shown);
            size_t len = strlen(line), at = 0;
            while (had && at + len <= have && memcmp(had + at, line, len)) at++;
            if (!had || at + len > have) {
                printf("  ");
                say_from(&WANT[w]);
                printf(": %s", line);
            }
            free(line);
        }
        return(is ? 0 : 1);
    }
    if (!write_whole(table, made, made_len)) { perror(table); return(1); }
    free(made);
    if (!of_image) {
        printf("the table of no image written to %s\n", table);
        return(0);
    }
    printf("%d instructions as one word and %d as more written to %s; %d needed four words; %d have no words\n",
           one, shown - one, table, needed_four, not_shown);
    // An instruction that no phrase of up to MOST words is equivalent to
    // has no entry, and see.fs shows it by its number.  List each one with
    // where it comes from.
    for(int w=0; w < wants; w++) {
        if (WANT[w].shown || (!WANT[w].uses && !WANT[w].made && !WANT[w].laid)) continue;
        printf("  no words for $%04x: ", WANT[w].ins);
        say_from(&WANT[w]);
        printf("\n");
    }
    return(shown ? 0 : 1);
}
