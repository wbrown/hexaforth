//
// uses PLAIN.hex PLAIN.tables IMAGE.hex IMAGE.tables COUNTS WORD ...
//
// Where the work uses a small word, how often, and what it is given.
//
// A small word is in the code two ways: called, and compiled in place of
// the call, where cross.fs then joins its instructions to their
// neighbours and they are no longer to be told apart.  So the places are
// found in the image built with the peephole off (PLAIN), where a word in
// place is still the word's own instructions one after another; and how
// often each place ran is read at the same place of the image built with
// it on (IMAGE), which the work was counted on (COUNTS: work.c).  The two
// are cut into the same runs in the same order (cut.c), and this stops if
// they are not.
//
// What a use is given is what is on top of the stack when it comes to it:
// the instruction before it, as it was typed.  A number is said, a
// variable is named, and anything else is said by its instruction.  For a
// use that a run starts with, what is on top was left by a call or came
// round a branch, and it says so.
//
// The words are looked for in the order they are named, and an instruction
// that is one word's is not looked at for the next: so `-` is named before
// `negate`, which it is made of.
//
// Prints, for each word, how often the work used it called and in place;
// then what it was given, by how often; then the places, by how often,
// each with the run it is in as typed and as cross.fs made it.  It fails
// if the work used none of the words.
//

#include "../util/fuse.c"
#include "../util/cut.c"
#include "../util/reach.c"
#include "../util/kinds.c"
#include "work.c"

static cut_image PLAIN, BUILT;
static double* work;                    // how often each of IMAGE's instructions ran

typedef struct {
    int of;                             // which of the words named
    char called;                        // called, or in place
    long item;                          // the run it is in, or the call, of both images
    long from;                          // in place: the instruction of the run it starts at
    double times;
    char* given;                        // what it is given, said
} use;

static use* USE;
static long uses, uses_room;

typedef struct { char* what; double times; } gift;
static gift* GIFT;
static long gifts, gifts_room;

static word* LABELS;                    // the names of cells
static long labels, labels_room;

// The name of a cell: of a label for data, or of a word that only pushes it.
static const char* cell_named(long cell) {
    for(long n=0; n < labels; n++) if (LABELS[n].at == cell) return(LABELS[n].name);
    return(NULL);
}

static void cells_are_named(const char* tables) {
    uint16_t leaves = ins_named("exit");
    labels = data_of(tables, &LABELS, &labels_room);
    for(int w=0; w < BUILT.n; w++) {
        long at = BUILT.words[w].at, value = 0;
        if (at >= BUILT.instructions || NUMBER_IN[BUILT.code[at]].does != PUSHES_IT) continue;
        value = NUMBER_IN[BUILT.code[at++]].number;
        while (at < BUILT.instructions && NUMBER_IN[BUILT.code[at]].does == ADDS_IT) value += NUMBER_IN[BUILT.code[at++]].number;
        if (at >= BUILT.instructions || BUILT.code[at] != leaves) continue;
        ONE_MORE(LABELS, labels, labels_room);
        LABELS[labels] = BUILT.words[w], LABELS[labels++].at = value;
    }
}

