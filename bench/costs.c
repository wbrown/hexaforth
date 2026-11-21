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
// Cycles are the Verilog CPU's, as cycles.h has them.  What an instruction
// with a number in it does (pushes it, adds it, fetches or stores the
// variable at it) is asked of the machine's step (kinds.h).
//
// Nothing here has room for only so many of anything: what there is is
// counted, and room is made for that.
//

#include <stdarg.h>
#include "../util/fuse.h"
#include "../util/cut.h"
#include "../util/kinds.h"
#include "../util/loops.h"
#include "cycles.h"
#include "files.h"
#include "work.h"

static cut_image IMAGE;         // the image, cut into its runs: its instructions, its words, and where its loops are
static const uint16_t* code;    // its instructions,
static long places;             // and how many of them the work is of
static double* work;            // how often each ran
static const word* WORDS;       // the words, in order of where they are
static int words;
static int* in_word;            // the word each place is in, or less than nothing
static span* SPANS;             // the files the code is of (files.h)
static int spans;
static double all_work, all_cycles;
static FILE* out;               // the report being written,
static int shown, lines;        // how many of its lines are printed too, and how many it has

// A line of the report, however long: it is written to the report, and
// printed too if it is one of the first.
static void say(const char* format, ...) {
    va_list args, again;
    va_start(args, format);
    va_copy(again, args);
    vfprintf(out, format, args);
    fputc('\n', out);
    va_end(args);
    if (lines++ < shown) {
        vprintf(format, again);
        putchar('\n');
    }
    va_end(again);
}

static void report(const char* dir, const char* name, int print) {
    char* path = NULL;
    text_more(&path, "%s/%s", dir, name);
    if (out) fclose(out);
    out = fopen(path, "w");
    if (out == NULL) {
        perror(path);
        exit(EXIT_FAILURE);
    }
    free(path);
    shown = print, lines = 0;
}

static const char* name_of(int w) { return(w < 0 ? "(before any word)" : WORDS[w].name); }

// The cycles the instruction at a place takes.
static int cycles_at(long at) { return(cycles_of(ins_of(code[at]), NUMBER_IN[code[at]].does == FETCHES_AT_IT)); }

// The source file a place's code is of; and what no file of the system's
// has is the nucleus's own.
static const char* file_of(long at) {
    const char* is = file_at(SPANS, spans, at);
    return(is ? is : "nuc.fs");
}

// Is a place the start of a word?
static bool starts_word(long at) {
    return(at >= 0 && at < places && in_word[at] >= 0 && WORDS[in_word[at]].at == at);
}

static void load(const char* hex, const char* tables, const char* sizes, const char* counts) {
    cut(&IMAGE, hex, tables);
    long counted = work_read(counts, &work, NULL);
    if (!counted) exit(EXIT_FAILURE);
    code = IMAGE.code;
    places = IMAGE.instructions < counted ? IMAGE.instructions : counted;
    WORDS = IMAGE.words;
    words = IMAGE.n;
    in_word = room_for(places, sizeof(int));
    int w = -1;
    for(long at=0; at < places; at++) {
        while (w + 1 < words && WORDS[w + 1].at <= at) w++;
        int first = w;                          // of the names for the same code, the first
        while (first > 0 && WORDS[first - 1].at == WORDS[w].at) first--;
        in_word[at] = w < 0 ? -1 : first;
    }
    spans = files_of(sizes, &SPANS);
    for(long at=0; at < places; at++) {
        if (!code[at]) work[at] = 0;            // nothing is there: what ran was put there at the prompt
        all_work += work[at];
        all_cycles += work[at] * cycles_at(at);
    }
}

// ---- each word, with all it calls

typedef struct { int from, to; double times; } call;
static call* CALLS;                     // as many as there are, and room for more as they are found (room.h)
static long calls, calls_room;
static double (*own)[2];                // by word.  What each did itself: instructions, and cycles
static double* called;
static double* entered;
static bool* comes_round;               // it goes back to its own start, which is then no measure of how often it is entered
static double (*whole_of)[2];
static char* worked_out;                // 0 not worked out, 1 being worked out, 2 worked out
static int* order;

// The call from one word to another, which is made if it is not there.
static call* call_from(int from, int to) {
    for(long n=0; n < calls; n++) if (CALLS[n].from == from && CALLS[n].to == to) return(&CALLS[n]);
    ONE_MORE(CALLS, calls, calls_room);
    CALLS[calls] = (call){ from, to, 0 };
    return(&CALLS[calls++]);
}

