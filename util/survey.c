//
// survey IMAGE.hex IMAGE.tables [COUNTS [REPORT]]
//
// The instruction encoding laid out against what a program does with it.
//
// An ALU instruction is fields: where the ALU's input comes from, the
// operation, where the result goes, how each stack moves, and whether it
// returns.  Every setting of them is an instruction the machine executes.
// This says, for all of them and with no opinion about which are useful:
//
//   1. the space: for each source, operation and destination, how many
//      instructions there are that do different things, how many of those
//      have a name, how many the image uses, and how often they run
//   2. every instruction the image uses, by how often it runs
//   3. what is not used and could be: every run of instructions in the
//      image that one instruction, or two, would do the work of, by how
//      many instructions that would save when the program runs.  cross.fs
//      has joined what it joins already: these are what it left
//   4. every instruction, and the one that stands for all that leave what
//      it leaves
//
// What an instruction does, and whether a run does the work of one, is
// asked of the machine: the step on terms (fuse.c), as cross.fs asks it.
// Nothing here says what an instruction does.
//
// COUNTS is how often each instruction ran, as bench/profile.c writes it
// (build/profile/desktop.counts): the desktop's work, and a turn of its
// idle loop.  Without it the counts are of places in the image.  The
// whole report goes to REPORT (build/survey.txt); the top of each part is
// printed.
//

#include "fuse.c"
#include "cut.c"
#include "kinds.c"
#include "../bench/work.c"

#define LONGEST 6                       // instructions in a run that is tried

// What the image does with each kind of instruction (kinds.c): the places
// it has one, how often those ran as work, and in a turn of the idle loop.
static struct { long places; double runs, turn; } USES[8192];

static word_node shown[1024];

// ---- every two instructions, by what they leave

typedef struct { uint64_t leaves; uint16_t first, second; } two;
#define TWOS (1 << 25)
static two* TWO;

// Two instructions that do the work of a run, or nothing: of every two
// that do not write to a port or read one, the second of which may
// return.  The table is made when it is first asked.
static const two* two_leaving(uint64_t leaves) {
    if (!TWO) {
        static uint16_t firsts[4096];
        int n = 0;
        TWO = calloc(TWOS, sizeof(two));
        for(int idx=0; idx < alus; idx++) {
            instruction ins = ins_of(ALU[idx].ins);
            if (!ins.alu.r_eip && ins.alu.alu_op != ALU_IO_READ && ins.alu.out_mux != OUTPUT_IO_T) firsts[n++] = ALU[idx].ins;
        }
        for(int a=0; a < n; a++) {
            for(int b=0; b < 2 * n; b++) {
                uint16_t pair[2] = { firsts[a], b < n ? firsts[b] : returning(firsts[b - n], 1) };
                named m;
                forget();
                uint64_t is = names(pair, 2, &m);
                if (!is || find(is) >= 0) continue;
                uint64_t at = (is * 0x9e3779b97f4a7c15ull) >> 39;
                while (TWO[at].leaves && TWO[at].leaves != is) at = (at + 1) & (TWOS - 1);
                if (!TWO[at].leaves) TWO[at] = (two){ is, pair[0], pair[1] };
            }
        }
    }
    uint64_t at = (leaves * 0x9e3779b97f4a7c15ull) >> 39;
    while (TWO[at].leaves && TWO[at].leaves != leaves) at = (at + 1) & (TWOS - 1);
    return(TWO[at].leaves ? &TWO[at] : NULL);
}

// ---- runs that fewer would do

