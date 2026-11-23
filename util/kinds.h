//
// kinds.h - groups the ALU instructions by their effect, and formats an
// instruction as text, for the tools and tests that include it.  It
// includes fuse.h and asks the machine through it.
//
// Many settings of an instruction's fields have the same effect from
// every state.  Those instructions are one kind.  The step on terms
// decides which they are (fuse.h).  Each kind has a representative, the
// one fuse.h chooses (plainer()).  A name in vm_opcodes.h belongs to a
// representative, which util/joins.c checks.
//
// An instruction with a number in it does one of four things, which the
// step also decides: it pushes the number, adds it to the top, fetches the
// cell at that address (a variable), or stores to that cell.
//
// As text, an ALU instruction is the name of its kind's representative if
// that has a name, and otherwise its fields, as vm_opcodes.h names them.
// A literal is shown as the number it pushes or adds, and a variable
// instruction as its cell's address.  An instruction with a return folded
// into it (cross.fs, fold-exit) is shown as the instruction followed by
// the return.
//

#ifndef HEXAFORTH_KINDS_H
#define HEXAFORTH_KINDS_H

#include "fuse.h"
#include "room.h"

typedef struct {
    uint16_t ins;                       // the kind's representative
    uint64_t leaves;                    // the hash of its final state (fuse.h)
    const char* name;                   // its name, or NULL
} kind;

static kind* KIND;                      // one entry per kind: at most one per ALU instruction
static int kinds;
static int kind_at[EVERY_WORD];         // the index of an instruction's kind, or -1

// ---- the instructions that have a number in them

enum { NO_NUMBER, PUSHES_IT, ADDS_IT, FETCHES_AT_IT, STORES_AT_IT, DOES_WITH_IT };
typedef struct { uint16_t ins; int64_t number; } numbered;

static numbered* NUMBERED[DOES_WITH_IT];        // the instructions that do each of the four things
static int numbers[DOES_WITH_IT];               // the number of instructions in each list
static struct { unsigned char does; int64_t number; } NUMBER_IN[EVERY_WORD];    // what each instruction does with its number

// Fills NUMBERED and NUMBER_IN.  Every literal instruction is stepped from
// the starting state, and its final state shows which of the four things
// it does, if any.
static void list_numbers(void) {
    int found[DOES_WITH_IT] = { 0 };
    memset(NUMBER_IN, 0, sizeof(NUMBER_IN));
    for(int word=0; word < EVERY_WORD; word++) {
        uint16_t x = word;
        if (!((instruction*)&x)->lit.lit_f) continue;   // vm.c's loop decodes only these as literals
        named m, none;
        forget();
        start(&none);
        start(&m);
        int does = NO_NUMBER, number = -1;
        step_named(&m, x);
        if (m.unsaid || lost) continue;
        const term* mem = &KNOWN[m.M];
        if (m.fetches == 1 && KNOWN[m.fetched[0]].kind == A_NUMBER && m.SP == none.SP + 1) {
            does = FETCHES_AT_IT, number = m.fetched[0];
        } else if (mem->kind == STORED && KNOWN[mem->of[1]].kind == A_NUMBER && m.SP == none.SP - 1) {
            does = STORES_AT_IT, number = mem->of[1];
        } else if (m.SP == none.SP + 1 && KNOWN[m.T].kind == A_NUMBER && !m.fetches && m.M == none.M) {
            does = PUSHES_IT, number = m.T;
        } else if (m.SP == none.SP && m.T != none.T && KNOWN[m.T].kind == BY_ALU && KNOWN[m.T].n == 2) {
            const term* sum = &KNOWN[m.T];              // the old top combined with a number
            int other = sum->of[0] == none.T ? sum->of[1] : sum->of[0];
            if ((sum->of[0] == none.T || sum->of[1] == none.T) && KNOWN[other].kind == A_NUMBER) does = ADDS_IT, number = other;
        }
        if (!does) continue;
        NUMBER_IN[x].does = does;
        NUMBER_IN[x].number = KNOWN[number].number;
        found[does]++;
    }
    for(int does=NO_NUMBER + 1; does < DOES_WITH_IT; does++) {
        free(NUMBERED[does]);
        NUMBERED[does] = room_for(found[does], sizeof(numbered));
        numbers[does] = 0;
    }
    for(int word=0; word < EVERY_WORD; word++) {
        int does = NUMBER_IN[word].does;
        if (does) NUMBERED[does][numbers[does]++] = (numbered){ word, NUMBER_IN[word].number };
    }
    forget();
}

// Returns the instruction that does one of the four things with a given
// number, or NULL if there is none.
static const numbered* numbered_as(int does, int64_t number) {
    for(int idx=0; idx < numbers[does]; idx++) if (NUMBERED[does][idx].number == number) return(&NUMBERED[does][idx]);
    return(NULL);
}

// Returns the name vm_opcodes.h gives an instruction, which is a word or
// the words of a phrase, or NULL if it has none.
static const char* name_of_ins(uint16_t ins) {
    for(word_node* w = FORTH_WORDS; w->repr && strlen(w->repr); w++) {
        if (w->num_ins == 1 && *(uint16_t*)&w->ins[0] == ins) return(w->repr);
    }
    return(NULL);
}

