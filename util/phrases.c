//
// phrases [REPORT]
//
// Writes a phrase book: what to write in Forth to get each instruction.
//
// An instruction is a control word.  Few have a name.  The rest are
// written as a phrase of words that the peephole joins: `over +` is one
// instruction, and so is `over @ swap !`.  For every ALU instruction that
// is equivalent to some run of up to three of the machine's words, this
// finds the shortest such run, which is the phrase to write.  (LONGEST=4
// in the environment searches runs of four words, which takes much
// longer.)  Where other runs of the same length are equivalent too, it
// lists a few of them beside the first, if they have three words or fewer.
//
// The machine decides whether a run of words is equivalent to an
// instruction: the step on terms gives the final state of each (fuse.h),
// and the two are equivalent if the states are the same.  This file does
// not encode what any instruction does.  Each phrase is then run on vm.c
// beside its instruction (fuse.h, tried()), and the program stops if the
// two end in different states there.
//
// It then checks two things.
//
// Does cross.fs compile each phrase to one instruction?  It compiles
// every phrase as a definition of its own.  A phrase that comes out longer
// is a case the peephole does not handle.
//
// Does each name mean what the standard says?  The machine's words that
// use only the data stack are run on vm.c and in gforth with the same
// stacks, to check that a name does here what it does there.  Words that
// ANS Forth, gforth and older Forths have, and that this machine has no
// name for, are run in gforth to find which of them are a single
// instruction here.
//
// Run it from forth/, because cross.fs runs from there.  The report goes
// to ../build/phrases.txt unless another file is named, and the first
// part of it is also printed.
//

#include "fuse.h"
#include "kinds.h"
#include "image.h"

// The limits of the search and the sizes of the tests.  These are choices
// made here, not properties of the machine.
#define MOST    4                       // the most words in a phrase
#define WAYS    4                       // the most phrases listed for one instruction
#define ALL     3                       // the alternatives are found by trying every run of up to this many words
#define LONG    48                      // the most instructions in a phrase
#define HELD    16                      // instructions of space reserved for each phrase when it is compiled
#define BATCH   200                     // phrases compiled in one image
#define FIRST_AT 0x1000                 // the instruction address of the first phrase of a batch, which is after main
#define MAIN_AT (FIRST_AT / 2)          // the instruction address of main
#define BY_CHANCE 24                    // test stacks with random contents
#define TOP_TWO   6                     // test stacks whose top two cells are equal
#define TOP_THIRD 6                     // test stacks whose top and third cells are equal
#define STACKS  (BY_CHANCE + TOP_TWO + TOP_THIRD)

// A word of vm_opcodes.h that a phrase can contain.  The array sizes are
// upper bounds taken from vm_opcodes.h: its number of words, and the most
// instructions a word there can have.
typedef struct {
    const char* name;
    uint16_t code[COUNT(((word_node*)0)->ins)];
    int len;
    int stack_only;                     // 1 if the step shows it uses only the data stack
} vocable;
static vocable TYPED[COUNT(FORTH_WORDS)];
static int vocab;

// The results for one kind of instruction (kinds.h).  There is an entry
// for each kind that does not return.
typedef struct {
    const kind* is;
    int phrase[WAYS][MOST];             // its phrases, as indexes into TYPED: the first found, then others of the same length
    int said, ways;                     // the number of words in each phrase (0 if it has none), and the number of phrases
    int compiled;                       // how many instructions cross.fs compiled the first phrase to, or 0 if it was not compiled
    int stack_only;
    int64_t result[STACKS][FILLED];     // the data stack it leaves from each test stack, run on vm.c
    int depth[STACKS];                  // the depth of each of those
} entry;
static entry* ENTRY;                    // at most one for each kind
static int entries;
static entry** entry_of;                // the entry for each kind, by the kind's index

// Returns the entry for the kind of instruction whose final state has
// this hash, or NULL if there is none.
static entry* entry_leaving(uint64_t leaves) {
    int found = find(leaves);
    int k = found >= 0 ? kind_at[ALU[found].ins] : -1;
    return(k >= 0 ? entry_of[k] : NULL);
}

// Returns 1 if a kind of instruction has a name: vm_opcodes.h has it as a
// word, and not only as a phrase.
static int has_name(const kind* k) {
    return(k->name && !is_phrase(k->name));
}

