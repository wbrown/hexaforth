//
// costs IMAGE.hex IMAGE.tables IMAGE.sizes COUNTS OUT [FILE.fs ...]
//
// What a program's work costs, three ways, from how often each of its
// instructions ran.  COUNTS is what bench/profile.c leaves
// (desktop.counts): for each place, the work of what the desktop was put
// through.  Each report is a file in OUT, and its first lines are printed.
//
//   paths.txt   What each word costs, with all that it calls.  The counts
//               say what a word did itself, but not whose work it was: a
//               fill is a dozen words, none of which looks like much.  Here
//               each word is charged with the work of what it calls: a
//               call that ran c times is charged c times what the word
//               called costs each time it is entered.  So a word's figure
//               is what would be saved if it were never called.  What a
//               call costs is taken to be the same whoever makes it; and a
//               word reached by `execute` of what is not a number in the
//               code (a vector, a window's draw routine, a task) has a
//               caller that cannot be seen, and is listed at the end.
//               Read this before saying what a change will save.
//
//   loops.txt   Where the instructions go round.  A loop is the
//               instructions from where a branch goes back to, up to the
//               branch.  For every loop that ran: the word it is in, how
//               often it went round, the instructions and the cycles a
//               turn took, and how much of all the work was spent in it;
//               then the loop itself.  A call in a loop is one
//               instruction: what the word called costs is that word's.
//
//   vars.txt    What the variables cost: fetches and stores, by where and
//               of what, for the code of the files named (all of it if
//               none is).  A variable looks as cheap as a cell of a stack
//               and is not: a store is an instruction that a value left on
//               a stack would not need, a fetch reads memory, and what is
//               fetched is on top of the stack, where the next instruction
//               has to take it from.  It says too which words keep what
//               they are working on in variables, and so can be in use
//               only once.
//
// Cycles are the Verilog CPU's, as cycles.h has them.
//

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../vm.c"
#include "../util/cut.c"
#include "cycles.h"

#define CODE    0x10000         // the places the counts are of
#define MOST    (1 << 14)       // the most words, loops or variables

static uint16_t code[CODE];     // the image's instructions,
static long places;             // and how many it has
static double work[CODE];       // how often each ran
static word WORDS[MOST];        // the words, in order of where they are
static int words;
static int in_word[CODE];       // the word each place is in, or less than nothing
static struct { long from, to; char file[64]; } SPANS[256];
static int spans;
static double all_work, all_cycles;
static FILE* out;               // the report being written,
static int shown, lines;        // how many of its lines are printed too, and how many it has

static void say(const char* format, ...) {
    char text[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    fprintf(out, "%s\n", text);
    if (lines++ < shown) printf("%s\n", text);
}

static void report(const char* dir, const char* name, int print) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    if (out) fclose(out);
    out = fopen(path, "w");
    if (out == NULL) {
        perror(path);
        exit(EXIT_FAILURE);
    }
    shown = print, lines = 0;
}

static const char* name_of(int w) { return(w < 0 ? "(before any word)" : WORDS[w].name); }

// The source file a place's code is of: the smallest file's that has it,
// since a file that includes others has their code in its own; and what
// no file of the system's has is the nucleus's own.
static const char* file_of(long at) {
    const char* is = "nuc.fs";
    long least = -1;
    for(int n=0; n < spans; n++) {
        long size = SPANS[n].to - SPANS[n].from;
        if (at >= SPANS[n].from && at < SPANS[n].to && (least < 0 || size < least)) least = size, is = SPANS[n].file;
    }
    return(is);
}

