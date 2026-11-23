//
// survey IMAGE.hex IMAGE.tables [COUNTS [REPORT]]
//
// Reports how a program uses the instruction encoding.
//
// An ALU instruction is a set of fields: the source of the ALU's input,
// the operation, the destination of the result, how each stack moves, and
// whether it returns.  Every combination is an instruction the machine
// executes.  This program covers all of them, with no opinion about which
// are useful, in four parts:
//
//   1. The encoding space.  For each source, operation and destination:
//      the number of kinds of instruction (kinds.h), how many of those
//      have a name, how many the image uses, and how often they run.
//   2. Every instruction the image uses, ordered by how often it runs.
//   3. The instructions that are not used and could be: every run of
//      instructions in the image that one instruction, or two, is
//      equivalent to, ordered by how many instructions the replacement
//      would save when the program runs.  cross.fs has already joined what
//      it can, so these are the runs it left.  Under each run the report
//      lists every place it occurs: the address, the word, and how often
//      that place ran.
//   4. Every instruction, with the representative of its kind.
//
// The machine decides what an instruction does and whether a run is
// equivalent to an instruction.  Both come from the step on terms
// (fuse.h), which cross.fs uses as well.  This file does not encode what
// any instruction does.
//
// COUNTS is the number of times each instruction ran, in the format
// bench/profile.c writes (build/profile/desktop.counts): the desktop's
// work, and one turn of its idle loop.  Without COUNTS, every place in
// the image counts once.  The whole report is written to REPORT
// (build/survey.txt if none is named), and the top of each part is also
// printed.
//

#include "fuse.h"
#include "cut.h"
#include "kinds.h"
#include "../bench/work.h"

#define LONGEST 6                       // the longest run that is tried; a choice made here, not a property of the machine

// How the image uses each kind of instruction (kinds.h): the number of
// places that have one, how often those places ran as work, and how often
// in one turn of the idle loop.
static struct { long places; double runs, turn; }* USES;       // indexed by kind

// ---- every pair of instructions, by final state

typedef struct { uint64_t leaves; uint16_t first, second; } two;
static two* TWO;                        // a hash table of pairs, keyed by the hash of the pair's final state
static int two_bits;                    // log2 of its size, which is at least twice the number of pairs

// Returns the slot of the pair whose final state has this hash, or the
// empty slot where that pair would go.
static two* two_at(uint64_t leaves) {
    uint64_t at = (leaves * 0x9e3779b97f4a7c15ull) >> (64 - two_bits);
    while (TWO[at].leaves && TWO[at].leaves != leaves) at = (at + 1) & (((uint64_t)1 << two_bits) - 1);
    return(&TWO[at]);
}

// Returns a pair of instructions whose final state has this hash, or NULL
// if there is none.  The pairs are every two instructions that do not
// return, and every such pair with a return added to the second.  The
// table is built on the first call.
static const two* two_leaving(uint64_t leaves) {
    if (!TWO) {
        uint16_t* firsts = room_for(alus, sizeof(uint16_t));
        int n = 0;
        for(int idx=0; idx < alus; idx++) {
            named m;
            forget();
            if (names(&ALU[idx].ins, 1, &m) && m.next < 0) firsts[n++] = ALU[idx].ins;
        }
        for(two_bits = 1; ((uint64_t)1 << two_bits) < 4 * (uint64_t)n * n; two_bits++);
        TWO = room_for((size_t)1 << two_bits, sizeof(two));
        for(int a=0; a < n; a++) {
            for(int b=0; b < 2 * n; b++) {
                uint16_t pair[2] = { firsts[a], b < n ? firsts[b] : returning(firsts[b - n], 1) };
                named m;
                forget();
                uint64_t is = names(pair, 2, &m);
                if (!is || find(is) >= 0) continue;
                two* at = two_at(is);
                if (!at->leaves) *at = (two){ is, pair[0], pair[1] };
            }
        }
        free(firsts);
    }
    const two* at = two_at(leaves);
    return(at->leaves ? at : NULL);
}

// ---- runs that fewer instructions are equivalent to