// Returns 1 if the step shows that a run uses only the data stack: it
// leaves the return stack and memory unchanged, fetches nothing, and does
// not return.
static int only_data(const uint16_t* code, int len) {
    named m, none;
    forget();
    if (!names(code, len, &m)) return(0);
    start(&none);
    return(m.RSP == none.RSP && m.R == none.R && m.M == none.M && !m.fetches && m.next < 0 &&
           !memcmp(m.RS, none.RS, sizeof(m.RS)));
}

// ---- the phrases

typedef struct { int phrase[MOST]; int n; uint16_t code[LONG]; int len; } reached;

static number_set REACHED;              // the final states reached so far, by hash (room.h)

// Finds the shortest phrase for every instruction that some run of up to
// `longest` words is equivalent to.  It searches breadth first: the runs
// of one word, then each run that reached a new final state extended by
// one more word.  Runs of `longest` words are not extended.
static void find_phrases(int longest) {
    reached* frontier = calloc(1, sizeof(reached));
    long fronts = 1;
    int found = 0;
    for(int depth=1; depth <= longest; depth++) {
        reached* next = depth < longest ? malloc(sizeof(reached) * (fronts * vocab + 1)) : NULL;
        long nexts = 0;
        for(long f=0; f < fronts; f++) {
            for(int w=0; w < vocab; w++) {
                reached r = frontier[f];
                if (r.len + TYPED[w].len > LONG) continue;
                r.phrase[r.n++] = w;
                memcpy(r.code + r.len, TYPED[w].code, TYPED[w].len * sizeof(uint16_t));
                r.len += TYPED[w].len;
                named m;
                forget();
                uint64_t leaves = names(r.code, r.len, &m);
                if (!leaves) continue;
                entry* k = entry_leaving(leaves);
                if (k && !k->said && same_work(r.code, r.len, k->is->ins)) {
                    tried(r.code, r.len, &k->is->ins, 1);
                    memcpy(k->phrase[0], r.phrase, sizeof(r.phrase));
                    k->said = r.n;
                    k->ways = 1;
                    found++;
                }
                if (depth < longest && !already_in(&REACHED, leaves)) next[nexts++] = r;
            }
        }
        free(frontier);
        frontier = next;
        fronts = nexts;
        printf("  %d words: %5d instructions have a phrase\n", depth, found);
        fflush(stdout);
    }
}

// Finds the other phrases that are as short as the first one found for an
// instruction.  It tries every run of up to `longest` words, because
// find_phrases() kept only the runs that reached a new final state.  Each
// phrase found is run on vm.c beside its instruction, as the first was.
static void find_others(int longest) {
    int at[MOST] = { 0 };
    for(int words=1; words <= longest; ) {
        uint16_t code[LONG];
        int len = 0;
        for(int n=0; n < words && len >= 0; n++) {
            if (len + TYPED[at[n]].len > LONG) { len = -1; break; }
            memcpy(code + len, TYPED[at[n]].code, TYPED[at[n]].len * sizeof(uint16_t));
            len += TYPED[at[n]].len;
        }
        named m;
        forget();
        uint64_t leaves = len > 0 ? names(code, len, &m) : 0;
        entry* k = leaves ? entry_leaving(leaves) : NULL;
        if (k && k->said == words && k->ways < WAYS && memcmp(k->phrase[0], at, words * sizeof(int)) &&
            same_work(code, len, k->is->ins)) {
            tried(code, len, &k->is->ins, 1);
            memcpy(k->phrase[k->ways++], at, sizeof(at));
        }
        int n = words - 1;                              // advance to the next run of this length, or to the first run one word longer
        while (n >= 0 && ++at[n] == vocab) at[n--] = 0;
        if (n < 0) words++;
    }
}

// Buffer sizes for a phrase as text.  A phrase is at most MOST names with
// spaces between them, and a name is at most as long as vm_opcodes.h allows.
// All of an instruction's phrases together are at most WAYS of those, with
// a separator between them.
#define PHRASE_ROOM  (MOST * sizeof(((forth_op*)0)->repr) + sizeof("(no phrase)"))
#define PHRASES_ROOM (WAYS * (PHRASE_ROOM + sizeof("  |  ")))

static void say_way(char* text, const entry* k, int way) {
    for(int n=0; n < k->said; n++) {
        if (n) strcat(text, " ");
        strcat(text, TYPED[k->phrase[way][n]].name);
    }
}

// Writes an instruction's first phrase to `text`.
static void say_phrase(char* text, const entry* k) {
    text[0] = '\0';
    say_way(text, k, 0);
    if (!k->said) strcpy(text, "(no phrase)");
}