static void load(const char* hex, const char* tables, const char* sizes, const char* counts) {
    uint8_t* image;
    size_t len = load_image(hex, &image);
    places = len / 2 < CODE ? (long)(len / 2) : CODE;
    memcpy(code, image, places * 2);
    words = words_of(image, len, tables, WORDS, MOST);
    int w = -1;
    for(long at=0; at < CODE; at++) {
        while (w + 1 < words && WORDS[w + 1].at <= at) w++;
        int first = w;                          // of the names for the same code, the first
        while (first > 0 && WORDS[first - 1].at == WORDS[w].at) first--;
        in_word[at] = w < 0 ? -1 : first;
    }
    FILE* in = fopen(sizes, "r");
    char row[256];
    while (in && fgets(row, sizeof(row), in) && spans < 256) {
        long bytes = 0, a, b, c, from = 0;
        if (sscanf(row, "%63s %ld %ld %ld %ld %ld", SPANS[spans].file, &bytes, &a, &b, &c, &from) != 6) continue;
        if (SPANS[spans].file[0] == '(' || !bytes) continue;
        SPANS[spans].from = from, SPANS[spans].to = from + bytes / 2;
        spans++;
    }
    if (in) fclose(in);
    in = fopen(counts, "rb");
    if (in == NULL || fread(work, sizeof(double), CODE, in) != CODE) {
        perror(counts);
        exit(EXIT_FAILURE);
    }
    fclose(in);
    for(long at=0; at < CODE; at++) {
        if (at >= places || !code[at]) work[at] = 0;    // what was defined at the prompt is not the image's
        all_work += work[at];
        all_cycles += work[at] * cycles_of(ins_of(code[at]));
    }
}

// ---- each word, with all it calls

typedef struct { int from, to; double times; } call;
static call CALLS[1 << 16];
static int calls;
static double own[MOST][2];             // what each word did itself: instructions, and cycles
static double called[MOST], entered[MOST];
static bool comes_round[MOST];          // it goes back to its own start, which is then no measure of how often it is entered
static double whole_of[MOST][2];
static char state[MOST];                // 0 not worked out, 1 being worked out, 2 worked out
static int order[MOST];

// The number the instructions before a place push, if they are a push of
// one: the execution token of a call to another page.  Less than nothing
// if they are not.
static long pushed(long at) {
    long value = 0;
    for(long k=at - 1; k >= 0 && (pushes_number(code[k], 0) || pushes_number(code[k], 1)); k--) {
        value += number_in(code[k]);
        if (pushes_number(code[k], 0)) return(value);
    }
    return(-1);
}

// A word and all it calls: instructions, and cycles.  A word that comes
// round to itself is its own, once.
static const double* whole(int w) {
    if (state[w] == 2) return(whole_of[w]);
    if (state[w] == 1) return(own[w]);
    state[w] = 1;
    double total[2] = { own[w][0], own[w][1] };
    for(int n=0; n < calls; n++) {
        if (CALLS[n].from != w || !entered[CALLS[n].to]) continue;
        const double* t = whole(CALLS[n].to);
        total[0] += CALLS[n].times * t[0] / entered[CALLS[n].to];
        total[1] += CALLS[n].times * t[1] / entered[CALLS[n].to];
    }
    whole_of[w][0] = total[0], whole_of[w][1] = total[1];
    state[w] = 2;
    return(whole_of[w]);
}

static int by_whole(const void* a, const void* b) {
    double x = whole_of[*(const int*)a][0], y = whole_of[*(const int*)b][0];
    return(x < y ? 1 : x > y ? -1 : 0);
}

typedef struct { double much; char what[200]; } charge;
static int by_much(const void* a, const void* b) {
    double x = ((const charge*)a)->much, y = ((const charge*)b)->much;
    return(x < y ? 1 : x > y ? -1 : 0);
}