// Returns 1 if a run is equivalent to a single instruction: both end with
// the same terms.
static int same_work(const uint16_t* code, int len, uint16_t ins) {
    named a, b;
    forget();
    uint64_t left = names(code, len, &a);
    return(left && names(&ins, 1, &b) == left && same_named(&a, &b));
}

// Returns the single literal instruction equivalent to a run, or 0 if
// there is none: a push of the number the run pushes, or an add of the
// number it adds, if that instruction ends with the same terms as the run.
static uint16_t numbered_doing(const uint16_t* seq, int len) {
    named m, none;
    const numbered* is = NULL;
    forget();
    if (!names(seq, len, &m)) return(0);
    start(&none);
    if (m.SP == none.SP + 1 && KNOWN[m.T].kind == A_NUMBER) {
        is = numbered_as(PUSHES_IT, KNOWN[m.T].number);
    } else if (m.SP == none.SP && KNOWN[m.T].kind == BY_ALU && KNOWN[m.T].n == 2) {
        const term* sum = &KNOWN[m.T];
        int other = sum->of[0] == none.T ? sum->of[1] : sum->of[0];
        if (KNOWN[other].kind == A_NUMBER) is = numbered_as(ADDS_IT, KNOWN[other].number);
    }
    return(is && same_work(seq, len, is->ins) ? is->ins : 0);
}

// Fills KIND and kind_at for every ALU instruction the step handles, and
// lists the literal instructions (list_numbers()).
static void list_kinds(void) {
    list_numbers();
    memset(kind_at, -1, sizeof(kind_at));
    free(KIND);
    KIND = room_for(alus, sizeof(kind));
    kinds = 0;
    for(int idx=0; idx < alus; idx++) {
        int k = -1;
        for(int n=0; n < kinds && k < 0; n++) {
            if (KIND[n].leaves == ALU[idx].names && same_work(&ALU[idx].ins, 1, KIND[n].ins)) k = n;
        }
        if (k < 0) {
            // the representative of the instructions with this effect (fuse.h)
            uint16_t stands = ALU[idx].stands;
            if (!same_work(&ALU[idx].ins, 1, stands)) stands = ALU[idx].ins;
            k = kinds;
            KIND[kinds++] = (kind){ stands, ALU[idx].names, name_of_ins(stands) };
        }
        kind_at[ALU[idx].ins] = k;
    }
    forget();
}

// Returns an instruction without the return folded into it, and sets
// `*returned` to 1 if it had one.  A plain `exit` is not a folded return.
static uint16_t unfolded(uint16_t w, int* returned) {
    uint16_t exits = ins_named("exit");
    instruction leaves = *(instruction*)&exits, ins = *(instruction*)&w;
    *returned = !ins.lit.lit_f && ins.alu.op_type == OP_TYPE_ALU && ins.alu.r_eip &&
                ins.alu.rstack == leaves.alu.rstack && w != exits;
    if (*returned) ins.alu.r_eip = 0, ins.alu.rstack = 0;
    return(*(uint16_t*)&ins);
}

// Formats an instruction as text into `text`, which has `room` bytes.
// With `numbers` set, a literal's number and a variable's address (in hex)
// are printed.  Without it, only the kind of literal is printed.
static void say_ins(char* text, size_t room, uint16_t w, int numbers) {
    static const char* const with[] = { NULL, "%lld", "%lld +", "[var %llx @]", "[var %llx !]" };
    static const char* const without[] = { NULL, "lit", "lit+", "var@", "var!" };
    if (((instruction*)&w)->lit.lit_f) {
        int does = NUMBER_IN[w].does;
        if (!does) snprintf(text, room, "(%04x)", w);   // a literal the step does not handle, or one that does nothing
        else if (numbers) snprintf(text, room, with[does], (long long)NUMBER_IN[w].number);
        else snprintf(text, room, "%s", without[does]);
        return;
    }
    int returned;
    uint16_t base = unfolded(w, &returned);
    int k = kind_at[base];
    if (k >= 0 && KIND[k].name) {
        snprintf(text, room, "%s%s", KIND[k].name, returned ? ";" : "");
    } else {
        instruction is = *(instruction*)&base;
        snprintf(text, room, "(%s %s %s d%+d r%+d)%s", INPUT_MUX_REPR[is.alu.in_mux], ALU_OPS_REPR[is.alu.alu_op],
                 OUTPUT_MUX_REPR[is.alu.out_mux], is.alu.dstack, is.alu.rstack, returned ? ";" : "");
    }
}

// Formats a run of instructions as text, separated by spaces.  Returns an
// allocated string, which the caller frees.
static char* run_said(const uint16_t* seq, int len, int numbers) {
    char* text = room_for(1, 1);
    size_t had = 0;
    for(int idx=0; idx < len; idx++) {
        char one[2 * sizeof(((forth_op*)0)->repr) + 64];        // room for a name or for the fields' names, with space to spare
        say_ins(one, sizeof(one), seq[idx], numbers);
        text = realloc(text, had + strlen(one) + 2);
        if (!text) { fprintf(stderr, "there is no room for the text of a run\n"); exit(EXIT_FAILURE); }
        had += sprintf(text + had, "%s%s", idx ? " " : "", one);
    }
    return(text);
}

#endif // HEXAFORTH_KINDS_H