typedef struct {
    char* run; char* does;              // the run and its replacement, as text (kinds.h)
    long places; double saved, turn;    // how many places have the run, and the instructions saved in the work and in an idle turn
    long* at; long ats, ats_room;       // the address of each place
} fewer;
static fewer* FEWER;                    // the runs found; the list grows as more are found
static int fewers, fewers_room;

static int by_saved(const void* a, const void* b) {
    double x = ((const fewer*)a)->saved, y = ((const fewer*)b)->saved;
    return(x < y ? 1 : x > y ? -1 : 0);
}

static int by_runs(const void* a, const void* b) {
    double x = USES[*(const int*)a].runs, y = USES[*(const int*)b].runs;
    return(x < y ? 1 : x > y ? -1 : 0);
}

static cut_image IMAGE;

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "usage: survey IMAGE.hex IMAGE.tables [COUNTS [REPORT]]\n");
        return(1);
    }
    const char* report = argc > 4 ? argv[4] : "build/survey.txt";
    machine_from(argv[1]);              // take the cell size from the image before the step is used
    get_ready();
    uint16_t takes = ins_named("r>");
    list_kinds();
    USES = room_for(kinds, sizeof(*USES));
    cut(&IMAGE, argv[1], argv[2]);
    const uint16_t* code = IMAGE.code;
    long n = IMAGE.instructions;

    // The number of times each of the image's instructions ran as work,
    // and in one turn of the idle loop.  It is 0 for an instruction the
    // counts do not cover.
    double* work = room_for(n, sizeof(double));
    double* idle = room_for(n, sizeof(double));
    const char* unit = "places";
    if (argc > 3) {
        double* counts;
        double* a_turn;
        long counted = work_read(argv[3], &counts, &a_turn);
        if (!counted) return(1);
        for(long at=0; at < n && at < counted; at++) work[at] = counts[at], idle[at] = a_turn[at];
        free(counts);
        unit = "runs";
    } else {
        for(long at=0; at < n; at++) work[at] = code[at] ? 1 : 0;
    }

    // Count the instructions the image uses, by kind.
    double total_runs = 0, total_alu = 0, folded = 0, unnamed = 0;
    double read_a_port = 0, stored_to_one = 0;  // the step on terms does not model IO, so these instructions have no kind
    long instructions = 0;
    int used = 0, used_unnamed = 0, with_name = 0;
    for(long at=0; at < n; at++) {
        instruction ins = ins_of(code[at]);
        if (!code[at]) continue;
        instructions++;
        total_runs += work[at];
        if (ins.lit.lit_f || ins.alu.op_type != OP_TYPE_ALU) continue;
        int returned, k = kind_at[unfolded(code[at], &returned)];
        if (k < 0) {
            if (ins.alu.alu_op == ALU_IO_READ) read_a_port += work[at];
            if (ins.alu.out_mux == OUTPUT_IO_T) stored_to_one += work[at];
            continue;
        }
        folded += returned ? work[at] : 0;
        USES[k].places++;
        USES[k].runs += work[at];
        USES[k].turn += idle[at];
        total_alu += work[at];
    }
    for(int k=0; k < kinds; k++) {
        with_name += KIND[k].name != NULL;
        used += USES[k].places > 0;
        used_unnamed += USES[k].places > 0 && !KIND[k].name;
        if (!KIND[k].name) unnamed += USES[k].runs;
    }

    // Find the runs that fewer instructions are equivalent to.  A word
    // whose first instruction is `r>` takes its return address, which is
    // the address of the instruction after the call.  So that instruction
    // must stay where it is, and it is never joined with the instructions
    // after it.
    unsigned char* pinned = room_for(n + 1, 1);
    for(long at=0; at < n; at++) {
        instruction ins = ins_of(code[at]);
        if (!code[at] || ins.lit.lit_f || ins.jmp.op_type != OP_TYPE_CALL) continue;
        long to = goes_to(at, code[at]);
        if (to >= 0 && to < n && code[to] == takes) pinned[at + 1] = 1;
    }
    long places = 0;
    double saved = 0, turn_saved = 0;
    for(long r=0; r < IMAGE.count; r++) {
        const item* it = &IMAGE.items[r];
        if (!it->run) continue;
        for(long i=0; i < it->len; ) {
            int best = 0, best_does = 0;
            uint16_t does[2] = { 0, 0 };
            for(int length = it->len - i < LONGEST ? (int)(it->len - i) : LONGEST; length > 1; length--) {
                const uint16_t* seq = code + it->at + i;
                int is_pinned = 0, made = 0;
                uint16_t with[2] = { 0, 0 };
                for(int j=0; j < length - 1; j++) is_pinned |= pinned[it->at + i + j];
                if (is_pinned) continue;
                named m, one;
                forget();
                uint64_t left = names(seq, length, &m);
                int found = left ? find(left) : -1;
                const two* pair = NULL;
                if (found >= 0 && names(&ALU[found].stands, 1, &one) == left && same_named(&m, &one)) {
                    made = 1, with[0] = ALU[found].stands;
                } else if (left && length > 2 && (pair = two_leaving(left)) != NULL) {
                    made = 2, with[0] = pair->first, with[1] = pair->second;
                } else if ((with[0] = numbered_doing(seq, length)) != 0) {
                    made = 1;                   // one literal instruction: a push, or an add
                } else {
                    continue;
                }
                if (!best || length - made > best - best_does) best = length, best_does = made, does[0] = with[0], does[1] = with[1];
            }
            if (!best) {
                i++;
                continue;
            }
            fewer f = { .run = run_said(code + it->at + i, best, 0), .does = run_said(does, best_does, 0) };
            int at = 0;
            while (at < fewers && (strcmp(FEWER[at].run, f.run) || strcmp(FEWER[at].does, f.does))) at++;
            if (at < fewers) {
                free(f.run);
                free(f.does);
            } else {
                if (fewers == fewers_room) {
                    fewers_room = 2 * fewers_room + kinds;
                    FEWER = realloc(FEWER, fewers_room * sizeof(fewer));
                    if (!FEWER) { fprintf(stderr, "there is no room for %d runs\n", fewers_room); return(1); }
                }
                FEWER[fewers++] = f;
            }
            int gain = best - best_does;
            ONE_MORE(FEWER[at].at, FEWER[at].ats, FEWER[at].ats_room);
            FEWER[at].at[FEWER[at].ats++] = it->at + i;
            FEWER[at].places++;
            FEWER[at].saved += gain * work[it->at + i];
            FEWER[at].turn += gain * idle[it->at + i];
            places++, saved += gain * work[it->at + i], turn_saved += gain * idle[it->at + i];
            i += best;
        }
    }

    FILE* out = fopen(report, "w");
    if (!out) { perror(report); return(1); }
    int printing = 1;