static void paths(const char* hex) {
    long execute = place_of(WORDS, words, "execute");
    double unseen = 0;                  // calls by execute of what is not a number
    for(long at=0; at < places; at++) {
        int w = in_word[at];
        instruction ins = ins_of(code[at]);
        if (w < 0 || !code[at]) continue;
        own[w][0] += work[at];
        own[w][1] += work[at] * cycles_of(ins);
        if (ins.lit.lit_f || ins.alu.op_type == OP_TYPE_ALU) continue;
        long to = goes_to(at, code[at]);
        bool branch = ins.jmp.op_type == OP_TYPE_CJMP;
        if (!branch && to == execute) {
            to = pushed(at);
            if (to < 0) {
                unseen += work[at];
                continue;
            }
        }
        if (to < 0 || to >= CODE) continue;
        if (to == WORDS[w].at) {
            comes_round[w] = true;
        } else if (!branch && in_word[to] >= 0 && WORDS[in_word[to]].at == to) {
            int n = 0;                  // a call, or a jump to another word, which is one
            while (n < calls && (CALLS[n].from != w || CALLS[n].to != in_word[to])) n++;
            if (n == calls) CALLS[calls++] = (call){ w, in_word[to], 0 };
            CALLS[n].times += work[at];
        }
    }
    for(int n=0; n < calls; n++) called[CALLS[n].to] += CALLS[n].times;
    int rows = 0;
    for(int w=0; w < words; w++) {
        double at_start = WORDS[w].at < CODE ? work[WORDS[w].at] : 0;
        entered[w] = comes_round[w] || called[w] > at_start ? called[w] : at_start;
    }
    for(int w=0; w < words; w++) {
        bool has_calls = false;
        for(int n=0; n < calls && !has_calls; n++) has_calls = CALLS[n].from == w;
        whole(w);
        if (in_word[WORDS[w].at < CODE ? WORDS[w].at : 0] == w && (own[w][0] || has_calls)) order[rows++] = w;
    }
    qsort(order, rows, sizeof(int), by_whole);

    say("%s: %.0f instructions of work, %.0f cycles.", hex, all_work, all_cycles);
    say("Each word with all it calls: what would be saved if it were never called.");
    say("");
    say("%-16s %-16s %13s %6s %13s %12s %11s %10s", "word", "file", "instructions", "", "cycles", "its own",
        "entered", "each time");
    for(int r=0; r < rows; r++) {
        int w = order[r];
        char each[32] = "";
        if (entered[w]) snprintf(each, sizeof(each), "%10.1f", whole_of[w][0] / entered[w]);
        say("%-16s %-16s %13.0f %5.1f%% %13.0f %12.0f %11.0f %10s", WORDS[w].name, file_of(WORDS[w].at),
            whole_of[w][0], 100 * whole_of[w][0] / all_work, whole_of[w][1], own[w][0], entered[w], each);
    }
    shown = 0;

    say("");
    say("Where it goes, for the words that cost most: what the word did itself, and what");
    say("each word it calls was charged to it.");
    say("");
    for(int r=0; r < rows && r < 60; r++) {
        static charge parts[MOST];
        int w = order[r], n = 0;
        parts[n].much = own[w][0];
        snprintf(parts[n++].what, sizeof(parts[0].what), "itself");
        for(int c=0; c < calls; c++) {
            if (CALLS[c].from != w || !entered[CALLS[c].to]) continue;
            parts[n].much = CALLS[c].times * whole_of[CALLS[c].to][0] / entered[CALLS[c].to];
            snprintf(parts[n++].what, sizeof(parts[0].what), "%s (%.0f times)", WORDS[CALLS[c].to].name, CALLS[c].times);
        }
        qsort(parts, n, sizeof(charge), by_much);
        say("%s  %.0f", WORDS[w].name, whole_of[w][0]);
        for(int p=0; p < n; p++) {
            if (parts[p].much >= whole_of[w][0] / 200) {
                say("    %13.0f %5.1f%%  %s", parts[p].much, 100 * parts[p].much / whole_of[w][0], parts[p].what);
            }
        }
    }

    say("");
    say("Entered more often than any call that can be seen: by execute of a vector, or as a");
    say("task, or from the prompt.  What they cost then is charged to no one above them.");
    say("");
    static charge hidden[MOST];
    int n = 0;
    for(int w=0; w < words; w++) {
        if (comes_round[w] || !entered[w] || entered[w] <= called[w]) continue;
        double share = whole_of[w][0] * (entered[w] - called[w]) / entered[w];
        if (share < all_work / 2000) continue;
        hidden[n].much = share;
        snprintf(hidden[n++].what, sizeof(hidden[0].what), "%-16s %13.0f %5.1f%%  %.0f of %.0f times", WORDS[w].name,
                 share, 100 * share / all_work, entered[w] - called[w], entered[w]);
    }
    qsort(hidden, n, sizeof(charge), by_much);
    for(int h=0; h < n; h++) say("    %s", hidden[h].what);
    say("");
    say("Calls by execute of what is not a number in the code: %.0f", unseen);
}

