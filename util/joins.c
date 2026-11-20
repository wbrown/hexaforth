//
// joins.c - what the nucleus's peephole (forth/join.fs) is told of the
// machine where a number has just been compiled, asked of the machine and
// written to forth/folds.fs.
//
//   joins forth/folds.fs
//
// The nucleus is on the machine, so what an instruction makes of numbers
// it finds by running the instruction.  What it cannot find that way is
// which instructions may be run: those that make their result of the top
// of the data stack, or of the top two, and touch nothing else.  They are
// listed here, of the instructions that have a name (vm_opcodes.h), since
// those are what is typed.
//
// The other thing it is told is what `dup 16 rshift` is: a dup, something
// pushed, and an instruction, which are the thing pushed and one
// instruction.  Which instruction is a rule by bits: an instruction that
// has these bits so is, after a dup and a push, the push and the
// instruction with these bits changed.  The same is asked of `r@` and of
// `r>` in the dup's place (ACROSS): `r> 8 rshift` is the 8 pushed and a
// shift whose input is the return stack.  A rule is right for every
// instruction it takes, and for whatever is pushed, a number or a
// variable: the step is asked with a number of which nothing is known
// (fuse.h, any_number).  Each rule is then run on vm.c for the named
// instructions it was made for, with a number and with a variable, as
// every answer of fuse.h is.
//
// Nothing here says what an instruction does or what its fields are: it
// is all asked of the step (vm_step.h), which fuse.h has on terms.
//
// Before any of that it holds the names to the machine: an instruction
// that vm_opcodes.h gives a name is to be the one that stands for all
// that leave the same (fuse.h), and the instruction of a phrase is to do
// what the phrase's words do.  It stops, saying which, if one is not.
//
#include "fuse.h"

#define MOST COUNT(FORTH_WORDS)         // there are no more named instructions than vm_opcodes.h has words,
                                        // nor more rules than named instructions
static uint16_t NAMED[MOST];            // the ALU instructions that have a name and do not return,
static const char* NAME_OF[MOST];       // and the name
static int named_ones;

static void list_named(void) {
    for(word_node* w = FORTH_WORDS; w->repr && strlen(w->repr); w++) {
        if (w->num_ins != 1) continue;
        uint16_t ins = *(uint16_t*)&w->ins[0];
        if (alu_at[ins] < 0 || returns(ins)) continue;
        int at = 0;
        while (at < named_ones && NAMED[at] != ins) at++;
        if (at < named_ones) continue;
        NAMED[named_ones] = ins;
        NAME_OF[named_ones++] = w->repr;
    }
}

// Is a term made of nothing but numbers and the cells allowed: the top of
// the data stack as it was (1), and the one under it (2)?
static int made_only_of(int t, int allowed) {
    const term* x = &KNOWN[t];
    if (x->kind == A_NUMBER) return(1);
    if (x->kind == IN_DATA) {
        if (x->number == DEEP - 1) return(allowed & 1);
        if (x->number == DEEP - 2) return((allowed & 2) != 0);
        return(0);
    }
    if (x->kind != BY_ALU) return(0);
    for(int idx=0; idx < x->n; idx++) if (!made_only_of(x->of[idx], allowed)) return(0);
    return(1);
}

// How many numbers an instruction makes one of, touching nothing else: one,
// two, or none if it is not such an instruction.
static int folds(uint16_t ins) {
    named m;
    forget();
    start(&m);
    step_named(&m, ins);
    if (m.unsaid || lost || m.next >= 0 || m.fetches || m.M != MEMORY_TERM) return(0);
    if (m.RSP != DEEP || m.R != RETURN_TERM(DEEP - 1)) return(0);
    for(int idx=0; idx < DEEP - 1; idx++) if (m.RS[idx] != RETURN_TERM(idx)) return(0);
    int of = DEEP + 1 - m.SP;
    if (of != 1 && of != 2) return(0);
    for(int idx=0; idx < m.SP - 1; idx++) if (m.DS[idx] != DATA_TERM(idx)) return(0);
    return(made_only_of(m.T, of == 1 ? 1 : 3) ? of : 0);
}

// Something pushed: a number, and a variable fetched.  With any_number so,
// the number and where the variable is are not known.
static uint16_t a_push(int variable) {
    instruction ins = {0};
    ins.lit.lit_f = true;
    ins.lit.lit_v = 1;
    if (variable) ins.lit.lit_shifts = 3;
    return(*(uint16_t*)&ins);
}

// Is `across`, a push and p the push and y, whatever is pushed?
static int crosses(uint16_t across, uint16_t p, uint16_t y) {
    if (alu_at[y] < 0 || returns(y)) return(0);
    int is = 1;
    any_number = 1;
    for(int variable=0; variable < 2 && is; variable++) {
        uint16_t before[3] = { across, a_push(variable), p }, after[2] = { a_push(variable), y };
        named a, b;
        forget();
        uint64_t left = names(before, 3, &a);
        is = left && names(after, 2, &b) == left && same_named(&a, &b);
    }
    any_number = 0;
    return(is);
}