// Writes all of an instruction's phrases to `text`.
static void say_phrases(char* text, const entry* k) {
    text[0] = '\0';
    for(int way=0; way < k->ways; way++) {
        if (way) strcat(text, "  |  ");
        say_way(text, k, way);
    }
}

// ---- each phrase, compiled by cross.fs

// Compiles a batch of phrases with cross.fs, each as a definition at an
// address with HELD instructions reserved for it, and records how many
// instructions each phrase compiled to.  A separate `exit` at the end,
// which the peephole could not fold into the phrase, is not counted.
static void compile(entry** batch, int count) {
    uint16_t leaves = ins_named("exit");
    const int each = sizeof(instruction);       // cross.fs's `org` takes a byte address
    FILE* source = fopen("phrasetest.fs", "w");
    if (!source) { perror("phrasetest.fs"); exit(1); }
    fprintf(source, "meta 4 org target\nheader execute : execute >r ;\n");
    for(int n=0; n < count; n++) {
        char phrase[PHRASE_ROOM];
        say_phrase(phrase, batch[n]);
        fprintf(source, "meta $%x org target : p%d %s ;\n", each * (FIRST_AT + HELD * n), n, phrase);
    }
    fprintf(source, "meta $%x org target\nheader main : main halt ;\nheader quit : quit halt ;\n", each * MAIN_AT);
    fclose(source);
    // GFORTH is the path to gforth, which the build defines.
    int built = !system(GFORTH " ./cross.fs ./basewords.fs ./phrasetest.fs > ../build/phrasetest.log 2>&1");
    remove("phrasetest.fs");
    uint8_t* image;
    size_t len = built ? load_image("../build/phrasetest.hex", &image) : 0;
    if (len / each < (size_t)FIRST_AT + HELD * count) {
        printf("    a batch of phrases did not build: see build/phrasetest.log\n");
        return;
    }
    const uint16_t* code = (const uint16_t*)image;
    for(int n=0; n < count; n++) {
        int size = 0, last = 0;
        for(int idx=0; idx < HELD; idx++) {
            if (code[FIRST_AT + HELD * n + idx]) size++, last = code[FIRST_AT + HELD * n + idx];
        }
        if (size && last == leaves) size--;
        batch[n]->compiled = size ? size : 1;
    }
    free(image);
}

// ---- names, against gforth

// The stacks a word is tested on.  They are filled from a small set of
// numbers, so that two cells of a stack are often equal, and 0 and
// negative numbers are often on top.
static int64_t TRY[STACKS][DEPTH];

static void make_stacks(void) {
    static const int64_t FEW[] = { 0, 1, -1, 2, 3, -7, 5, 62, 1ll << 40, -(1ll << 62) };
    uint64_t seed = 1984;
    for(int s=0; s < STACKS; s++) {
        for(int idx=0; idx < DEPTH; idx++) {
            seed ^= seed << 13, seed ^= seed >> 7, seed ^= seed << 17;
            TRY[s][idx] = FEW[(seed >> 32) % COUNT(FEW)];
        }
        if (s >= BY_CHANCE && s < BY_CHANCE + TOP_TWO) TRY[s][DEPTH - 1] = TRY[s][DEPTH - 2];  // the top two cells equal
        if (s >= BY_CHANCE + TOP_TWO) TRY[s][DEPTH - 1] = TRY[s][DEPTH - 3];                   // the top and third cells equal
    }
}

// Runs a run on vm.c from each test stack, and stores the data stack it
// leaves in `result` and its depth in `depth`.  Returns 1 if it ran to its
// end from every test stack and left the same data stack whatever was on
// the return stack.  A run that fails that test does not use only the
// data stack, even if it leaves the return stack unchanged.
static int on_machine(const uint16_t* code, int len, int64_t result[STACKS][FILLED], int* depth) {
    lay(0, code, len);
    for(int s=0; s < STACKS; s++) {
        for(int other=0; other < 3; other++) {
            state from = STATE[0];
            memset(from.dstack, 0, sizeof(from.dstack));
            memcpy(from.dstack, TRY[s], sizeof(TRY[s]));
            if (other) from.rstack[DEPTH - 1] = TRY[s][DEPTH - other];  // set the top of the return stack to the data stack's top cell, or the cell under it
            if (!run(0, len, &from) || machine.SP < 0 || machine.SP > FILLED) return(0);
            if (other && (machine.SP != depth[s] || memcmp(result[s], machine.DSTACK, machine.SP * sizeof(int64_t)))) return(0);
            depth[s] = machine.SP;
            memcpy(result[s], machine.DSTACK, machine.SP * sizeof(int64_t));
        }
    }
    return(1);
}