// The number the instructions before a place push, if they are a push of
// one and what is added to it: the execution token of a call to another
// page.  Less than nothing if they are not.
static long pushed(long at) {
    long value = 0;
    for(long k=at - 1; k >= 0; k--) {
        int does = NUMBER_IN[code[k]].does;
        if (does != PUSHES_IT && does != ADDS_IT) break;
        value += NUMBER_IN[code[k]].number;
        if (does == PUSHES_IT) return(value);
    }
    return(-1);
}

// A word and all it calls: instructions, and cycles.  A word that comes
// round to itself is its own, once.
static const double* whole(int w) {
    if (worked_out[w] == 2) return(whole_of[w]);
    if (worked_out[w] == 1) return(own[w]);
    worked_out[w] = 1;
    double total[2] = { own[w][0], own[w][1] };
    for(int n=0; n < calls; n++) {
        if (CALLS[n].from != w || !entered[CALLS[n].to]) continue;
        const double* t = whole(CALLS[n].to);
        total[0] += CALLS[n].times * t[0] / entered[CALLS[n].to];
        total[1] += CALLS[n].times * t[1] / entered[CALLS[n].to];
    }
    whole_of[w][0] = total[0], whole_of[w][1] = total[1];
    worked_out[w] = 2;
    return(whole_of[w]);
}

static int by_whole(const void* a, const void* b) {
    double x = whole_of[*(const int*)a][0], y = whole_of[*(const int*)b][0];
    return(x < y ? 1 : x > y ? -1 : 0);
}

typedef struct { double much; char* what; } charge;     // and what it is of, as text that is as long as it is
static int by_much(const void* a, const void* b) {
    double x = ((const charge*)a)->much, y = ((const charge*)b)->much;
    return(x < y ? 1 : x > y ? -1 : 0);
}