#define SAY(...) do { fprintf(out, __VA_ARGS__); fprintf(out, "\n"); if (printing) { printf(__VA_ARGS__); printf("\n"); } } while (0)
    SAY("%s: %ld instructions", argv[1], instructions);
    SAY("instructions that do different things %d: named %d, the image uses %d, of which %d have no name",
        kinds, with_name, used, used_unnamed);
    if (argc > 3) {
        SAY("instructions run as work: %.0f, of them ALU %.0f, and by those with no name %.0f (%.0f%% of ALU); "
            "%.0f had a return folded in", total_runs, total_alu, unnamed, total_alu ? 100 * unnamed / total_alu : 0, folded);
        SAY("and of the ALU's that are not among those, since they are IO: %.0f read a port and %.0f stored to one",
            read_a_port, stored_to_one);
    }
    SAY("runs of instructions that fewer would do: %ld places, %.0f %s saved (%.1f%% of the work), %.1f a turn of the idle loop",
        places, saved, unit, total_runs ? 100 * saved / total_runs : 0, turn_saved);

    // 1. The space.
    printing = 0;
    SAY("%s", "");
    SAY("1. The space.  For each source and operation, and each destination: instructions that do");
    SAY("   different things / named / used in the image, and %s.", unit);
    SAY("%s", "");
    static struct { int all, named, used; double runs; } cell[INPUTS][OPS][OUTPUTS];
    for(int k=0; k < kinds; k++) {
        instruction ins = ins_of(KIND[k].ins);
        cell[ins.alu.in_mux][ins.alu.alu_op][ins.alu.out_mux].all++;
        cell[ins.alu.in_mux][ins.alu.alu_op][ins.alu.out_mux].named += KIND[k].name != NULL;
        cell[ins.alu.in_mux][ins.alu.alu_op][ins.alu.out_mux].used += USES[k].places > 0;
        cell[ins.alu.in_mux][ins.alu.alu_op][ins.alu.out_mux].runs += USES[k].runs;
    }
    char* row = NULL;               // one row of the table, with a column for each destination
    text_more(&row, "%22s", "");
    for(int m=0; m < OUTPUTS; m++) text_more(&row, "%28s", OUTPUT_MUX_REPR[m]);
    SAY("%s", row);
    for(int i=0; i < INPUTS; i++) {
        for(int o=0; o < OPS; o++) {
            row[0] = '\0';
            text_more(&row, "%-8s %-12s ", INPUT_MUX_REPR[i], ALU_OPS_REPR[o]);
            for(int m=0; m < OUTPUTS; m++) {
                if (cell[i][o][m].all) {
                    text_more(&row, "%6d/%2d/%3d %13.0f", cell[i][o][m].all, cell[i][o][m].named, cell[i][o][m].used,
                              cell[i][o][m].runs);
                } else {
                    text_more(&row, "%28s", "-");
                }
            }
            SAY("%s", row);
        }
        SAY("%s", "");
    }
    free(row);

    // 2. What the image uses.
    printing = 1;
    SAY("2. Every instruction the image uses, by %s.  * has no name.", unit);
    SAY("%s", "");
    SAY("   %6s %14s %7s  instruction", "places", unit, "idle");
    int* order = room_for(kinds, sizeof(int));
    int listed = 0;
    for(int k=0; k < kinds; k++) if (USES[k].places) order[listed++] = k;
    qsort(order, listed, sizeof(int), by_runs);
    for(int idx=0; idx < listed; idx++) {
        const kind* k = &KIND[order[idx]];
        char is[256];
        decode_instruction(is, ins_of(k->ins), FORTH_WORDS);
        printing = idx < 30;
        SAY(" %s %6ld %14.0f %7.1f  %-9s %s", k->name ? " " : "*", USES[order[idx]].places, USES[order[idx]].runs,
            USES[order[idx]].turn, k->name ? k->name : "", is);
    }

    // 3. What could be used.
    printing = 1;
    SAY("%s", "");
    SAY("3. Runs of instructions that fewer would do, by %s saved.", unit);
    SAY("%s", "");
    SAY("   %6s %14s %7s  instructions -> what does the same", "places", "saved", "idle");
    qsort(FEWER, fewers, sizeof(fewer), by_saved);
    for(int idx=0; idx < fewers; idx++) {
        printing = idx < 40;
        SAY("   %6ld %14.0f %7.1f  %s  ->  %s", FEWER[idx].places, FEWER[idx].saved, FEWER[idx].turn, FEWER[idx].run, FEWER[idx].does);
        for(long p=0; p < FEWER[idx].ats; p++) {
            long at = FEWER[idx].at[p];
            fprintf(out, "          $%04lx in %-20s %14.0f %s\n", at, word_at(IMAGE.words, IMAGE.n, at), work[at], unit);
        }
    }

    // 4. Every instruction.
    printing = 0;
    SAY("%s", "");
    SAY("4. Every instruction of the ALU that does no IO, and the one that stands for all that leave what it leaves.");
    SAY("%s", "");
    for(int idx=0; idx < alus; idx++) {
        const kind* k = &KIND[kind_at[ALU[idx].ins]];
        char is[256];
        decode_instruction(is, ins_of(ALU[idx].ins), FORTH_WORDS);
        SAY("   %04x  %s  as %04x %s", ALU[idx].ins, is, k->ins, k->name ? k->name : "");
    }
    fclose(out);
    printf("\nall of it: %s\n", report);
    return(instructions && kinds ? 0 : 1);
}