// Returns 1 if gforth has a word.  gforth is run once for each word, and
// only looks the name up.  It is never made to execute a word it lacks:
// this program does not wait on whatever gforth does after that error.
static int gforth_has(const char* name) {
    FILE* script = fopen("../build/phrases.ask", "w");
    if (!script) { perror("../build/phrases.ask"); exit(1); }
    fprintf(script, ": t parse-name find-name 0<> . cr ;\nt %s\nbye\n", name);
    fclose(script);
    FILE* answer = popen(GFORTH " ../build/phrases.ask < /dev/null", "r");
    int has = 0;
    if (!answer || fscanf(answer, "%d", &has) != 1) {
        fprintf(stderr, "phrases: gforth did not say whether it has %s\n", name);
        exit(1);
    }
    pclose(answer);
    return(has != 0);
}

// Runs some text in gforth from each test stack, and stores the data
// stack it leaves in `result` and its depth in `depth`.  The text must be
// words gforth has.  Returns 0 if gforth's output could not be read.
static int in_gforth(const char* text, int64_t result[STACKS][FILLED], int* depth) {
    FILE* script = fopen("../build/phrases.ask", "w");
    if (!script) { perror("../build/phrases.ask"); exit(1); }
    fprintf(script, "%d maxdepth-.s !\n", FILLED);      // make .s print the whole stack, not only its top few cells
    for(int s=0; s < STACKS; s++) {
        for(int idx=0; idx < DEPTH; idx++) fprintf(script, "%lld ", (long long)TRY[s][idx]);
        fprintf(script, "%s .s clearstack cr\n", text);
    }
    fprintf(script, "bye\n");
    fclose(script);
    FILE* answers = popen(GFORTH " ../build/phrases.ask < /dev/null 2>&1", "r");
    char* row = NULL;               // one line of gforth's output, of any length
    long row_room = 0;
    int s = 0, known = answers != NULL;
    while (known && line_of(answers, &row, &row_room)) {
        int deep = 0, used = 0;
        if (sscanf(row, " <%d>%n", &deep, &used) != 1 || deep > FILLED || s == STACKS) { known = 0; break; }
        const char* at = row + used;
        for(int idx=0; idx < deep; idx++) {
            char* end;
            result[s][idx] = strtoll(at, &end, 10);     // gforth prints a cell as a signed number
            at = end;
        }
        depth[s++] = deep;
    }
    if (answers) pclose(answers);
    free(row);
    remove("../build/phrases.ask");
    return(known && s == STACKS);
}

// Returns 1 if two sets of results are the same.  The bottom cell of each
// stack is not compared: the lead-in that sets the machine's stack depths
// before a run (fuse.h, lay_leads()) leaves a number of its own there, and
// no word tested here reaches that far down.
static int same_results(int64_t a[STACKS][FILLED], const int* da, int64_t b[STACKS][FILLED], const int* db) {
    for(int s=0; s < STACKS; s++) {
        if (da[s] != db[s] || da[s] < 1 || memcmp(a[s] + 1, b[s] + 1, (da[s] - 1) * sizeof(int64_t))) return(0);
    }
    return(1);
}