// The instruction that a push and it are, after `across`, a push and p.
static uint16_t crossed(uint16_t across, uint16_t p) {
    for(int at=0; at < alus; at++) {
        uint16_t y = ALU[at].ins;
        if (ALU[at].stands == y && crosses(across, p, y)) return(y);
    }
    return(0);
}

// The rules are in sets, one for each instruction a push is taken across:
// what puts on top a value that the instruction after the push is of.
typedef struct { uint16_t mask, is, change; } rule;
typedef struct {
    uint16_t across;
    rule rules[MOST];
    int n;
} rule_set;
static const char* const ACROSS[] = { "dup", "r@", "r>" };
static rule_set SET[COUNT(ACROSS)];
static int sets;

// Is an instruction one the peephole may be asked of: of the ALU, and not
// returning?  The step says nothing of some of them (IO), and no rule may
// take one of those.
static int of_alu(uint16_t x) {
    instruction ins = *(instruction*)&x;
    return(!ins.lit.lit_f && ins.alu.op_type == OP_TYPE_ALU && !ins.alu.r_eip);
}

static int right_for_all(uint16_t across, uint16_t mask, uint16_t is, uint16_t change) {
    for(int word=0; word < EVERY_WORD; word++) {
        if (!of_alu(word) || (word & mask) != is) continue;
        if (alu_at[word] < 0 || !crosses(across, word, word ^ change)) return(0);
    }
    return(1);
}

// The rules for the named instructions that cross: each is widened a bit at
// a time for as long as it is right for all it takes.
static void find_rules(rule_set* s) {
    uint16_t across = s->across;
    for(int at=0; at < named_ones; at++) {
        uint16_t p = NAMED[at], y = crossed(across, p);
        if (!y) continue;
        int taken = 0;
        for(int r=0; r < s->n; r++) taken |= (p & s->rules[r].mask) == s->rules[r].is;
        if (taken) continue;
        rule* new = &s->rules[s->n++];
        new->mask = 0xffff; new->change = p ^ y;
        for(int bit=0; bit < 8 * (int)sizeof(instruction); bit++) {
            uint16_t wider = new->mask & ~(1 << bit);
            if (right_for_all(across, wider, p & wider, new->change)) new->mask = wider;
        }
        new->is = p & new->mask;
    }
    // and each is run on vm.c, for the named instructions it takes
    for(int at=0; at < named_ones; at++) {
        uint16_t p = NAMED[at];
        for(int r=0; r < s->n; r++) {
            if ((p & s->rules[r].mask) != s->rules[r].is) continue;
            for(int variable=0; variable < 2; variable++) {
                uint16_t before[3] = { across, a_push(variable), p }, after[2] = { a_push(variable), p ^ s->rules[r].change };
                tried(before, 3, after, 2);
            }
            break;
        }
    }
}

static const char* name_of(uint16_t ins) {
    for(int at=0; at < named_ones; at++) if (NAMED[at] == ins) return(NAME_OF[at]);
    return("");
}

// The words of a phrase as the instructions they are, one after another;
// how many, or nothing if one of them is not a word of the table.
static int phrase_run(const word_node* phrase, uint16_t** run) {
    // no more words than it has characters, and no more instructions to a
    // word than a word has room for
    *run = room_for(strlen(phrase->repr) * COUNT(phrase->ins), sizeof(uint16_t));
    char* said = room_for(strlen(phrase->repr) + 1, 1);
    strcpy(said, phrase->repr);
    int len = 0;
    for(char* word = strtok(said, " "); word; word = strtok(NULL, " ")) {
        const word_node* part = word_named(word);
        if (!part) { len = 0; break; }
        for(int idx=0; idx < part->num_ins; idx++) (*run)[len++] = *(const uint16_t*)&part->ins[idx];
    }
    free(said);
    return(len);
}