// ---- the loops

typedef struct { long head, tail; double cycles, ran, turns; bool inner; int calls; } loop;
static loop LOOPS[MOST];
static int loops;

static int by_cycles(const void* a, const void* b) {
    double x = ((const loop*)a)->cycles, y = ((const loop*)b)->cycles;
    return(x < y ? 1 : x > y ? -1 : 0);
}

static void the_loops(const char* hex) {
    // Every branch that goes back, within a word.  A jump back to the
    // start of a word, or into another, is a call that will not come back
    // and no loop.
    static struct { long head, tail; } back[MOST];
    int backs = 0;
    for(long at=0; at < places && backs < MOST; at++) {
        instruction ins = ins_of(code[at]);
        if (!code[at] || ins.lit.lit_f || ins.alu.op_type == OP_TYPE_ALU || ins.jmp.op_type == OP_TYPE_CALL) continue;
        long head = goes_to(at, code[at]);
        if (head < 0 || head > at) continue;
        bool starts = false;
        for(long a=head; a <= at && !starts; a++) starts = in_word[a] >= 0 && WORDS[in_word[a]].at == a;
        if (!starts) back[backs].head = head, back[backs++].tail = at;
    }
    for(int b=0; b < backs; b++) {
        loop l = { back[b].head, back[b].tail, 0, 0, work[back[b].tail], true, 0 };
        if (l.turns < 1) continue;
        for(long a=l.head; a <= l.tail; a++) {
            instruction ins = ins_of(code[a]);
            l.ran += work[a];
            l.cycles += work[a] * cycles_of(ins);
            if (!ins.lit.lit_f && ins.jmp.op_type == OP_TYPE_CALL && work[a] >= l.turns / 2) l.calls++;
        }
        for(int o=0; o < backs; o++) {
            if (o != b && back[o].head >= l.head && back[o].tail <= l.tail) l.inner = false;
        }
        LOOPS[loops++] = l;
    }
    qsort(LOOPS, loops, sizeof(loop), by_cycles);

    say("%s: %.0f instructions of work, %.0f cycles; %d loops ran.", hex, all_work, all_cycles, loops);
    say("A loop with another inside it has that one's cost in its own.");
    say("");
    say("%12s %6s %11s %8s %8s %5s  %-16s where", "cycles", "of all", "turns", "ins/turn", "cyc/turn", "calls", "word");
    for(int n=0; n < loops; n++) {
        const loop* l = &LOOPS[n];
        say("%12.0f %5.1f%% %11.0f %8.1f %8.1f %5d  %-16s %04lx-%04lx%s", l->cycles, 100 * l->cycles / all_cycles,
            l->turns, l->ran / l->turns, l->cycles / l->turns, l->calls, name_of(in_word[l->head]), l->head, l->tail,
            l->inner ? "" : "  (has a loop inside)");
    }
    shown = 0;
    static word_node opcodes[1024];
    init_opcodes(opcodes);
    for(int n=0; n < loops; n++) {
        const loop* l = &LOOPS[n];
        say("");
        say("%s  %04lx-%04lx: %.0f turns, %.1f instructions and %.1f cycles a turn, %.1f%% of all cycles",
            name_of(in_word[l->head]), l->head, l->tail, l->turns, l->ran / l->turns, l->cycles / l->turns,
            100 * l->cycles / all_cycles);
        for(long a=l->head; a <= l->tail; a++) {
            instruction ins = ins_of(code[a]);
            char is[256];
            if (!ins.lit.lit_f && ins.alu.op_type != OP_TYPE_ALU) {
                long to = goes_to(a, code[a]);
                bool named = to >= 0 && to < CODE && in_word[to] >= 0 && WORDS[in_word[to]].at == to;
                const char* how = ins.jmp.op_type == OP_TYPE_CALL ? "call" : ins.jmp.op_type == OP_TYPE_CJMP ? "0branch" : "jump";
                if (named) snprintf(is, sizeof(is), "%s %s", how, WORDS[in_word[to]].name);
                else snprintf(is, sizeof(is), "%s %04lx", how, to);
            } else {
                decode_instruction(is, ins, opcodes);
            }
            say("    %04lx %04x %12.0f  %s", a, code[a], work[a], is);
        }
    }
}