// Words that other Forths have and that use only the data stack.  A word
// that gforth has is run in gforth by its name.  For a word gforth lacks,
// `is` defines in gforth's words what the word does in the Forth it comes
// from, and that definition is run.
static const struct { const char* name; const char* is; } OTHERS[] = {
    // ANS Forth and gforth
    {"third"}, {"fourth"}, {"under+"}, {"2nip"}, {"2tuck"}, {"2rot"}, {"0<="}, {"0>="}, {"<="}, {">="},
    {"u<="}, {"u>="}, {"umin"}, {"umax"}, {"min"}, {"max"}, {"abs"}, {"within"}, {"cell+"}, {"cell-"},
    {"cells"}, {"char+"}, {"chars"}, {"d>s"}, {"s>d"}, {"2*"}, {"2/"}, {"1+"}, {"1-"}, {"true"}, {"false"},
    {"bounds"}, {"-rot"}, {"nip"}, {"tuck"}, {"2dup"}, {"2drop"}, {"2swap"}, {"2over"}, {"rot"}, {"negate"},
    {"invert"}, {"0="}, {"0<"}, {"0>"}, {"0<>"}, {"="}, {"<>"}, {"<"}, {">"}, {"u<"}, {"u>"}, {"+"}, {"-"},
    {"*"}, {"and"}, {"or"}, {"xor"}, {"lshift"}, {"rshift"}, {"dup"}, {"drop"}, {"swap"}, {"over"},
    // fig-Forth, F83 and others, each defined in words gforth has
    {"2+", "2 +"}, {"2-", "2 -"}, {"not", "0="}, {"minus", "negate"}, {"-dup"}, {"swap-", "swap -"},
    {"over-", "over -"}, {"tuck+", "tuck +"}, {"under", "swap over"}, {"nand", "and invert"},
    {"nor", "or invert"}, {"andn", "invert and"}, {"orn", "invert or"}, {"xnor", "xor invert"},
    {"3dup", "third third third"}, {"2dup=", "2dup ="}, {"2dup+", "2dup +"}, {"dup+", "dup +"},
    {"over+", "over +"}, {"overand", "over and"}, {"over=", "over ="}, {"swap<", "swap <"},
    {"bic", "invert and"}, {NULL}};

static int by_ins(const void* a, const void* b) {
    return((int)(*(entry* const*)a)->is->ins - (int)(*(entry* const*)b)->is->ins);
}

static int by_length(const void* a, const void* b) {
    const entry* x = *(entry* const*)a;
    const entry* y = *(entry* const*)b;
    if (x->said != y->said) return(x->said - y->said);
    if (x->compiled != y->compiled) return(x->compiled - y->compiled);
    return((int)x->is->ins - (int)y->is->ins);
}

