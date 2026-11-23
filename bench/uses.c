//
// uses PLAIN.hex PLAIN.tables IMAGE.hex IMAGE.tables COUNTS WORD ...
//
// Reports where the work uses a small word, how often, and what value the
// word is passed.
//
// A small word appears in the code in two forms: as a call, and inlined in
// place of the call.  cross.fs joins the inlined instructions with their
// neighbours, and then they can no longer be told apart.  So the uses are
// found in the image built with the peephole off (PLAIN), where an inlined
// word is still the word's own instructions in sequence.  The number of
// times each use ran is read at the same position in the image built with
// the peephole on (IMAGE), which is the image the work was counted on
// (COUNTS: work.h).  The two images are cut into the same runs in the same
// order (cut.h), and the program stops if they are not.
//
// The value a use is passed is the top of the stack when the use is
// reached.  It is described from the instruction before the use, as that
// instruction was typed: a number is printed, a variable is named, and
// anything else is described by its instruction.  If the use is the first
// thing in a run, the top of the stack was left by a call or arrived
// through a branch, and the report says so.
//
// The words are searched for in the order they are named, and an
// instruction that belongs to a use of one word is not considered for the
// next word.  So `-` is named before `negate`, because `-` contains
// `negate`.
//
// For each word it prints the number of times the work used it, as a call
// and inlined.  Then it prints the values the words were passed, ordered
// by count.  Then it prints the uses, ordered by count, each with the run
// it is in as typed and as cross.fs compiled it.  It fails if the work
// used none of the words.
//

#include "../util/fuse.h"
#include "../util/cut.h"
#include "../util/reach.h"
#include "../util/kinds.h"
#include "work.h"

static cut_image PLAIN, BUILT;
static double* work;                    // the number of times each of IMAGE's instructions ran

typedef struct {
    int of;                             // the index of the word among those named
    char called;                        // 1 if the use is a call, 0 if it is inlined
    long item;                          // the index of the run it is in, or of the call; the same in both images
    long from;                          // for an inlined use: the offset in the run where it starts
    double times;                       // the number of times it ran
    char* given;                        // a description of the value it is passed
} use;

static use* USE;
static long uses, uses_room;

typedef struct { char* what; double times; } gift;      // a description of a value, and the number of times it was passed
static gift* GIFT;
static long gifts, gifts_room;

static word* LABELS;                    // the names of cells
static long labels, labels_room;

// Returns the name of a cell: a label for data, or a word that only pushes
// the cell's address.  Returns NULL if the cell has no name.
static const char* cell_named(long cell) {
    for(long n=0; n < labels; n++) if (LABELS[n].at == cell) return(LABELS[n].name);
    return(NULL);
}

// Fills LABELS from the image's labels for data, and from its words that
// only push a number.
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

// Returns a description of the value on top of the stack after some
// instructions, as they were typed.  The text is allocated.
static char* top_after(const uint16_t* typed, long len) {
    char* is = NULL;
    if (!len) {
        text_more(&is, "what a call left, or a branch came with");
        return(is);
    }
    uint16_t last = typed[len - 1];
    long k = len;
    while (k && pushes_number(typed[k - 1], 1)) k--;
    if (k && pushes_number(typed[k - 1], 0)) {          // a number pushed, and the literals added to it
        long number = 0;
        for(long j=k - 1; j < len; j++) number += number_in(typed[j]);
        text_more(&is, "the number %ld", number);
    } else if (NUMBER_IN[last].does == FETCHES_AT_IT) {
        const char* name = cell_named((long)NUMBER_IN[last].number);
        if (name) text_more(&is, "the variable %s", name);
        else text_more(&is, "the variable at cell %lld", (long long)NUMBER_IN[last].number);
    } else if (last == ins_named("@")) {                // a cell fetched: it is a variable if its address is a number
        long cell = 0;
        k = len - 1;
        while (k && pushes_number(typed[k - 1], 1)) k--;
        int is_number = k && pushes_number(typed[k - 1], 0);
        for(long j=k - 1; is_number && j < len - 1; j++) cell += number_in(typed[j]);
        if (is_number && cell_named(cell)) text_more(&is, "the variable %s", cell_named(cell));
        else if (is_number) text_more(&is, "the cell %ld", cell);
        else text_more(&is, "a cell of a record, fetched");
    } else {
        char one[2 * sizeof(((forth_op*)0)->repr) + 64];    // the size kinds.h uses for one instruction's text
        say_ins(one, sizeof(one), last, 1);
        text_more(&is, "what `%s` left", one);
    }
    return(is);
}

// Records a use, with its count from the built image and a description of
// the value it is passed.  `before` is the `len` instructions typed before
// the use in its run.
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

// Returns the number of a word's own instructions as they were typed,
// without its return.  `*body` receives the instructions, which are
// allocated, and `*is_word` receives the word's index in PLAIN.  Exits if
// the image has no such word, or if the word has no instructions of its
// own.
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
    machine_from(argv[3]);              // take the cell size from the image before the step is used
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
    char* taken = room_for(PLAIN.instructions, 1);      // 1 for an instruction in a named word's own definition, or in a use already found
    double* called = room_for(named, sizeof(double));
    double* in_place = room_for(named, sizeof(double));
    long* length = room_for(named, sizeof(long));
    for(int of=0; of < named; of++) {                   // a named word's own definition is not a use of that word, or of another named word
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
        char where[sizeof(BUILT.words[0].name) + 32];    // room for a word's name and an address
        snprintf(where, sizeof(where), "%s@%lx", word_at(BUILT.words, BUILT.n, made->at), made->at);
        printf("%-24s %10.0f  %-8s  %s", where, u->times, names[u->of], u->given);
        if (u->called) printf(": called\n");
        else printf(": %s | %s\n", was, is);
        free(was);
        free(is);
    }
    return(used >= 1 ? 0 : 1);
}