// What is on top of the stack after some instructions as they were typed:
// the text is made here.
static char* top_after(const uint16_t* typed, long len) {
    char* is = NULL;
    if (!len) {
        text_more(&is, "what a call left, or a branch came with");
        return(is);
    }
    uint16_t last = typed[len - 1];
    long k = len;
    while (k && pushes_number(typed[k - 1], 1)) k--;
    if (k && pushes_number(typed[k - 1], 0)) {          // a number, and what was added to it
        long number = 0;
        for(long j=k - 1; j < len; j++) number += number_in(typed[j]);
        text_more(&is, "the number %ld", number);
    } else if (NUMBER_IN[last].does == FETCHES_AT_IT) {
        const char* name = cell_named((long)NUMBER_IN[last].number);
        if (name) text_more(&is, "the variable %s", name);
        else text_more(&is, "the variable at cell %lld", (long long)NUMBER_IN[last].number);
    } else if (last == ins_named("@")) {                // a cell fetched: a variable's, if where it is is a number
        long cell = 0;
        k = len - 1;
        while (k && pushes_number(typed[k - 1], 1)) k--;
        int is_number = k && pushes_number(typed[k - 1], 0);
        for(long j=k - 1; is_number && j < len - 1; j++) cell += number_in(typed[j]);
        if (is_number && cell_named(cell)) text_more(&is, "the variable %s", cell_named(cell));
        else if (is_number) text_more(&is, "the cell %ld", cell);
        else text_more(&is, "a cell of a record, fetched");
    } else {
        char one[2 * sizeof(((forth_op*)0)->repr) + 64];    // as kinds.c has room for one
        say_ins(one, sizeof(one), last, 1);
        text_more(&is, "what `%s` left", one);
    }
    return(is);
}

static void add_use(int of, int called, long item, long from, const uint16_t* before, long len) {
    ONE_MORE(USE, uses, uses_room);
    use* u = &USE[uses++];
    u->of = of, u->called = called, u->item = item, u->from = from;
    u->times = work[BUILT.items[item].at];
    u->given = top_after(before, len);
    if (u->times < 1) return;
    long g = 0;
    while (g < gifts && strcmp(GIFT[g].what, u->given)) g++;
    if (g == gifts) {
        ONE_MORE(GIFT, gifts, gifts_room);
        GIFT[gifts++].what = u->given;
    }
    GIFT[g].times += u->times;
}

// A word's own instructions, as they were typed: without its return.  How
// many there are; they are made here.
static long body_of(const char* name, uint16_t** body, int* is_word) {
    long at = -1, size = 0;
    for(int idx=0; idx < PLAIN.n; idx++) {
        if (!strcmp(PLAIN.words[idx].name, name)) at = PLAIN.words[idx].at;
    }
    if (at < 0) {
        fprintf(stderr, "uses: the image has no word %s\n", name);
        exit(EXIT_FAILURE);
    }
    *is_word = starting_at(&PLAIN, at);
    if (*is_word < 0) {
        fprintf(stderr, "uses: %s has no instructions of its own\n", name);
        exit(EXIT_FAILURE);
    }
    size = PLAIN.words[*is_word].size;
    *body = room_for(size, sizeof(uint16_t));
    memcpy(*body, PLAIN.code + at, size * sizeof(uint16_t));
    int returned = 0;
    if (size && (*body)[size - 1] == ins_named("exit")) size--;
    else if (size) (*body)[size - 1] = unfolded((*body)[size - 1], &returned);
    return(size);
}

static int by_times(const void* a, const void* b) {
    double x = ((const use*)a)->times, y = ((const use*)b)->times;
    return(x < y ? 1 : x > y ? -1 : 0);
}

static int by_most(const void* a, const void* b) {
    double x = ((const gift*)a)->times, y = ((const gift*)b)->times;
    return(x < y ? 1 : x > y ? -1 : 0);
}