static void paths(const char* hex) {
    long execute = place_of(WORDS, words, "execute");
    double unseen = 0;                  // calls by execute of what is not a number
    own = room_for(words, sizeof(*own));
    called = room_for(words, sizeof(double));
    entered = room_for(words, sizeof(double));
    comes_round = room_for(words, sizeof(bool));
    whole_of = room_for(words, sizeof(*whole_of));
    worked_out = room_for(words, sizeof(char));
    order = room_for(words, sizeof(int));
    for(long at=0; at < places; at++) {
        int w = in_word[at];
        instruction ins = ins_of(code[at]);
        if (w < 0 || !code[at]) continue;
        own[w][0] += work[at];
        own[w][1] += work[at] * cycles_at(at);
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
        if (to < 0 || to >= places) continue;
        if (to == WORDS[w].at) {
            comes_round[w] = true;
        } else if (!branch && starts_word(to)) {
            call_from(w, in_word[to])->times += work[at];   // a call, or a jump to another word, which is one
        }
    }
    for(int n=0; n < calls; n++) called[CALLS[n].to] += CALLS[n].times;
    int rows = 0;
    for(int w=0; w < words; w++) {
        double at_start = WORDS[w].at < places ? work[WORDS[w].at] : 0;
        entered[w] = comes_round[w] || called[w] > at_start ? called[w] : at_start;
    }
    for(int w=0; w < words; w++) {
        bool has_calls = false;
        for(int n=0; n < calls && !has_calls; n++) has_calls = CALLS[n].from == w;
        whole(w);
        if (in_word[WORDS[w].at < places ? WORDS[w].at : 0] == w && (own[w][0] || has_calls)) order[rows++] = w;
    }
    qsort(order, rows, sizeof(int), by_whole);

    say("%s: %.0f instructions of work, %.0f cycles.", hex, all_work, all_cycles);
    say("Each word with all it calls: what would be saved if it were never called.");
    say("");
    say("%-16s %-16s %13s %6s %13s %12s %11s %10s", "word", "file", "instructions", "", "cycles", "its own",
        "entered", "each time");
    for(int r=0; r < rows; r++) {
        int w = order[r];
        char* each = NULL;
        text_more(&each, "");
        if (entered[w]) text_more(&each, "%10.1f", whole_of[w][0] / entered[w]);
        say("%-16s %-16s %13.0f %5.1f%% %13.0f %12.0f %11.0f %10s", WORDS[w].name, file_of(WORDS[w].at),
            whole_of[w][0], 100 * whole_of[w][0] / all_work, whole_of[w][1], own[w][0], entered[w], each);
        free(each);
    }
    shown = 0;

    say("");
    say("Where it goes, for the words that cost most: what the word did itself, and what");
    say("each word it calls was charged to it.");
    say("");
    charge* parts = room_for(calls + 1, sizeof(charge));        // a word's own, and one for each call
    for(int r=0; r < rows && r < 60; r++) {
        int w = order[r], n = 0;
        parts[n].much = own[w][0];
        text_more(&parts[n++].what, "itself");
        for(int c=0; c < calls; c++) {
            if (CALLS[c].from != w || !entered[CALLS[c].to]) continue;
            parts[n].much = CALLS[c].times * whole_of[CALLS[c].to][0] / entered[CALLS[c].to];
            text_more(&parts[n++].what, "%s (%.0f times)", WORDS[CALLS[c].to].name, CALLS[c].times);
        }
        qsort(parts, n, sizeof(charge), by_much);
        say("%s  %.0f", WORDS[w].name, whole_of[w][0]);
        for(int p=0; p < n; p++) {
            if (parts[p].much >= whole_of[w][0] / 200) {
                say("    %13.0f %5.1f%%  %s", parts[p].much, 100 * parts[p].much / whole_of[w][0], parts[p].what);
            }
            free(parts[p].what);
            parts[p].what = NULL;
        }
    }
    free(parts);

    say("");
    say("Entered more often than any call that can be seen: by execute of a vector, or as a");
    say("task, or from the prompt.  What they cost then is charged to no one above them.");
    say("");
    charge* hidden = room_for(words, sizeof(charge));
    int n = 0;
    for(int w=0; w < words; w++) {
        if (comes_round[w] || !entered[w] || entered[w] <= called[w]) continue;
        double share = whole_of[w][0] * (entered[w] - called[w]) / entered[w];
        if (share < all_work / 2000) continue;
        hidden[n].much = share;
        text_more(&hidden[n++].what, "%-16s %13.0f %5.1f%%  %.0f of %.0f times", WORDS[w].name,
                  share, 100 * share / all_work, entered[w] - called[w], entered[w]);
    }
    qsort(hidden, n, sizeof(charge), by_much);
    for(int h=0; h < n; h++) {
        say("    %s", hidden[h].what);
        free(hidden[h].what);
    }
    free(hidden);
    say("");
    say("Calls by execute of what is not a number in the code: %.0f", unseen);
}

// ---- the loops

typedef struct { long head, tail; double cycles, ran, turns; bool inner; int calls; } ran_loop;

static int by_cycles(const void* a, const void* b) {
    double x = ((const ran_loop*)a)->cycles, y = ((const ran_loop*)b)->cycles;
    return(x < y ? 1 : x > y ? -1 : 0);
}