int main(int argc, char *argv[]) {
    const char* report = argc > 1 ? argv[1] : "../build/phrases.txt";
    int longest = getenv("LONGEST") ? atoi(getenv("LONGEST")) : 3;
    if (longest < 1 || longest > MOST) longest = 3;
    get_ready();
    make_stacks();
    word_node* const shown = FORTH_WORDS;       // instructions are printed with the machine's own names

    // List the distinct kinds of instruction (kinds.h), leaving out the
    // ones that return, which the step identifies.
    list_kinds();
    ENTRY = room_for(kinds, sizeof(entry));
    entry_of = room_for(kinds, sizeof(entry*));
    int have_name = 0;
    for(int k=0; k < kinds; k++) {
        named m;
        forget();
        if (!names(&KIND[k].ins, 1, &m) || m.next >= 0) continue;
        entry_of[k] = &ENTRY[entries];
        ENTRY[entries++].is = &KIND[k];
        have_name += has_name(&KIND[k]);
    }

    // List the words a phrase can contain: the words of vm_opcodes.h that
    // the step models.  A phrase in vm_opcodes.h is left out, because its
    // words are already in the list.
    for(word_node* w = FORTH_WORDS; w->repr && strlen(w->repr); w++) {
        if (!w->num_ins || is_phrase(w->repr)) continue;
        if (!strcmp(w->repr, "noop") || !strcmp(w->repr, "halt") || !strcmp(w->repr, "exit")) continue;
        named m;
        forget();
        if (!names((const uint16_t*)w->ins, w->num_ins, &m)) continue;
        TYPED[vocab].name = w->repr;
        TYPED[vocab].len = w->num_ins;
        memcpy(TYPED[vocab].code, w->ins, w->num_ins * sizeof(uint16_t));
        TYPED[vocab].stack_only = only_data(TYPED[vocab].code, TYPED[vocab].len);
        vocab++;
    }

    FILE* out = fopen(report, "w");
    if (!out) { perror(report); return(1); }
#define SAY(...) do { fprintf(out, __VA_ARGS__); fprintf(out, "\n"); if (printing) { printf(__VA_ARGS__); printf("\n"); } } while (0)
    int printing = 1;

    printf("phrases of up to %d of %d words:\n", longest, vocab);
    find_phrases(longest);
    find_others(longest < ALL ? longest : ALL);
    entry** said = room_for(entries, sizeof(entry*));
    int phrased = 0;
    for(int n=0; n < entries; n++) if (ENTRY[n].said) said[phrased++] = &ENTRY[n];

    printf("compiling each phrase:\n");
    for(int from=0; from < phrased; from += BATCH) compile(said + from, phrased - from < BATCH ? phrased - from : BATCH);
    int compiled = 0, longer = 0;
    for(int n=0; n < phrased; n++) compiled += said[n]->compiled > 0, longer += said[n]->compiled > 1;

    printf("names, against gforth:\n");
    static int64_t theirs[STACKS][FILLED], ours[STACKS][FILLED];
    int their_depth[STACKS], our_depth[STACKS], agree = 0, differ = 0, absent = 0;
    char* differing = NULL;         // the names of all the words that differ, each preceded by a space
    char* lacking = NULL;
    text_more(&differing, "");
    text_more(&lacking, "");
    for(int w=0; w < vocab; w++) {
        if (!TYPED[w].stack_only || !on_machine(TYPED[w].code, TYPED[w].len, ours, our_depth)) continue;
        if (!gforth_has(TYPED[w].name) || !in_gforth(TYPED[w].name, theirs, their_depth)) {
            absent++;
            text_more(&lacking, " %s", TYPED[w].name);
        } else if (same_results(ours, our_depth, theirs, their_depth)) {
            agree++;
        } else {
            differ++;
            text_more(&differing, " %s", TYPED[w].name);
        }
    }

    SAY("%d instructions of the ALU that do no IO, and %d that do different things, not counting those that also return; %d have a name.",
        alus, entries, have_name);
    SAY("%d are what some run of up to %d of the machine's %d words does, and each run does the same on vm.c; %d are not.",
        phrased, longest, vocab, entries - phrased);
    SAY("cross.fs makes one instruction of %d of the %d phrases it was given, and more than one of %d.",
        compiled - longer, compiled, longer);
    SAY("of the machine's words that only use the data stack, gforth has %d and %d mean the same there; "
        "%d it has not:%s", agree + differ, agree, absent, lacking);
    if (differ) SAY("NOT THE SAME IN GFORTH:%s", differing);

    // For each unnamed instruction that uses only the data stack, compute
    // the data stack it leaves from each test stack.  Then list the words
    // of other Forths that leave the same stacks.
    for(int n=0; n < entries; n++) {
        entry* k = &ENTRY[n];
        k->stack_only = !has_name(k->is) && only_data(&k->is->ins, 1) && on_machine(&k->is->ins, 1, k->result, k->depth);
    }
    SAY("%s", "");
    SAY("Words other Forths have, that are one instruction here and have no name here:");
    for(int o=0; OTHERS[o].name; o++) {
        if (word_named(OTHERS[o].name)) continue;       // the machine already has a word of this name
        const char* said = gforth_has(OTHERS[o].name) ? OTHERS[o].name : OTHERS[o].is;
        if (!said || !in_gforth(said, theirs, their_depth)) continue;
        for(int n=0; n < entries; n++) {
            entry* k = &ENTRY[n];
            char is[256], phrase[PHRASE_ROOM];
            if (!k->stack_only || !same_results(k->result, k->depth, theirs, their_depth)) continue;
            decode_instruction(is, *(instruction*)&k->is->ins, shown);
            say_phrase(phrase, k);
            SAY("    %-8s %04x  %s = %s", OTHERS[o].name, k->is->ins, is, phrase);
        }
    }

    SAY("%s", "");
    SAY("Phrases cross.fs does not make one instruction of:");
    entry** order = room_for(phrased, sizeof(entry*));
    int listed = 0;
    for(int n=0; n < phrased; n++) if (said[n]->compiled > 1) order[listed++] = said[n];
    qsort(order, listed, sizeof(entry*), by_length);
    for(int n=0; n < listed; n++) {
        char is[256], phrase[PHRASE_ROOM];
        decode_instruction(is, *(instruction*)&order[n]->is->ins, shown);
        say_phrase(phrase, order[n]);
        printing = n < 24;
        SAY("    %04x  %s = %-28s %d instructions", order[n]->is->ins, is, phrase, order[n]->compiled);
    }
    printing = 0;

    SAY("%s", "");
    SAY("Every instruction a phrase gives, and the phrase; and other phrases as short, if there are any:");
    qsort(said, phrased, sizeof(entry*), by_ins);
    for(int n=0; n < phrased; n++) {
        char is[256], phrase[PHRASES_ROOM];
        decode_instruction(is, *(instruction*)&said[n]->is->ins, shown);
        say_phrases(phrase, said[n]);
        SAY("    %04x  %-9s %s = %s", said[n]->is->ins, has_name(said[n]->is) ? said[n]->is->name : "", is, phrase);
    }
    fclose(out);
    printf("...\nall of it: %s\n", report);
    return(phrased && compiled ? 0 : 1);
}