// ---- the variables

static word DATA[MOST];         // the names of cells: of each label for data, and of each word that only pushes one
static int data;
typedef struct { const char* name; double fetched, stored; int users, user; } variable;
static variable VARS[MOST];
static int vars;

static const char* cell_name(long cell) {
    static char numbered[MOST][24];
    static int next;
    for(int n=0; n < data; n++) if (DATA[n].at == cell) return(DATA[n].name);
    snprintf(numbered[next % MOST], sizeof(numbered[0]), "cell %lx", cell);
    return(numbered[next++ % MOST]);
}

static variable* var_of(const char* name) {
    for(int n=0; n < vars; n++) if (!strcmp(VARS[n].name, name)) return(&VARS[n]);
    VARS[vars] = (variable){ name, 0, 0, 0, -1 };
    return(&VARS[vars++]);
}

static void used_by(variable* v, int w) {
    if (v->user == w) return;
    if (v->users++ == 0) v->user = w; else v->user = -2;
}

static int by_use(const void* a, const void* b) {
    const variable* x = a;
    const variable* y = b;
    double p = x->fetched + x->stored, q = y->fetched + y->stored;
    return(p < q ? 1 : p > q ? -1 : 0);
}

static bool is_fetch(instruction ins) { return(ins.lit.lit_f && ins.lit.lit_shifts == 3 && !ins.lit.lit_add); }
static bool is_store(instruction ins) { return(ins.lit.lit_f && ins.lit.lit_shifts == 3 && ins.lit.lit_add); }

static bool wanted(long at, char** files, int count) {
    for(int n=0; n < count; n++) if (!strcmp(file_of(at), files[n])) return(true);
    return(!count);
}