static void the_loops(const char* hex) {
    loop* back;                         // the image's loops (loops.h), of which those that ran are kept
    int backs = loops_of(&IMAGE, &back), loops = 0;
    ran_loop* LOOPS = room_for(backs, sizeof(ran_loop));
    for(int b=0; b < backs; b++) {
        if (back[b].tail >= places) continue;
        ran_loop l = { back[b].head, back[b].tail, 0, 0, work[back[b].tail], true, 0 };
        if (l.turns < 1) continue;
        for(long a=l.head; a <= l.tail; a++) {
            instruction ins = ins_of(code[a]);
            l.ran += work[a];
            l.cycles += work[a] * cycles_at(a);
            if (!ins.lit.lit_f && ins.jmp.op_type == OP_TYPE_CALL && work[a] >= l.turns / 2) l.calls++;
        }
        for(int o=0; o < backs; o++) if (loop_within(&back[o], &back[b])) l.inner = false;
        LOOPS[loops++] = l;
    }
    qsort(LOOPS, loops, sizeof(ran_loop), by_cycles);

    say("%s: %.0f instructions of work, %.0f cycles; %d loops ran.", hex, all_work, all_cycles, loops);
    say("A loop with another inside it has that one's cost in its own.");
    say("");
    say("%12s %6s %11s %8s %8s %5s  %-16s where", "cycles", "of all", "turns", "ins/turn", "cyc/turn", "calls", "word");
    for(int n=0; n < loops; n++) {
        const ran_loop* l = &LOOPS[n];
        say("%12.0f %5.1f%% %11.0f %8.1f %8.1f %5d  %-16s %04lx-%04lx%s", l->cycles, 100 * l->cycles / all_cycles,
            l->turns, l->ran / l->turns, l->cycles / l->turns, l->calls, name_of(in_word[l->head]), l->head, l->tail,
            l->inner ? "" : "  (has a loop inside)");
    }
    shown = 0;
    for(int n=0; n < loops; n++) {
        const ran_loop* l = &LOOPS[n];
        say("");
        say("%s  %04lx-%04lx: %.0f turns, %.1f instructions and %.1f cycles a turn, %.1f%% of all cycles",
            name_of(in_word[l->head]), l->head, l->tail, l->turns, l->ran / l->turns, l->cycles / l->turns,
            100 * l->cycles / all_cycles);
        for(long a=l->head; a <= l->tail; a++) {
            instruction ins = ins_of(code[a]);
            char is[256];
            if (!ins.lit.lit_f && ins.alu.op_type != OP_TYPE_ALU) {
                long to = goes_to(a, code[a]);
                const char* how = ins.jmp.op_type == OP_TYPE_CALL ? "call" : ins.jmp.op_type == OP_TYPE_CJMP ? "0branch" : "jump";
                if (starts_word(to)) snprintf(is, sizeof(is), "%s %s", how, WORDS[in_word[to]].name);
                else snprintf(is, sizeof(is), "%s %04lx", how, to);
            } else {
                decode_instruction(is, ins, FORTH_WORDS);
            }
            say("    %04lx %04x %12.0f  %s", a, code[a], work[a], is);
        }
    }
    free(LOOPS);
    free(back);
}

// ---- the variables

// Each of these has as many as there are, and room for more as they are
// found (room.h).
static word* LABELS;            // the names of cells: of each label for data, and of each word that only pushes one
static long labels, labels_room;
typedef struct { const char* name; double fetched, stored; int users, user; } variable;
static variable* VARS;          // those that are fetched or stored
static long vars, vars_room;
static const char unnamed_is[] = "cell ";
typedef struct { long cell; char name[sizeof(unnamed_is) + 2 * sizeof(long)]; } unnamed;   // and its number in hex
static unnamed** UNNAMED;       // the cells that have no name, each said by its number
static long unnameds, unnameds_room;

static const char* cell_name(long cell) {
    for(long n=0; n < labels; n++) if (LABELS[n].at == cell) return(LABELS[n].name);
    for(long n=0; n < unnameds; n++) if (UNNAMED[n]->cell == cell) return(UNNAMED[n]->name);
    ONE_MORE(UNNAMED, unnameds, unnameds_room);
    UNNAMED[unnameds] = room_for(1, sizeof(unnamed));
    UNNAMED[unnameds]->cell = cell;
    snprintf(UNNAMED[unnameds]->name, sizeof(UNNAMED[0]->name), "%s%lx", unnamed_is, cell);
    return(UNNAMED[unnameds++]->name);
}

