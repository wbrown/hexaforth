//
// fused IMAGE.hex IMAGE.tables forth/fused.fs
//
// The table see.fs shows an instruction as words by (forth/fused.fs),
// made from a built image.  It is in the image, so an image is built with
// the table there was, the table is made of that image, and the image is
// built again, until the table stops changing.
//
// What is shown is meant to be read, so it is the plain words a programmer
// would write and not the name of whatever the instruction happens to be.
// For every ALU instruction in the image's code, and every one that a
// word of vm_opcodes.h in the dictionary is, the first of these that
// there is:
//
//   the standard word that is the instruction (dup, +, @)
//   two standard words (2dup xor, over +, dup @), which read better than
//     a name made of them (2dupxor, over+, dup@)
//   the machine's own name for it, if that is in the dictionary (or!)
//   three standard words, then two or three of any in the dictionary,
//     then four
//
// A word is standard if gforth has it, and gforth is asked.  A word of the
// dictionary that is not in vm_opcodes.h is never used, though it be one
// instruction.  The table names each word by its header, so a word must
// have one.  A return folded into an instruction is taken off first, as
// see.fs does.
//
// Whether some words are an instruction is asked of the machine: the step
// on terms says what each leaves (fuse.c), and they are the instruction
// if they leave the same.  Nothing here says what an instruction does.
// The names of an instruction's fields, which the table ends with, are
// read from vm_opcodes.h.
//

#include "fuse.c"
#include "image.h"
#include "words.c"

#define MOST 4                          // the most words an instruction is shown as: chosen, and not the machine's

typedef struct {
    const char* name;
    uint16_t code[COUNT(((word_node*)0)->ins)];     // as long as a word of vm_opcodes.h can be
    int len;
    int standard;                       // gforth has it
} vocable;
static vocable TYPED[COUNT(FORTH_WORDS)];       // the words of vm_opcodes.h that have a header, and that the step
static int typed;                               // says of: no more than it has words

typedef struct {
    uint16_t ins;
    uint64_t leaves;                    // the number for what it leaves, or nothing if the step does not say
    long uses;
    const char* own;                    // the machine's name for it, if the dictionary has that
    int said[MOST], said_n;             // the words it is shown as: places in TYPED
    long rank;                          // of those words: fewer, then fewer instructions, then shorter
    const char* shown;                  // or the one name it is shown as
} wanted;
static wanted* WANT;                    // as many as the image has, with room for more as they are found
static long wants, wants_room;
static int want_at[EVERY_WORD];

static int has_header(const word* words, int n, const char* name) {
    return(place_of(words, n, name) >= 0);
}

// What is wanted of an instruction, which is there from when it is first
// asked for.  (It is where it is only until the next is asked for.)
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

// How often some instructions fetch from memory, each taken by itself: the
// step says how often one does.
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

// Are some words an instruction?  They leave what it leaves; and they
// fetch as often as it does, since a cell stored and then fetched leaves
// what the store alone does, and `tuck ! @` is not how a store that keeps
// what it stored is to be read.
static int is_the_instruction(const uint16_t* code, int len, uint16_t ins) {
    named a, b;
    forget();
    uint64_t left = names(code, len, &a);
    return(left && names(&ins, 1, &b) == left && same_named(&a, &b) && fetching(code, len) == fetching(&ins, 1));
}