static void variables(const char* hex, const char* tables, char** files, int count) {
    // A label for data is the name of its cell; and a word that only
    // pushes a number is a name for the cell of that number.
    instruction leaves[8];
    lookup_word(FORTH_WORDS, "exit", leaves);
    data = data_of(tables, DATA, MOST);
    long port = (long)data_called(tables, "gstate"), port_end = -1;
    for(int n=0; n < data && port; n++) {       // the port's cells are from there to the next label
        if (DATA[n].at > port && (port_end < 0 || DATA[n].at < port_end)) port_end = DATA[n].at;
    }
    static struct { long from, to; } aside[64]; // from a word named ...-state up to the one named ...-end
    int asides = 0;
    for(int w=0; w < words && data < MOST; w++) {
        long at = WORDS[w].at, value = 0;
        if (at >= places || !pushes_number(code[at], 0)) continue;
        value = number_in(code[at++]);
        while (at < places && pushes_number(code[at], 1)) value += number_in(code[at++]);
        if (at >= places || code[at] != *(uint16_t*)leaves) continue;
        DATA[data] = WORDS[w], DATA[data++].at = value;
    }
    for(int n=0; n < data && asides < 64; n++) {
        size_t len = strlen(DATA[n].name);
        if (len < 6 || strcmp(DATA[n].name + len - 6, "-state")) continue;
        for(int e=0; e < data; e++) {
            if (strlen(DATA[e].name) == len - 2 && !strncmp(DATA[e].name, DATA[n].name, len - 6) &&
                !strcmp(DATA[e].name + len - 6, "-end")) {
                aside[asides].from = DATA[n].at, aside[asides++].to = DATA[e].at;
            }
        }
    }

    // Which words can be in use more than once.  A word that keeps what it
    // is working on in a variable cannot.  The port is not that: it is
    // what drawing is told to draw with.  Nor are the variables that one
    // word has in hand in one run of memory, which it sets aside if it is
    // run while it is running (draw-text, copy-bits), nor the ...-busy
    // that says it is running.  A cell nothing here stores to is a number,
    // not something kept.  This is of the code as it is, not of what ran.
    bool* written = calloc((size_t)1 << LIT_BITS, 1);   // of each cell an instruction reaches
    static bool keeps[MOST], counted[MOST];
    static char kept_in[MOST][400];
    int every = 0, keeping = 0;
    for(long at=0; at < places; at++) {
        instruction ins = ins_of(code[at]);
        if (code[at] && wanted(at, files, count) && is_store(ins)) written[ins.lit.lit_v] = true;
    }
    for(long at=0; at < places; at++) {
        instruction ins = ins_of(code[at]);
        int w = in_word[at];
        if (!code[at] || w < 0 || !wanted(at, files, count)) continue;
        if (!counted[w]) counted[w] = true, every++;
        if (!ins.lit.lit_f || ins.lit.lit_shifts != 3 || !written[ins.lit.lit_v]) continue;
        long cell = VAR_BASE + ins.lit.lit_v;
        const char* name = cell_name(cell);
        size_t len = strlen(name);
        bool set_aside = false;
        for(int n=0; n < asides; n++) set_aside = set_aside || (cell >= aside[n].from && cell < aside[n].to);
        if ((port && cell >= port && cell < port_end) || set_aside || (len >= 5 && !strcmp(name + len - 5, "-busy"))) continue;
        if (!keeps[w]) keeps[w] = true, keeping++;
        if (!strstr(kept_in[w], name) && strlen(kept_in[w]) + len + 2 < sizeof(kept_in[0])) {
            strcat(kept_in[w], " ");
            strcat(kept_in[w], name);
        }
    }

    // What ran.
    static double word_all[MOST], word_fetched[MOST], word_stored[MOST];
    double total = 0, fetched = 0, stored = 0, joined = 0;
    for(long at=0; at < places; at++) {
        instruction ins = ins_of(code[at]);
        int w = in_word[at];
        if (!code[at] || !work[at] || w < 0 || !wanted(at, files, count)) continue;
        total += work[at];
        word_all[w] += work[at];
        if (!is_fetch(ins) && !is_store(ins)) continue;
        variable* v = var_of(cell_name(VAR_BASE + ins.lit.lit_v));
        used_by(v, w);
        if (is_store(ins)) {
            stored += work[at], v->stored += work[at], word_stored[w] += work[at];
            continue;
        }
        fetched += work[at], v->fetched += work[at], word_fetched[w] += work[at];
        // An operation on what was fetched and the cell under it, that
        // leaves one result: T->IN, an operation of two, to T, the data
        // stack one shorter, and no return.
        instruction next = ins_of(at + 1 < places ? code[at + 1] : 0);
        if (!next.lit.lit_f && next.alu.op_type == OP_TYPE_ALU && next.alu.in_mux == INPUT_T &&
            next.alu.out_mux == OUTPUT_T && next.alu.dstack == -1 && next.alu.rstack == 0 && !next.alu.r_eip &&
            (ALU_EITHER_WAY(next.alu.alu_op) || next.alu.alu_op == ALU_GT || next.alu.alu_op == ALU_U_GT)) {
            joined += work[at];
        }
    }
    if (total <= 0) {
        fprintf(stderr, "none of the code of the files named ran: are they files the image's .sizes has?\n");
        exit(EXIT_FAILURE);
    }
    qsort(VARS, vars, sizeof(variable), by_use);

    say("%s: the code of %s", hex, count ? "the files named" : "every file");
    say("instructions run: %.0f", total);
    say("  fetches of variables: %.0f (%.1f%%), each a cycle more than a cell of a stack", fetched, 100 * fetched / total);
    say("  stores to variables:  %.0f (%.1f%%), each an instruction a value left on a stack would not need",
        stored, 100 * stored / total);
    say("  fetches followed at once by an operation of two on what was fetched: %.0f (%.1f%%),", joined, 100 * joined / total);
    say("    each two instructions that one would do with the value on the return stack");
    say("");
    say("The variables, by how often they are fetched and stored.  `own` is a variable");
    say("that one word alone stores and fetches: it is that word's, and could be on a stack.");
    say("");
    say("%12s %12s  %5s  variable", "fetched", "stored", "words");
    for(int n=0; n < vars; n++) {
        const variable* v = &VARS[n];
        say("%12.0f %12.0f  %5d  %s%s%s", v->fetched, v->stored, v->users, v->name, v->users == 1 ? "  own: " : "",
            v->users == 1 ? name_of(v->user) : "");
    }
    shown = 0;
    say("");
    say("The words, by what they spend on variables: all they run, fetches and stores.");
    say("");
    static int by[MOST];
    int spenders = 0;
    for(int w=0; w < words; w++) if (word_fetched[w] + word_stored[w] > 0) by[spenders++] = w;
    for(int a=0; a < spenders; a++) {           // the most first: there are few
        for(int b=a + 1; b < spenders; b++) {
            if (word_fetched[by[b]] + word_stored[by[b]] > word_fetched[by[a]] + word_stored[by[a]]) {
                int swap = by[a];
                by[a] = by[b], by[b] = swap;
            }
        }
        int w = by[a];
        say("%s: %.0f run, %.0f fetches and %.0f stores (%.0f%%)", WORDS[w].name, word_all[w], word_fetched[w],
            word_stored[w], 100 * (word_fetched[w] + word_stored[w]) / word_all[w]);
    }
    say("");
    say("Which words can be in use more than once: by two tasks, or by something that draws");
    say("while something else is drawing.");
    say("");
    shown = lines + 1;
    say("words: %d; %d keep nothing in a variable but the port's, and %d do:", every, every - keeping, keeping);
    shown = 0;
    for(int w=0; w < words; w++) if (keeps[w]) say("    %s:%s", WORDS[w].name, kept_in[w]);
}

int main(int argc, char *argv[]) {
    if (argc < 6) {
        fprintf(stderr, "Usage: %s IMAGE.hex IMAGE.tables IMAGE.sizes COUNTS OUT [FILE.fs ...]\n", argv[0]);
        return(1);
    }
    load(argv[1], argv[2], argv[3], argv[4]);
    if (all_work <= 0) {
        fprintf(stderr, "%s: nothing ran\n", argv[4]);
        return(1);
    }
    report(argv[5], "paths.txt", 34);
    paths(argv[1]);
    printf("\nall of it, where each one's cost goes, and what is reached by execute: %s/paths.txt\n\n", argv[5]);
    report(argv[5], "loops.txt", 24);
    the_loops(argv[1]);
    printf("\nall of them, and each loop's instructions: %s/loops.txt\n\n", argv[5]);
    report(argv[5], "vars.txt", 20);
    variables(argv[1], argv[2], argv + 6, argc - 6);
    printf("\nall of it, and every word: %s/vars.txt\n", argv[5]);
    fclose(out);
    return(0);
}
