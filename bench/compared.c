//
// compared WAS WAS.work IS IS.work
//
// Two measurements of the same work, compared by word.
//
// WAS and IS are each a directory that has an image and its names in it
// (nuc.hex, nuc.tables): one that an image is built in (build/16), or one
// that an image was kept in (build/kept/IMAGE-AS-IT-IS/image, util/keep.c).
// Each .work is how often each instruction of that image ran (work.h),
// which a counting VM left and which is kept beside the image.
//
// The two are not compared by their totals, and cannot be by place: a
// change moves every word after it.  A word is the same word by its name,
// so each image's work is added up by the word each place is in, and the
// words are put beside each other.  What a word is charged is what its own
// instructions ran, and not what it calls: a word that is called less
// shows as itself, and what calls it less shows by having fewer
// instructions run.  (What a word costs with all it calls is paths.txt,
// which is kept too.)  Words of one name are one line.
//
// Prints what each ran in all, in instructions and in cycles (cycles.h);
// then every word that ran in either, by how much its cycles changed, with
// how long the word is in each image, since a word that is another length
// is one the change made otherwise.  A word that only one image has, or
// that ran in only one, has nothing in the other's columns.
//
// It fails if every word is as long and ran the same in both, since then
// there was nothing to compare: the same image and work were given twice.
//

#include <math.h>
#include "../util/fuse.h"
#include "../util/cut.h"
#include "../util/kinds.h"
#include "cycles.h"
#include "work.h"

typedef struct {
    char name[NAME_ROOM];
    double ran[2], cycles[2];           // of each image: was, and is
    long size[2];                       // how many instructions the word is in each
    char in[2];                         // whether each image has it
} line;

static line* LINES;
static long lines, lines_room;
static double all_ran[2], all_cycles[2];

static line* line_named(const char* name) {
    for(long n=0; n < lines; n++) if (!strcmp(LINES[n].name, name)) return(&LINES[n]);
    ONE_MORE(LINES, lines, lines_room);
    line* is = &LINES[lines++];
    memset(is, 0, sizeof(line));
    snprintf(is->name, sizeof(is->name), "%s", name);
    return(is);
}

// One image's work, added up by word.
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
        if (!m.code[at] || work[at] == 0) continue;     // nothing is there: what ran was put there at the prompt
        int first = w;                                  // of the names for the same code, the first
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

// A figure of an image, or nothing if the image has no such word.
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
    // The two are of one machine, which an image says beside it: one of
    // another machine stops this where it is read (util/image.h).  One
    // that was kept before an image said its machine is taken to be of
    // the other's.
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