// Which of some words gforth has: it is asked, a line a word.
static void ask_gforth(const char* beside) {
    static const char ask[] = ".ask", run[] = GFORTH " ";       // GFORTH is where gforth is, which the build says
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

// Every phrase of so many of the words, standard ones only or any: where
// it is an instruction that is wanted and is better than what that has, it
// is what the instruction is shown as.  They are tried in the order of
// vm_opcodes.h, and of two as good the first stays.
static void phrases(int count, int standard_only, int at, int* phrase, uint16_t* code, int len, long letters) {
    if (at == count) {
        named m;
        forget();
        uint64_t leaves = names(code, len, &m);
        if (!leaves) return;
        long rank = ((long)count << 40) | ((long)len << 20) | letters;
        for(int w=0; w < wants; w++) {
            wanted* x = &WANT[w];
            if (x->shown || x->leaves != leaves || (x->said_n && x->rank <= rank)) continue;
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

// What an instruction is shown as, of what has been found for it so far.
static void settle(int standard) {
    for(int w=0; w < wants; w++) {
        wanted* x = &WANT[w];
        if (x->shown || !x->said_n) continue;
        static const char before[] = " hdr, ";          // what is before each name, but for its space the first time
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

// The four-word phrases: the phrases of one, two and three words that
// leave something no earlier phrase left, each with one word more.
#define LONGEST 48                      // instructions in a phrase: chosen
typedef struct { int phrase[MOST]; int n; uint16_t code[LONGEST]; int len; } reached;

static number_set REACHED;              // what has been reached, by the number for what it leaves (room.c)

static void four_words(void) {
    int* left = room_for(wants, sizeof(int));   // the instructions that have no words yet
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

static int by_uses(const void* a, const void* b) {
    const wanted* x = a;
    const wanted* y = b;
    if (x->uses != y->uses) return(x->uses < y->uses ? 1 : -1);
    return((int)x->ins - (int)y->ins);
}

// The names of a field's values, in the order of the values: they are the
// words of vm_opcodes.h, under the field's heading there, that are that
// field with a value and nothing else.  They go on over lines a block's
// line long, each under the first.
static void field_names(FILE* out, const char* table, const char* heading, int values, uint16_t (*with)(int)) {
    int len = fprintf(out, "create %s ", table);        // and a space before each name
    const int indent = len;                             // where the space before each line's first name is
    for(int value=0; value < values; value++) {
        const char* name = NULL;
        int under = 0;
        for(forth_op* op = INS_FIELDS; strlen(op->repr) && !name; op++) {
            if (op->type == COMMT) { under = !strcmp(op->repr, heading); continue; }
            if (under && *(uint16_t*)&op->ins == with(value)) name = op->repr;
        }
        if (!name) { fprintf(stderr, "fused: vm_opcodes.h has no name for %s %d\n", heading, value); exit(1); }
        if (value && len + (int)strlen(name) + 5 > DISK_LINE) len = fprintf(out, "\n%*s", indent, "") - 1;
        len += fprintf(out, " ,\" %s\"", name);
    }
    fprintf(out, "\n");
}
static uint16_t with_in(int v)  { instruction ins = {0}; ins.alu.in_mux = v;  return(*(uint16_t*)&ins); }
static uint16_t with_op(int v)  { instruction ins = {0}; ins.alu.alu_op = v;  return(*(uint16_t*)&ins); }
static uint16_t with_out(int v) { instruction ins = {0}; ins.alu.out_mux = v; return(*(uint16_t*)&ins); }

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "usage: fused IMAGE.hex IMAGE.tables forth/fused.fs\n");
        return(1);
    }
    get_ready();
    memset(want_at, -1, sizeof(want_at));
    uint8_t* image;
    size_t len = load_image(argv[1], &image);
    word* words;
    int n = words_of(image, len, argv[2], &words);
    without_tables(image, len, words, n);

    uint16_t noop = ins_named("noop"), leaves = ins_named("exit");
    instruction exit_ins = *(instruction*)&leaves;

    // The words that may be said: those of vm_opcodes.h that have a header
    // and that the step says of; and the machine's own name for an
    // instruction, which may be one the step says nothing of (io@).
    for(word_node* w = FORTH_WORDS; w->repr && strlen(w->repr); w++) {
        if (!w->num_ins || !has_header(words, n, w->repr)) continue;
        if (!strcmp(w->repr, "noop") || !strcmp(w->repr, "halt") || !strcmp(w->repr, "exit") || !strcmp(w->repr, "r@;")) continue;
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
    ask_gforth(argv[3]);

    // The instructions the image's code has.  Code is the pages that words
    // are in.
    const size_t instructions = len / sizeof(uint16_t);
    unsigned char* has_code = room_for(instructions / PAGE + 1, 1);
    for(int idx=0; idx < n; idx++) if ((size_t)words[idx].at < instructions) has_code[words[idx].at / PAGE] = 1;
    const uint16_t* code = (const uint16_t*)image;
    for(size_t at=0; at < instructions; at++) {
        if (!has_code[at / PAGE]) continue;
        instruction ins = *(const instruction*)&code[at];
        if (ins.lit.lit_f || ins.alu.op_type != OP_TYPE_ALU) continue;
        if (ins.alu.r_eip) {
            if (ins.alu.rstack != exit_ins.alu.rstack) continue;    // not a return that was folded in
            ins.alu.r_eip = 0;
            ins.alu.rstack = 0;
        }
        uint16_t x = *(uint16_t*)&ins;
        if (x == noop) continue;
        want(x)->uses++;
    }

    // One standard word, or two; then the machine's own name; then three
    // standard words; then two or three of any; then four.
    int phrase[MOST];
    static uint16_t seq[MOST * COUNT(((word_node*)0)->ins)];    // as many words, each as long as one can be
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

    qsort(WANT, wants, sizeof(wanted), by_uses);
    char* made = NULL;                  // the table, which is written to the file if it is not there already
    size_t made_len = 0;
    FILE* out = open_memstream(&made, &made_len);
    if (!out) { perror(argv[3]); return(1); }
    fprintf(out, "\\ fused.fs - the image's instructions as words, for see.fs\n"
                 "\\ GENERATED from the image by util/fused.c whenever the image is\n"
                 "\\ built (util/tables.cmake).  Do not edit.\n\\\n"
                 "\\ Each entry is cells: the instruction, how many words it is,\n"
                 "\\ and the address of each word's header.  The words are the\n"
                 "\\ plain ones a programmer would write: one standard word, or\n"
                 "\\ two, before the machine's own name for the instruction.  Those\n"
                 "\\ the image has most of are first, since the table is searched\n"
                 "\\ from the start.  A zero ends the table.  It is a table of\n"
                 "\\ cells, reached a cell at a time: high.\n\n"
                 "high\n"
                 "header fused create fused\n");
    // The table is source that goes on a disk: cut with a line of nothing,
    // between two of its entries, before it is longer than a block's lines
    // (doc/formatting.md).
    int run = 2;                        // its lines since a line of nothing: high and the header so far
    int shown = 0, one = 0, not_shown = 0;
    for(int w=0; w < wants; w++) {
        if (!WANT[w].shown) { not_shown++; continue; }
        if (run == DISK_BLOCK / DISK_LINE) {
            fprintf(out, "\n");
            run = 0;
        }
        fprintf(out, "$%04x , $%x , %s\n", WANT[w].ins, WANT[w].said_n, WANT[w].shown);
        run++;
        shown++;
        one += WANT[w].said_n == 1;
    }
    if (run + 2 > DISK_BLOCK / DISK_LINE) fprintf(out, "\n");
    fprintf(out, "0 ,\nunhigh\n");
    fprintf(out, "\n\\ The names of an instruction's fields, as vm_opcodes.h has\n"
                 "\\ them: where the ALU's input is from, the operation, and where\n"
                 "\\ the result goes.  Each is counted strings laid end to end:\n"
                 "\\ text, so bulk.\nbulk\n");
    field_names(out, "f-in", "input_mux", INPUTS, with_in);
    field_names(out, "f-op", "alu_op", OPS, with_op);
    field_names(out, "f-out", "output_mux", OUTPUTS, with_out);
    fprintf(out, "unbulk\n");
    fclose(out);
    if (!write_if_new(argv[3], made, made_len)) { perror(argv[3]); return(1); }
    free(made);
    printf("%d instructions as one word and %d as more written to %s; %d needed four words; %d have no words\n",
           one, shown - one, argv[3], needed_four, not_shown);
    return(shown ? 0 : 1);
}