static long var_of(const char* name) {
    for(long n=0; n < vars; n++) if (!strcmp(VARS[n].name, name)) return(n);
    ONE_MORE(VARS, vars, vars_room);
    VARS[vars] = (variable){ name, 0, 0, 0, -1 };
    return(vars++);
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

// What an instruction does with a variable, as the step says (kinds.h);
// and which variable it is of, as where the one that stores to it is among
// those that store, or less than nothing if it is of none.
static bool is_fetch(uint16_t w) { return(NUMBER_IN[w].does == FETCHES_AT_IT); }
static bool is_store(uint16_t w) { return(NUMBER_IN[w].does == STORES_AT_IT); }

static int variable_at(uint16_t w) {
    const numbered* stores = is_fetch(w) || is_store(w) ? numbered_as(STORES_AT_IT, NUMBER_IN[w].number) : NULL;
    return(stores ? (int)(stores - NUMBERED[STORES_AT_IT]) : -1);
}

static bool in_files(long at, char** files, int count) {
    for(int n=0; n < count; n++) if (!strcmp(file_of(at), files[n])) return(true);
    return(!count);
}

// A name more in what a word keeps things in, if it is not there.
static void also_in(char** kept_in, const char* name) {
    if (*kept_in && strstr(*kept_in, name)) return;
    text_more(kept_in, " %s", name);
}

// How long a name is before an ending, if it has one before that ending;
// or nothing.
static size_t ends_in(const char* name, const char* ending) {
    size_t len = strlen(name), end = strlen(ending);
    return(len > end && !strcmp(name + len - end, ending) ? len - end : 0);
}

static void variables(const char* hex, const char* tables, char** files, int count) {
    // A label for data is the name of its cell; and a word that only
    // pushes a number is a name for the cell of that number.
    uint16_t leaves = ins_named("exit");
    if (!leaves) {
        fprintf(stderr, "costs: exit is not one instruction\n");
        exit(EXIT_FAILURE);
    }
    labels = data_of(tables, &LABELS, &labels_room);
    long port = (long)data_called(tables, "gstate"), port_end = -1;
    for(long n=0; n < labels && port; n++) {    // the port's cells are from there to the next label
        if (LABELS[n].at > port && (port_end < 0 || LABELS[n].at < port_end)) port_end = LABELS[n].at;
    }
    for(int w=0; w < words; w++) {
        long at = WORDS[w].at, value = 0;
        if (at >= places || NUMBER_IN[code[at]].does != PUSHES_IT) continue;
        value = NUMBER_IN[code[at++]].number;
        while (at < places && NUMBER_IN[code[at]].does == ADDS_IT) value += NUMBER_IN[code[at++]].number;
        if (at >= places || code[at] != leaves) continue;
        ONE_MORE(LABELS, labels, labels_room);
        LABELS[labels] = WORDS[w], LABELS[labels++].at = value;
    }
    struct { long from, to; }* aside = NULL;    // from a word named ...-state up to the one named ...-end
    long asides = 0, asides_room = 0;
    for(long n=0; n < labels; n++) {
        size_t stem = ends_in(LABELS[n].name, "-state");
        if (!stem) continue;
        for(long e=0; e < labels; e++) {
            if (ends_in(LABELS[e].name, "-end") == stem && !strncmp(LABELS[e].name, LABELS[n].name, stem)) {
                ONE_MORE(aside, asides, asides_room);
                aside[asides].from = LABELS[n].at, aside[asides++].to = LABELS[e].at;
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
    bool* written = room_for(numbers[STORES_AT_IT], sizeof(bool));     // of each variable an instruction stores to
    bool* keeps = room_for(words, sizeof(bool));
    bool* counted = room_for(words, sizeof(bool));
    char** kept_in = room_for(words, sizeof(char*));
    int every = 0, keeping = 0;
    for(long at=0; at < places; at++) {
        if (code[at] && in_files(at, files, count) && is_store(code[at])) written[variable_at(code[at])] = true;
    }
    for(long at=0; at < places; at++) {
        int w = in_word[at], var = variable_at(code[at]);
        if (!code[at] || w < 0 || !in_files(at, files, count)) continue;
        if (!counted[w]) counted[w] = true, every++;
        if (var < 0 || !written[var]) continue;
        long cell = NUMBER_IN[code[at]].number;
        const char* name = cell_name(cell);
        bool set_aside = false;
        for(long n=0; n < asides; n++) set_aside = set_aside || (cell >= aside[n].from && cell < aside[n].to);
        if ((port && cell >= port && cell < port_end) || set_aside || ends_in(name, "-busy")) continue;
        if (!keeps[w]) keeps[w] = true, keeping++;
        also_in(&kept_in[w], name);
    }

    // What ran.
    double* word_all = room_for(words, sizeof(double));
    double* word_fetched = room_for(words, sizeof(double));
    double* word_stored = room_for(words, sizeof(double));
    double total = 0, fetched = 0, stored = 0, joined = 0;
    for(long at=0; at < places; at++) {
        int w = in_word[at];
        if (!code[at] || !work[at] || w < 0 || !in_files(at, files, count)) continue;
        total += work[at];
        word_all[w] += work[at];
        if (!is_fetch(code[at]) && !is_store(code[at])) continue;
        int v = var_of(cell_name(NUMBER_IN[code[at]].number));
        used_by(&VARS[v], w);
        if (is_store(code[at])) {
            stored += work[at], VARS[v].stored += work[at], word_stored[w] += work[at];
            continue;
        }
        fetched += work[at], VARS[v].fetched += work[at], word_fetched[w] += work[at];
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
    int* by = room_for(words, sizeof(int));
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
    get_ready();
    list_kinds();
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
