//
// compared WAS WAS.work IS IS.work
//
// Compares two measurements of the same work, word by word.
//
// WAS and IS are each a directory that holds an image and its names
// (nuc.hex, nuc.tables).  It is either a directory an image is built in
// (build/16), or one an image was kept in
// (build/kept/IMAGE-AS-IT-IS/image, util/keep.c).  Each .work file gives
// the number of times each instruction of that image ran (work.h).  A
// counting VM writes it, and it is kept beside the image.
//
// The totals alone do not show what a change did, and the two images
// cannot be compared address by address, because a change moves every
// word after it.  Words are matched by name instead.  Each image's counts
// are totalled by the word each address is in, and the two totals for a
// word are printed side by side.  A word is charged for its own
// instructions that ran, and not for the words it calls.  So a word that
// is called less often has a smaller total of its own, and the word that
// calls it less often has fewer instructions run as well.  (paths.txt,
// which is kept too, gives the cost of a word with everything it calls.)
// All the words with one name are reported on one line.
//
// It prints the total each image ran, in instructions and in cycles
// (cycles.h).  Then it prints every word that ran in either image, ordered
// by how much its cycles changed, with the word's length in each image.
// A word whose length differs was compiled differently after the change.
// If only one image has a word, the other image's columns are blank.  If
// both have it and it ran in only one, the other's columns are 0.
//
// It fails if every word has the same length and ran the same number of
// times in both images, because then there was nothing to compare: the
// same image and the same work were given twice.
//

#include <math.h>
#include "../util/fuse.h"
#include "../util/cut.h"
#include "../util/kinds.h"
#include "cycles.h"
#include "work.h"

typedef struct {
    char name[NAME_ROOM];
    double ran[2], cycles[2];           // instructions run and cycles, in WAS ([0]) and in IS ([1])
    long size[2];                       // the word's length in instructions, in each image
    char in[2];                         // non-zero if the image has the word
} line;

static line* LINES;
static long lines, lines_room;
static double all_ran[2], all_cycles[2];

// Returns the line for a name, creating it if there is none.
static line* line_named(const char* name) {
    for(long n=0; n < lines; n++) if (!strcmp(LINES[n].name, name)) return(&LINES[n]);
    ONE_MORE(LINES, lines, lines_room);
    line* is = &LINES[lines++];
    memset(is, 0, sizeof(line));
    snprintf(is->name, sizeof(is->name), "%s", name);
    return(is);
}

// Totals one image's counts by word.  `which` is 0 for WAS and 1 for IS.
static void weigh(int which, const char* dir, const char* counts) {
    cut_image m;
    char* hex = NULL;
    char* tables = NULL;
    double* work;
    text_more(&hex, "%s/nuc.hex", dir);
    text_more(&tables, "%s/nuc.tables", dir);
    cut(&m, hex, tables);
    long counted = work_read(counts, &work, NULL);
    if (!counted) exit(EXIT_FAILURE);
    long places = m.instructions < counted ? m.instructions : counted;

    for(int w=0; w < m.n; w++) {
        line* l = line_named(m.words[w].name);
        l->in[which] = 1;
        l->size[which] += m.words[w].size;
    }
    int w = -1;
    for(long at=0; at < places; at++) {
        while (w + 1 < m.n && m.words[w + 1].at <= at) w++;
        if (!m.code[at] || work[at] == 0) continue;     // the image has no instruction here: code that ran here was compiled at the prompt
        int first = w;                                  // when several words name the same code, use the first
        while (first > 0 && m.words[first - 1].at == m.words[w].at) first--;
        line* l = line_named(w < 0 ? "(before any word)" : m.words[first].name);
        double cycles = work[at] * cycles_of(ins_of(m.code[at]), NUMBER_IN[m.code[at]].does == FETCHES_AT_IT);
        l->ran[which] += work[at];
        l->cycles[which] += cycles;
        all_ran[which] += work[at];
        all_cycles[which] += cycles;
    }
    free(hex);
    free(tables);
}

static double change_of(const line* l) { return(l->cycles[1] - l->cycles[0]); }

static int by_change(const void* a, const void* b) {
    double x = fabs(change_of(a)), y = fabs(change_of(b));
    if (x != y) return(x < y ? 1 : -1);
    return(strcmp(((const line*)a)->name, ((const line*)b)->name));
}

// Prints one image's figure for a word, or blanks if the image does not
// have the word.
static void figure(int has, int width, double is) {
    if (has) printf(" %*.0f", width, is);
    else printf(" %*s", width, "");
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "usage: compared WAS WAS.work IS IS.work\n"
                        "  WAS and IS: each a directory with nuc.hex and nuc.tables in it\n");
        return(1);
    }
    // Both images must be for the same machine, which is recorded beside
    // each image.  Reading an image for a different machine stops the
    // program (util/image.h).  An image that was kept before images
    // recorded their machine is taken to be for the other image's machine.
    char* was = NULL;
    char* is = NULL;
    text_more(&was, "%s/nuc.hex", argv[1]);
    text_more(&is, "%s/nuc.hex", argv[3]);
    if (!machine_of(is)) machine_from(was);
    free(was);
    free(is);
    get_ready();
    list_kinds();
    weigh(0, argv[1], argv[2]);
    weigh(1, argv[3], argv[4]);

    printf("was  %s, %s: %.0f instructions run, %.0f cycles\n", argv[1], argv[2], all_ran[0], all_cycles[0]);
    printf("is   %s, %s: %.0f instructions run, %.0f cycles\n", argv[3], argv[4], all_ran[1], all_cycles[1]);
    printf("the change: %+.0f instructions (%+.2f%%), %+.0f cycles (%+.2f%%)\n\n",
           all_ran[1] - all_ran[0], all_ran[0] ? 100 * (all_ran[1] - all_ran[0]) / all_ran[0] : 0,
           all_cycles[1] - all_cycles[0], all_cycles[0] ? 100 * (all_cycles[1] - all_cycles[0]) / all_cycles[0] : 0);

    qsort(LINES, lines, sizeof(line), by_change);
    long changed = 0;
    for(long n=0; n < lines; n++) {
        const line* l = &LINES[n];
        if (change_of(l) != 0 || l->ran[0] != l->ran[1] || l->size[0] != l->size[1]) changed++;
    }
    if (!changed) {
        fprintf(stderr, "compared: every word is as long and ran the same in both: there is nothing to compare\n");
        return(1);
    }
    printf("%-24s %12s %12s %12s %8s %12s %12s %6s %6s\n", "word", "cycles: was", "is", "change", "of all",
           "run: was", "is", "long:", "is");
    for(long n=0; n < lines; n++) {
        const line* l = &LINES[n];
        if (l->ran[0] == 0 && l->ran[1] == 0) continue;
        printf("%-24s", l->name);
        figure(l->in[0], 12, l->cycles[0]);
        figure(l->in[1], 12, l->cycles[1]);
        printf(" %+12.0f %+7.2f%%", change_of(l), all_cycles[0] ? 100 * change_of(l) / all_cycles[0] : 0);
        figure(l->in[0], 12, l->ran[0]);
        figure(l->in[1], 12, l->ran[1]);
        figure(l->in[0], 6, (double)l->size[0]);
        figure(l->in[1], 6, (double)l->size[1]);
        putchar('\n');
    }
    return(0);
}