typedef struct { char run[200], does[120]; long places; double saved, turn; } fewer;
static fewer FEWER[1 << 14];
static int fewers;

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
    get_ready();
    init_opcodes(shown);
    instruction takes[8];               // lookup_word clears as many
    lookup_word(FORTH_WORDS, "r>", takes);
    list_kinds();
    cut(&IMAGE, argv[1], argv[2]);
    const uint16_t* code = IMAGE.code;
    long n = IMAGE.instructions < WORKED ? IMAGE.instructions : WORKED;

    static double work[WORKED], idle[WORKED];
    const char* unit = "places";
    if (argc > 3) {
        if (!work_read(argv[3], work, idle)) return(1);
        unit = "runs";
    } else {
        for(long at=0; at < n; at++) work[at] = code[at] ? 1 : 0;
    }

    // What the image uses.
    double total_runs = 0, total_alu = 0, folded = 0, unnamed = 0;
    long instructions = 0;
    int used = 0, used_unnamed = 0, with_name = 0;
    for(long at=0; at < n; at++) {
        instruction ins = ins_of(code[at]);
        if (!code[at]) continue;
        instructions++;
        total_runs += work[at];
        if (ins.lit.lit_f || ins.alu.op_type != OP_TYPE_ALU) continue;
        int returned, k = kind_at[unfolded(code[at], &returned)];
        if (k < 0) continue;
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

    // What is not used and could be.  The instruction after a call of a
    // word that takes its return address is not joined to what follows it.
    static unsigned char pinned[CUT_PLACES];
    for(long at=0; at < n; at++) {
        instruction ins = ins_of(code[at]);
        if (!code[at] || ins.lit.lit_f || ins.jmp.op_type != OP_TYPE_CALL) continue;
        long to = goes_to(at, code[at]);
        if (to >= 0 && to < n && code[to] == *(uint16_t*)takes) pinned[at + 1] = 1;
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
                    made = 1;                   // one instruction with a number in it: a push, or an add
                } else {
                    continue;
                }
                if (!best || length - made > best - best_does) best = length, best_does = made, does[0] = with[0], does[1] = with[1];
            }
            if (!best) {
                i++;
                continue;
            }
            fewer f = { "", "", 0, 0, 0 };
            say_run(f.run, code + it->at + i, best, 0);
            say_run(f.does, does, best_does, 0);
            int at = 0;
            while (at < fewers && (strcmp(FEWER[at].run, f.run) || strcmp(FEWER[at].does, f.does))) at++;
            if (at == fewers && fewers < (1 << 14)) FEWER[fewers++] = f;
            int gain = best - best_does;
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
    }
    SAY("runs of instructions that fewer would do: %ld places, %.0f %s saved (%.1f%% of the work), %.1f a turn of the idle loop",
        places, saved, unit, total_runs ? 100 * saved / total_runs : 0, turn_saved);

    // 1. The space.
    printing = 0;
    SAY("%s", "");
    SAY("1. The space.  For each source and operation, and each destination: instructions that do");
    SAY("   different things / named / used in the image, and %s.", unit);
    SAY("%s", "");
    static struct { int all, named, used; double runs; } cell[4][16][4];
    for(int k=0; k < kinds; k++) {
        instruction ins = ins_of(KIND[k].ins);
        cell[ins.alu.in_mux][ins.alu.alu_op][ins.alu.out_mux].all++;
        cell[ins.alu.in_mux][ins.alu.alu_op][ins.alu.out_mux].named += KIND[k].name != NULL;
        cell[ins.alu.in_mux][ins.alu.alu_op][ins.alu.out_mux].used += USES[k].places > 0;
        cell[ins.alu.in_mux][ins.alu.alu_op][ins.alu.out_mux].runs += USES[k].runs;
    }
    char row[512];
    snprintf(row, sizeof(row), "%22s", "");
    for(int m=0; m < 4; m++) snprintf(row + strlen(row), sizeof(row) - strlen(row), "%28s", OUTPUT_MUX_REPR[m]);
    SAY("%s", row);
    for(int i=0; i < 4; i++) {
        for(int o=0; o < 16; o++) {
            snprintf(row, sizeof(row), "%-8s %-12s ", INPUT_MUX_REPR[i], ALU_OPS_REPR[o]);
            for(int m=0; m < 4; m++) {
                if (cell[i][o][m].all) {
                    snprintf(row + strlen(row), sizeof(row) - strlen(row), "%6d/%2d/%3d %13.0f", cell[i][o][m].all,
                             cell[i][o][m].named, cell[i][o][m].used, cell[i][o][m].runs);
                } else {
                    snprintf(row + strlen(row), sizeof(row) - strlen(row), "%28s", "-");
                }
            }
            SAY("%s", row);
        }
        SAY("%s", "");
    }

    // 2. What the image uses.
    printing = 1;
    SAY("2. Every instruction the image uses, by %s.  * has no name.", unit);
    SAY("%s", "");
    SAY("   %6s %14s %7s  instruction", "places", unit, "idle");
    static int order[8192];
    int listed = 0;
    for(int k=0; k < kinds; k++) if (USES[k].places) order[listed++] = k;
    qsort(order, listed, sizeof(int), by_runs);
    for(int idx=0; idx < listed; idx++) {
        const kind* k = &KIND[order[idx]];
        char is[256];
        decode_instruction(is, ins_of(k->ins), shown);
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
    }

    // 4. Every instruction.
    printing = 0;
    SAY("%s", "");
    SAY("4. Every instruction of the ALU that does no IO, and the one that stands for all that leave what it leaves.");
    SAY("%s", "");
    for(int idx=0; idx < alus; idx++) {
        const kind* k = &KIND[kind_at[ALU[idx].ins]];
        char is[256];
        decode_instruction(is, ins_of(ALU[idx].ins), shown);
        SAY("   %04x  %s  as %04x %s", ALU[idx].ins, is, k->ins, k->name ? k->name : "");
    }
    fclose(out);
    printf("\nall of it: %s\n", report);
    return(instructions && kinds ? 0 : 1);
}
