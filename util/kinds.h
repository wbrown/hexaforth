//
// kinds.h - the instructions of the ALU by what they leave, and an
// instruction as it is said, for the tools and the tests that have this in
// them.  It has fuse.h in it, and asks the machine by it.
//
// Many settings of an instruction's fields leave the same from every
// state: they are one kind.  Which do is asked of the step on terms
// (fuse.h).  One instruction stands for each kind: one that has a name in
// vm_opcodes.h if any of the kind has, or else the lowest.
//
// An instruction that has a number in it is one of four things, and which
// is asked of the step too: it pushes the number, or adds it to the top,
// or fetches the cell at it (a variable), or stores to that cell.
//
// An instruction is said by the name of its kind's, if that has one, or by
// its fields, as vm_opcodes.h names them; a number by what it pushes or
// adds, and a variable by its cell.  One that has a return folded into it
// (cross.fs, fold-exit) is the instruction and then the return.
//

#ifndef HEXAFORTH_KINDS_H
#define HEXAFORTH_KINDS_H

#include "fuse.h"
#include "room.h"

typedef struct {
    uint16_t ins;                       // the instruction that stands for the kind
    uint64_t leaves;                    // the number for what it leaves (fuse.h)
    const char* name;                   // its name, or none
} kind;

static kind* KIND;                      // as many as there are: no more than the instructions the step says of
static int kinds;
static int kind_at[EVERY_WORD];         // the kind an instruction is of, or less than nothing

// ---- the instructions that have a number in them

enum { NO_NUMBER, PUSHES_IT, ADDS_IT, FETCHES_AT_IT, STORES_AT_IT, DOES_WITH_IT };
typedef struct { uint16_t ins; int64_t number; } numbered;

static numbered* NUMBERED[DOES_WITH_IT];        // those that do each of the four, and how many
static int numbers[DOES_WITH_IT];
static struct { unsigned char does; int64_t number; } NUMBER_IN[EVERY_WORD];    // by instruction

// Every instruction is stepped from the start, and what the step left says
// which of the four it is, if any.
static void list_numbers(void) {
    int found[DOES_WITH_IT] = { 0 };
    memset(NUMBER_IN, 0, sizeof(NUMBER_IN));
    for(int word=0; word < EVERY_WORD; word++) {
        uint16_t x = word;
        if (!((instruction*)&x)->lit.lit_f) continue;   // as vm.c's loop takes one to have a number in it
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
            const term* sum = &KNOWN[m.T];              // the top, and a number, made one
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

// The instruction that does one of the four with a number, if one does.
static const numbered* numbered_as(int does, int64_t number) {
    for(int idx=0; idx < numbers[does]; idx++) if (NUMBERED[does][idx].number == number) return(&NUMBERED[does][idx]);
    return(NULL);
}

// The name vm_opcodes.h has for an instruction, if it is one word's.
static const char* name_of_ins(uint16_t ins) {
    for(word_node* w = FORTH_WORDS; w->repr && strlen(w->repr); w++) {
        if (w->num_ins == 1 && *(uint16_t*)&w->ins[0] == ins) return(w->repr);
    }
    return(NULL);
}

// Does a run do the work of an instruction: does it leave the same terms?
static int same_work(const uint16_t* code, int len, uint16_t ins) {
    named a, b;
    forget();
    uint64_t left = names(code, len, &a);
    return(left && names(&ins, 1, &b) == left && same_named(&a, &b));
}

// The one instruction with a number in it that does the work of a run, or
// nothing: a push of what the run pushes, or an add of what it adds, if
// that leaves what the run leaves.
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

// The kinds there are, of every instruction the step says of; and the
// instructions that have a number in them.
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
        const char* name = name_of_ins(ALU[idx].ins);
        if (k < 0) {
            k = kinds;
            KIND[kinds++] = (kind){ ALU[idx].ins, ALU[idx].names, name };
        } else if (name && !KIND[k].name) {
            KIND[k].ins = ALU[idx].ins, KIND[k].name = name;
        }
        kind_at[ALU[idx].ins] = k;
    }
    forget();
}

// An instruction with a return folded into it, without the return; and
// whether it had one.  The return by itself is not one folded in.
static uint16_t unfolded(uint16_t w, int* returned) {
    uint16_t exits = ins_named("exit");
    instruction leaves = *(instruction*)&exits, ins = *(instruction*)&w;
    *returned = !ins.lit.lit_f && ins.alu.op_type == OP_TYPE_ALU && ins.alu.r_eip &&
                ins.alu.rstack == leaves.alu.rstack && w != exits;
    if (*returned) ins.alu.r_eip = 0, ins.alu.rstack = 0;
    return(*(uint16_t*)&ins);
}

// An instruction as it is said, in the room there is for it: with
// `numbers`, a number and a variable's cell (its address, in hex) are
// said, and without they are only said to be one.
static void say_ins(char* text, size_t room, uint16_t w, int numbers) {
    static const char* const with[] = { NULL, "%lld", "%lld +", "[var %llx @]", "[var %llx !]" };
    static const char* const without[] = { NULL, "lit", "lit+", "var@", "var!" };
    if (((instruction*)&w)->lit.lit_f) {
        int does = NUMBER_IN[w].does;
        if (!does) snprintf(text, room, "(%04x)", w);   // one the step says nothing of, or that does nothing
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

// A run of instructions as it is said, with a space between each: the
// text is made here, as long as it has to be, and is whoever asks's to
// free.
static char* run_said(const uint16_t* seq, int len, int numbers) {
    char* text = room_for(1, 1);
    size_t had = 0;
    for(int idx=0; idx < len; idx++) {
        char one[2 * sizeof(((forth_op*)0)->repr) + 64];        // room for a name or the fields' names, and to spare
        say_ins(one, sizeof(one), seq[idx], numbers);
        text = realloc(text, had + strlen(one) + 2);
        if (!text) { fprintf(stderr, "there is no room for the text of a run\n"); exit(EXIT_FAILURE); }
        had += sprintf(text + had, "%s%s", idx ? " " : "", one);
    }
    return(text);
}

#endif // HEXAFORTH_KINDS_H