// Every name is held to the machine; how many are not as they are to be.
// The instruction a name is given is the one that stands for all that
// leave the same (fuse.h, plainer): what a compiler makes of the words is
// that one, and a name for another would be a second way of saying it.
// And a phrase's instruction does what its words do one after another,
// which is asked of the step and then run on vm.c.
static int names_not_so(void) {
    int not_so = 0;
    for(const word_node* w = FORTH_WORDS; w->repr && strlen(w->repr); w++) {
        uint16_t ins = *(const uint16_t*)&w->ins[0];
        if (is_phrase(w->repr) && (w->num_ins != 1 || alu_at[ins] < 0)) {
            fprintf(stderr, "joins: the phrase `%s` is not one instruction that the step says of\n", w->repr);
            not_so++;
            continue;
        }
        if (w->num_ins != 1 || alu_at[ins] < 0) continue;
        uint16_t stands = ALU[alu_at[ins]].stands;
        if (stands != ins) {
            fprintf(stderr, "joins: %s is %04x in vm_opcodes.h, and what stands for all that do the same is %04x\n",
                    w->repr, ins, stands);
            not_so++;
        }
        if (!is_phrase(w->repr)) continue;
        uint16_t* run;
        int len = phrase_run(w, &run);
        named a, b;
        forget();
        uint64_t left = len ? names(run, len, &a) : 0;
        if (!left || names(&ins, 1, &b) != left || !same_named(&a, &b)) {
            fprintf(stderr, "joins: the phrase `%s` is %04x in vm_opcodes.h, which is not what its words do\n", w->repr, ins);
            not_so++;
        } else {
            tried(run, len, &ins, 1);
        }
        free(run);
    }
    return(not_so);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "usage: joins forth/folds.fs\n");
        return(1);
    }
    get_ready();
    list_named();
    if (names_not_so()) return(1);
    int rules = 0;                      // in all the sets
    for(int at=0; at < COUNT(ACROSS); at++) {
        rule_set* s = &SET[sets];
        s->across = ins_named(ACROSS[at]);
        if (!s->across) {
            fprintf(stderr, "joins: %s is not one instruction\n", ACROSS[at]);
            return(1);
        }
        find_rules(s);
        if (!s->n) continue;            // nothing that is typed is taken across it
        rules += s->n;
        sets++;
    }

    uint16_t of_one[MOST], of_two[MOST];
    int ones = 0, twos = 0;
    for(int at=0; at < named_ones; at++) {
        int of = folds(NAMED[at]);
        if (of == 1) of_one[ones++] = NAMED[at];
        if (of == 2) of_two[twos++] = NAMED[at];
    }
    if (!ones || !twos || !rules) {
        fprintf(stderr, "joins: found %d instructions that make one number of one, %d of two, and %d rules: there are some of each\n",
                ones, twos, rules);
        return(1);
    }

    // The file is source that goes on a disk, laid out as doc/formatting.md
    // says: lines of DISK_LINE characters at most, and the table cut with a
    // line of nothing, between two of its entries, before it is longer than
    // a block's lines.
    char* made = NULL;                  // the file
    size_t made_len = 0;
    FILE* out = open_memstream(&made, &made_len);
    if (!out) { perror(argv[1]); return(1); }
    fprintf(out, "\\ folds.fs - what the nucleus's peephole (join.fs) is told of\n"
                 "\\ the machine, where a number has just been compiled.\n"
                 "\\ GENERATED by `joins forth/folds.fs` (util/joins.c) from the\n"
                 "\\ machine.  Do not edit.\n\n"
                 "\\ They are instructions laid down after the peephole's code, all\n"
                 "\\ on one page, and folds-at, in the kernel, is told where.\n"
                 "\\ First how many there are of each of three things: the\n"
                 "\\ instructions that make one number of the top of the stack\n"
                 "\\ and touch nothing else, those that make one of the top two,\n"
                 "\\ and the sets of rules.  Then the instructions themselves;\n"
                 "\\ then each set: the instruction a push is taken across, how\n"
                 "\\ many rules it has, and the rules: a mask, what an\n"
                 "\\ instruction has under it, and the bits that are changed.\n\n");
    int run = 0;                        // lines of the table since a line of nothing
    #define TABLE_LINE() do { if (run == DISK_BLOCK / DISK_LINE) { fprintf(out, "\n"); run = 0; } run++; } while (0)
    TABLE_LINE();
    fprintf(out, "meta $%x table-room  tcp @ 2/ t' folds-at tvar! target\n", 3 + ones + twos + 2 * sets + 3 * rules);
    TABLE_LINE();
    fprintf(out, "$%04x i, $%04x i, $%04x i,   \\ how many of each\n", ones, twos, sets);
    for(int at=0; at < ones; at++) {
        TABLE_LINE();
        fprintf(out, "$%04x i,   \\ %s\n", of_one[at], name_of(of_one[at]));
    }
    for(int at=0; at < twos; at++) {
        TABLE_LINE();
        fprintf(out, "$%04x i,   \\ %s\n", of_two[at], name_of(of_two[at]));
    }
    for(int set=0; set < sets; set++) {
        const rule_set* s = &SET[set];
        TABLE_LINE();
        fprintf(out, "$%04x i, $%04x i,   \\ across %s\n", s->across, s->n, name_of(s->across));
        for(int r=0; r < s->n; r++) {
            TABLE_LINE();
            int len = fprintf(out, "$%04x i, $%04x i, $%04x i,   \\", s->rules[r].mask, s->rules[r].is, s->rules[r].change);
            int column = len - 1;       // the \ comment's, which a line that goes on with it has too
            for(int at=0; at < named_ones; at++) {
                if ((NAMED[at] & s->rules[r].mask) != s->rules[r].is) continue;
                if (len + 1 + (int)strlen(NAME_OF[at]) > DISK_LINE) {
                    fprintf(out, "\n");
                    TABLE_LINE();
                    len = fprintf(out, "%*s\\", column, "");
                }
                len += fprintf(out, " %s", NAME_OF[at]);
            }
            fprintf(out, "\n");
        }
    }
    fclose(out);
    if (!write_whole(argv[1], made, made_len)) { perror(argv[1]); return(1); }
    free(made);
    printf("%d instructions that make one number of one and %d of two, and %d rules in %d sets for what follows",
           ones, twos, rules, sets);
    for(int set=0; set < sets; set++) printf("%s %s", set ? "," : "", name_of(SET[set].across));
    printf(" and a push, written to %s\n", argv[1]);
    return(0);
}