int main(int argc, char *argv[]) {
    if (argc < 7) {
        fprintf(stderr, "usage: uses PLAIN.hex PLAIN.tables IMAGE.hex IMAGE.tables COUNTS WORD ...\n");
        return(1);
    }
    get_ready();
    list_kinds();
    cut(&PLAIN, argv[1], argv[2]);
    cut(&BUILT, argv[3], argv[4]);
    long parted = part(&PLAIN, &BUILT);
    if (parted >= 0) {
        fprintf(stderr, "uses: the two images are not the same runs and calls in the same order: they part at %lx and %lx\n",
                PLAIN.items[parted].at, BUILT.items[parted].at);
        return(1);
    }
    double* counts;
    long counted = work_read(argv[5], &counts, NULL);
    if (!counted) return(1);
    work = room_for(BUILT.instructions, sizeof(double));
    double all = 0;
    for(long at=0; at < BUILT.instructions && at < counted; at++) {
        work[at] = counts[at];
        if (BUILT.code[at]) all += work[at];
    }
    cells_are_named(argv[4]);

    char** names = argv + 6;
    int named = argc - 6;
    char* taken = room_for(PLAIN.instructions, 1);      // an instruction that is one of the words', in place
    double* called = room_for(named, sizeof(double));
    double* in_place = room_for(named, sizeof(double));
    long* length = room_for(named, sizeof(long));
    for(int of=0; of < named; of++) {                   // a word that is named is not a use of itself, or of another that is
        uint16_t* body;
        int is_word;
        body_of(names[of], &body, &is_word);
        memset(taken + PLAIN.words[is_word].at, 1, PLAIN.words[is_word].size);
        free(body);
    }
    for(int of=0; of < named; of++) {
        uint16_t* body;
        int is_word;
        long len = body_of(names[of], &body, &is_word), place = place_of(BUILT.words, BUILT.n, names[of]);
        length[of] = len;
        for(long idx=0; idx < BUILT.count; idx++) {
            const item* typed = &PLAIN.items[idx];
            if (!typed->run) {
                instruction ins = ins_of(BUILT.code[BUILT.items[idx].at + BUILT.items[idx].len - 1]);
                if (BUILT.items[idx].to != place || ins.jmp.op_type == OP_TYPE_CJMP) continue;
                const item* before = idx ? &PLAIN.items[idx - 1] : NULL;
                int joined = before && before->run && before->at + before->len == typed->at;
                add_use(of, 1, idx, 0, joined ? PLAIN.code + before->at : NULL, joined ? before->len : 0);
                called[of] += USE[uses - 1].times;
                continue;
            }
            for(long from=0; len && from + len <= typed->len; from++) {
                const uint16_t* here = PLAIN.code + typed->at + from;
                int is = !taken[typed->at + from];
                for(long k=0; is && k < len; k++) {
                    int returned;
                    is = unfolded(here[k], &returned) == body[k] && !taken[typed->at + from + k];
                }
                if (!is) continue;
                memset(taken + typed->at + from, 1, len);
                add_use(of, 0, idx, from, PLAIN.code + typed->at, from);
                in_place[of] += USE[uses - 1].times;
                from += len - 1;
            }
        }
        free(body);
    }

    double used = 0;
    printf("%s: %.0f instructions of work.\n\n", argv[3], all);
    for(int of=0; of < named; of++) {
        printf("%-8s %9.0f times: %9.0f called, which is the call and %ld more instructions each; %9.0f in place\n",
               names[of], called[of] + in_place[of], called[of], length[of], in_place[of]);
        used += called[of] + in_place[of];
    }

    qsort(GIFT, gifts, sizeof(gift), by_most);
    printf("\nwhat they were given                                times\n");
    for(long g=0; g < gifts; g++) printf("  %-46s %9.0f  %5.1f%%\n", GIFT[g].what, GIFT[g].times, 100 * GIFT[g].times / used);

    qsort(USE, uses, sizeof(use), by_times);
    printf("\nwhere                         times  word      given, and the run as typed | as made\n");
    for(long n=0; n < uses && USE[n].times >= 1; n++) {
        const use* u = &USE[n];
        const item* typed = &PLAIN.items[u->item];
        const item* made = &BUILT.items[u->item];
        char* was = typed->run ? run_said(PLAIN.code + typed->at, (int)typed->len, 1) : NULL;
        char* is = made->run ? run_said(BUILT.code + made->at, (int)made->len, 1) : NULL;
        char where[sizeof(BUILT.words[0].name) + 32];    // a word's name and a place
        snprintf(where, sizeof(where), "%s@%lx", word_at(BUILT.words, BUILT.n, made->at), made->at);
        printf("%-24s %10.0f  %-8s  %s", where, u->times, names[u->of], u->given);
        if (u->called) printf(": called\n");
        else printf(": %s | %s\n", was, is);
        free(was);
        free(is);
    }
    return(used >= 1 ? 0 : 1);
}
