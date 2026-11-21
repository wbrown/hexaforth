//
// disasm_hex IMAGE.hex [NAME... | FROM TO] [LABELS.lst] [--typed FILE.fs] [--counts WORK]
//                      [--layout [--near NAME]...] [--memory FILE [--own DISK.img]] [--headers]
//                      [--color | --no-color]
//
// An image's code, word by word, as the decompiler shows it (forth/see.fs),
// with the machine's form of every instruction beside it.
//
// --layout is the whole image as it is laid out, to be read for a better
// way of laying it: the pages of code as `examine` has them; the calls
// that go to a word on another page, which are a push and a call of
// execute where a call within a page or into the kernel's is one
// instruction, each with how often it was run if --counts says, and what
// they come to of the work; the words that are called so, by how often
// those calls are run and again by how many places make them, each beside
// the calls that reach it with one instruction, which moving it would
// make longer; the data by cell, each variable and table with
// the cells to the next and whether one instruction reaches it; and then
// every word in order of place, with a line where each page starts and
// ends that says what it has and what it has left, and a line for whatever
// is between two words, nothing or what is no word's.  The targets
// listing-64 and the like write it (CMakeLists.txt).  With --near NAME,
// as often as there are words, it is instead what the calls would be were
// those words in the kernel's page (show_near, below, says what it
// prints): asked before a word is moved there.
//
// --memory FILE is the machine's memory in the image's place, as the
// reference VM wrote it when it stopped: what the machine compiled is in
// it, under the names its dictionary still has, and with --layout the calls
// that code makes to other pages are counted with the image's.
//
// With --own DISK.img as well, the disk that machine booted from as it left
// it, the words its boot compiled that are each file's own have their names
// too, which it keeps there (forth/files.fs).
//
// --headers is the dictionary and nothing else, from the header a search
// starts at to the one its chain stops at (show_headers).
//
// --typed FILE.fs is the image as the machine has it when that file has
// been typed at the nucleus's prompt: the words the file defines are
// there as the nucleus's own compiler made them, and with no word named
// those are the words shown.  --counts WORK has beside each instruction
// how often it was run: WORK is the work by place that the reference VM
// that counts leaves (RUN_WORK, bench/counting.c).
//
// Each word starts with its name, and its code is laid out as source is
// written: what is inside an if or a begin is indented, and its branches
// are said as `if`, `else`, `then`, `begin`, `until`, `again`, `while` and
// `repeat`, read from where they go as see.fs reads them.  An instruction
// is a line: where it is, its number, the words for it, and its fields as
// vm_opcodes.h names them.
//
//   a call            the word it calls, by its header or its label
//   a number          the word that does nothing but push it (a variable,
//                     anything made with create), or its label of data;
//                     an execution token as ['] and its word; or itself
//   a variable        fetched or stored by one instruction, by its name
//   a call of execute after a number that is a word's place: that word,
//                     on another page
//   an instruction    the words the image's own table has for it (`fused`,
//                     made by util/fused.c, which asks the machine); else
//                     its name in vm_opcodes.h; else its number
//   a return          folded into the instruction before it: `exit`, or
//                     `;` where the word ends
//
// The names are the image's dictionary and the labels in the file of names
// beside it (IMAGE.tables, which util/words.h reads), and the file a
// word's code is from is from the .sizes beside it (bench/files.h).  A
// listing (.lst) may be given too, for whatever it names at a place of
// code.  How long a word is, is util/words.h's to say.
// With no word named and no range, it is every word of the image; a range
// is the words that have code in it.  The nucleus's tables, which are laid
// down after the code of their page, are not code and are not shown.
//
// On a terminal it is in colour, as `NO_COLOR` and `--no-color` say
// otherwise: a word called in cyan, and in italics when it is a label;
// numbers in yellow; variables and data in magenta; the words that steer
// in red; and the words of one instruction that is several underlined as
// one.  Where it is and its fields are dim.
//

#include <unistd.h>
#include "../vm.c"
#include "image.h"
#include "words.h"
#include "layout.h"
#include "../bench/files.h"
#include "../bench/work.h"

// The nucleus typed at (test/nucleus.c): what of it is used here.  Its
// header is not had: it names a function as vm.c, which this has, names
// one of its own.
extern context machine;
extern char* said;
void nucleus_is(const char* path);
void start(void);
void line(const char* format, ...);
void type(void);

static int COLOR;
static double* RAN;             // how often the instruction at each place was run, if that was asked for,
static long RAN_PLACES;         // and of how many places it is said

// ---- the image

static uint8_t* BYTES;          // the image as it is,
static uint8_t* PLAIN;          // and with the nucleus's tables made nothing
static size_t LEN;
static const uint16_t* INSN;    // its instructions
static long INSTRUCTIONS;
static word* WORDS;             // its words, by header and by label (words.h)
static int NWORDS;
static word* DATA;              // its labels of data
static long NDATA;
static span* FILES;             // the source files its code is from (bench/files.h)
static int NFILES;

typedef struct { long at; char* name; } named;
static named* LISTED;           // names from a listing, where there are no labels
static long NLISTED, LISTED_ROOM;

typedef struct { long xt; long head; int pushes; uint64_t value; } header;
static header* HEADERS;         // the dictionary: each word's place, its header,
static long NHEADERS, HEADER_ROOM;  // and the number it pushes, if that is all it does

typedef struct { uint16_t ins; int n; long head[4]; } fusion;
static fusion* FUSED;           // the image's own table of an instruction's words
static long NFUSED, FUSED_ROOM;

static long EXECUTE = -1;       // where execute is: a call of it after a word's place

static long target_of(long at) { return((long)branch(at + 1, INSN[at] & 0x1fff)); }
static int is_literal(uint16_t w) { return(w & 0x8000); }
static int is_variable(uint16_t w) { return(is_literal(w) && (w & 0x3000) == 0x3000); }
static int adds(uint16_t w) { return(is_literal(w) && !is_variable(w) && (w & 0x4000)); }
static int pushes(uint16_t w) { return(is_literal(w) && !is_variable(w) && !(w & 0x4000)); }
static uint64_t number_in(uint16_t w) { return((uint64_t)(w & 0xfff) << (((w >> 12) & 3) * LIT_BITS)); }
static int kind_of(uint16_t w) { return(is_literal(w) ? -1 : (w >> 13) & 3); }     // OP_TYPE, or a literal

// The number the code at a place pushes, if that is all it does: pushes,
// adds, and a plain exit (see.fs, hval).
static int pushes_only(long at, uint64_t* value) {
    if (at < 0 || at >= INSTRUCTIONS || !pushes(INSN[at])) return(0);
    uint64_t n = number_in(INSN[at]);
    for(at++; at < INSTRUCTIONS && adds(INSN[at]); at++) n += number_in(INSN[at]);
    *value = n;
    return(at < INSTRUCTIONS && INSN[at] == 0x7403);
}

static void read_dictionary(void) {
    for(uint32_t head = first_header(BYTES, LEN); head; ) {
        uint64_t xt;
        char name[NAME_ROOM];
        uint32_t after = read_header(BYTES, LEN, head, &xt, name);
        ONE_MORE(HEADERS, NHEADERS, HEADER_ROOM);
        header* h = &HEADERS[NHEADERS++];
        h->xt = (long)xt, h->head = head;
        h->pushes = pushes_only(h->xt, &h->value);
        if (!strcmp(name, "execute")) EXECUTE = h->xt;
        head = after;
    }
}

// The dictionary as it is searched, from the header a search starts at:
// the cell each is at, the cell its link says, which is the header looked
// at after it, its word's place and its name.  The last line is the header
// the chain stops at, whose link in an image is nothing: of a machine's
// memory it is where a chain that was broken stops, and its link is there
// to be read.
static void show_headers(void) {
    printf("  cell   after   place  name\n");
    for(uint32_t head = first_header(BYTES, LEN); head; ) {
        uint64_t xt;
        char name[NAME_ROOM];
        uint32_t after = read_header(BYTES, LEN, head, &xt, name);
        printf("%6lx  %6llx  %6llx  %s\n", (long)(head / CELL_BYTES),
               (unsigned long long)cell_of(BYTES, head / CELL_BYTES), (unsigned long long)xt, name);
        head = after;
    }
}

static const char* name_at_head(long head) {
    static char name[NAME_ROOM];
    uint64_t xt;
    read_header(BYTES, LEN, (uint32_t)head, &xt, name);
    return(name);
}

static int has_header(long xt) {
    for(long idx=0; idx < NHEADERS; idx++) if (HEADERS[idx].xt == xt) return(1);
    return(0);
}

static int immediate(long xt) {
    for(long idx=0; idx < NHEADERS; idx++) {
        if (HEADERS[idx].xt == xt) return((cell_of(BYTES, HEADERS[idx].head / CELL_BYTES + 1) >> (CELL_BITS - 1)) & 1);
    }
    return(0);
}

// The decompiler's table (fused.fs), which the cell `fused-at` says the
// place of (see.fs): cells of the instruction, how many words, and the
// address of each word's header, to a cell that is nothing.
static void read_fused(void) {
    long at = place_of(WORDS, NWORDS, "fused-at");
    uint64_t told, cells;
    size_t room = LEN / CELL_BYTES;
    if (at < 0 || !pushes_only(at, &told) || told >= room) return;
    cells = cell_of(BYTES, told);
    if (!cells) return;
    while (cells + 2 < room && cell_of(BYTES, cells)) {
        ONE_MORE(FUSED, NFUSED, FUSED_ROOM);
        fusion* f = &FUSED[NFUSED];
        f->ins = (uint16_t)cell_of(BYTES, cells);
        f->n = (int)cell_of(BYTES, cells + 1);
        if (f->n < 1 || f->n > 4 || cells + 2 + f->n > room) break;
        for(int k=0; k < f->n; k++) f->head[k] = (long)cell_of(BYTES, cells + 2 + k) * CELL_BYTES;
        NFUSED++;
        cells += 2 + f->n;
    }
}

static const fusion* fused(uint16_t w) {
    for(long idx=0; idx < NFUSED; idx++) if (FUSED[idx].ins == w) return(&FUSED[idx]);
    return(NULL);
}

static void read_listing(const char* path) {
    FILE* in = fopen(path, "r");
    if (!in) { perror(path); exit(EXIT_FAILURE); }
    char* line = NULL;
    long room = 0;
    while (line_of(in, &line, &room)) {
        long at;
        int skip = 0;
        if (sscanf(line, "%lx %n", &at, &skip) != 1 || !line[skip]) continue;
        line[strcspn(line, "\r\n")] = 0;
        ONE_MORE(LISTED, NLISTED, LISTED_ROOM);
        LISTED[NLISTED].at = at;
        LISTED[NLISTED++].name = room_for(strlen(line + skip) + 1, 1);
        strcpy(LISTED[NLISTED - 1].name, line + skip);
    }
    free(line);
    fclose(in);
}

// The name of the word that starts at a place, and whether it is a label;
// or nothing.
static const char* code_name(long at, int* label) {
    int lo = 0, hi = NWORDS;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (WORDS[mid].at < at) lo = mid + 1; else hi = mid;
    }
    for(int idx=lo; idx < NWORDS && WORDS[idx].at == at; idx++) {
        *label = !has_header(at);
        if (!*label) {                                  // the name of its header, of all it may have
            for(long h=0; h < NHEADERS; h++) if (HEADERS[h].xt == at) return(name_at_head(HEADERS[h].head));
        }
        return(WORDS[idx].name);
    }
    for(long idx=0; idx < NLISTED; idx++) {
        if (LISTED[idx].at == at) { *label = 1; return(LISTED[idx].name); }
    }
    return(NULL);
}

// ---- what is printed: a line in pieces, each with how it looks

enum { PLAIN_STYLE, DIM, CALL, LABEL, NUMBER, DATA_STYLE, DATA_LABEL, STEER, UNDER, RAW, HEAD, HEAD_LABEL };
static const char* LOOK[] = {
    "", "\033[2m", "\033[36m", "\033[3;36m", "\033[33m", "\033[35m", "\033[3;35m", "\033[1;31m", "\033[4;32m",
    "\033[2m", "\033[1;36m", "\033[1;3;36m" };

static char* LINE;
static long LINE_ROOM, LINE_LEN, SHOWN;         // its text, and how wide it shows

static void put(int style, const char* text) {
    long len = (long)strlen(text);
    int styled = COLOR && style != PLAIN_STYLE;
    long more = len + (styled ? (long)strlen(LOOK[style]) + 4 : 0);
    AS_MANY(LINE, LINE_LEN + more + 1, LINE_ROOM);
    if (styled) LINE_LEN += sprintf(LINE + LINE_LEN, "%s", LOOK[style]);
    memcpy(LINE + LINE_LEN, text, len);
    LINE_LEN += len;
    if (styled) LINE_LEN += sprintf(LINE + LINE_LEN, "\033[0m");
    LINE[LINE_LEN] = 0;
    SHOWN += len;
}

static void putf(int style, const char* format, ...) {
    va_list args, again;
    va_start(args, format);
    va_copy(again, args);
    int len = vsnprintf(NULL, 0, format, args);         // how long it is, and then it is written
    va_end(args);
    char* text = room_for((size_t)len + 1, 1);
    vsnprintf(text, (size_t)len + 1, format, again);
    va_end(again);
    put(style, text);
    free(text);
}

static void to_column(long column) {
    while (SHOWN < column) put(PLAIN_STYLE, " ");
}

static void line_done(void) {
    puts(LINE_LEN ? LINE : "");
    LINE_LEN = 0, SHOWN = 0;
    if (LINE) LINE[0] = 0;
}

#define SOURCE_AT 13            // where the source starts on a line,
#define FIELDS_AT 58            // where the fields do,
#define RAN_AT    96            // and where how often it was run does

// An instruction's fields, as vm_opcodes.h names them; and after them how
// often it was run, if that is known.
static void fields(long at) {
    uint16_t w = INSN[at];
    instruction ins = *(instruction*)&w;
    to_column(FIELDS_AT);
    if (is_variable(w)) {
        putf(DIM, "var %d %s", w & 0xfff, (w & 0x4000) ? "!" : "@");
    } else if (is_literal(w)) {
        int shift = (w >> 12) & 3;
        putf(DIM, "%s%d%s", (w & 0x4000) ? "add " : "push ", w & 0xfff, shift ? (shift == 1 ? "<<12" : "<<24") : "");
    } else if (kind_of(w) == OP_TYPE_ALU) {
        static const int DELTA[] = { 0, 1, 3, 2 };     // the field's value to its place in the table
        putf(DIM, "%s %s %s", INPUT_MUX_REPR[ins.alu.in_mux], ALU_OPS_REPR[ins.alu.alu_op], OUTPUT_MUX_REPR[ins.alu.out_mux]);
        if (ins.alu.dstack) putf(DIM, " %s", DSTACK_REPR[DELTA[w >> 2 & 3]]);
        if (ins.alu.rstack) putf(DIM, " %s", RSTACK_REPR[DELTA[w & 3]]);
        if (ins.alu.r_eip) put(DIM, " RET");
    } else {
        putf(DIM, "%s %04lx", (const char*[]){ "jump", "0jump", "call" }[kind_of(w)], target_of(at));
    }
    if (RAN) {
        to_column(RAN_AT);
        putf(PLAIN_STYLE, "%12.0f", at < RAN_PLACES ? RAN[at] : 0.0);
    }
}

// A word by its place: its name, or the place.
static void link_to(long at, int style_if_named) {
    int label;
    const char* name = code_name(at, &label);
    if (name) put(label ? LABEL : style_if_named, name);
    else putf(RAW, "$%04lx", at);
}

// A number, by what it is (see.fs, snum): the word that pushes it, its
// label of data, an execution token, or itself.
static void number(uint64_t n, long self) {
    if (n >= VAR_BASE) {
        for(long idx=0; idx < NHEADERS; idx++) {
            if (HEADERS[idx].pushes && HEADERS[idx].value == n && HEADERS[idx].xt != self) {
                put(DATA_STYLE, name_at_head(HEADERS[idx].head));
                return;
            }
        }
        for(long idx=0; idx < NDATA; idx++) {
            if ((uint64_t)DATA[idx].at == n) { put(DATA_LABEL, DATA[idx].name); return; }
        }
    }
    int label;
    if (n > PAGE && n < (uint64_t)INSTRUCTIONS && code_name((long)n, &label)) {
        put(STEER, "[']");
        put(PLAIN_STYLE, " ");
        link_to((long)n, CALL);
        return;
    }
    putf(NUMBER, "%lld", (long long)(int64_t)n);
    if (n >= 4095) putf(DIM, " $%llx", (unsigned long long)n);    // a mask, or a place, is read in hex
}

// The words for an ALU instruction with no return in it.
static void alu_words(uint16_t w) {
    const fusion* f = fused(w);
    if (f) {
        for(int k=0; k < f->n; k++) {
            if (k) put(f->n > 1 ? UNDER : PLAIN_STYLE, " ");
            put(f->n > 1 ? UNDER : PLAIN_STYLE, name_at_head(f->head[k]));
        }
        return;
    }
    instruction ins = *(instruction*)&w;
    const char* named = lookup_opcode(FORTH_WORDS, ins);
    if (named) put(PLAIN_STYLE, named);
    else putf(RAW, "[$%04x]", w);
}

// A call to a word on another page is the word's place pushed, in one
// instruction or more, and a call of execute, or a jump to it where the
// call is the last thing done.  Is that what starts at a place, before
// `to`?  Then where the call of execute is, and the word's place.
static int far_call(long at, long to, long* call_at, long* callee) {
    if (at >= to || !pushes(INSN[at]) || EXECUTE < 0) return(0);
    uint64_t n = number_in(INSN[at]);
    long end = at + 1;
    while (end < to && adds(INSN[end])) n += number_in(INSN[end++]);
    int label;
    if (end >= to || !INSN[end] || (kind_of(INSN[end]) != OP_TYPE_CALL && kind_of(INSN[end]) != OP_TYPE_JMP)) return(0);
    if (target_of(end) != EXECUTE || n >= (uint64_t)INSTRUCTIONS || !code_name((long)n, &label)) return(0);
    *call_at = end;
    *callee = (long)n;
    return(1);
}

// ---- a word: where its branches go, then each instruction

typedef struct { long* at; long n, room; } places;
static void add_place(places* p, long at) { ONE_MORE(p->at, p->n, p->room); p->at[p->n++] = at; }
static int count_at(const places* p, long at) { int n = 0; for(long k=0; k < p->n; k++) n += p->at[k] == at; return(n); }
static int move_place(places* p, long from, long to) {
    for(long k=0; k < p->n; k++) if (p->at[k] == from) { p->at[k] = to; return(1); }
    return(0);
}

enum { PLAIN_INS, IF, ELSE, UNTIL, AGAIN, REPEAT, WHILE, AWAY };

static void show_word(const word* w) {
    long from = w->at, to = w->at + w->size;
    places thens = { 0 }, begins = { 0 }, whiles = { 0 };
    char* role = room_for(w->size + 1, 1);

    // Where its branches go (see.fs, the first reading): a 0branch on is an
    // if, or a while where the instruction before where it lands is a jump
    // back; a jump on from just before where an if lands is an else; a jump
    // or a 0branch back into the word is an until, an again or a repeat; any
    // other jump is to another word, and does not come back.
    for(long at=from; at < to; at++) {
        uint16_t c = INSN[at];
        int kind = kind_of(c);
        if (c == 0 || (kind != OP_TYPE_JMP && kind != OP_TYPE_CJMP)) continue;
        long t = target_of(at);
        if (kind == OP_TYPE_CJMP && t > at) {
            long before = t - 1;
            int back = before > at && before < to && kind_of(INSN[before]) == OP_TYPE_JMP && INSN[before] &&
                       target_of(before) >= from && target_of(before) <= at;
            if (back) { role[at - from] = WHILE; add_place(&whiles, t); }
            else { role[at - from] = IF; add_place(&thens, t); }
        } else if (kind == OP_TYPE_CJMP) {
            role[at - from] = UNTIL; add_place(&begins, t);
        } else if (t > at && move_place(&thens, at + 1, t)) {
            role[at - from] = ELSE;
        } else if (t >= from && t <= at) {
            role[at - from] = count_at(&whiles, at + 1) ? REPEAT : AGAIN; add_place(&begins, t);
        } else {
            role[at - from] = AWAY;
        }
    }

    int label;
    const char* name = code_name(from, &label);
    put(DIM, "            ");
    put(STEER, ":");
    put(PLAIN_STYLE, " ");
    if (name) put(label ? HEAD_LABEL : HEAD, name);
    else putf(RAW, "$%04lx", from);
    to_column(FIELDS_AT);
    const char* file = NFILES ? file_at(FILES, NFILES, from) : NULL;
    putf(DIM, "%ld instructions from %04lx%s%s%s", w->size, from, immediate(from) ? ", immediate" : "",
         file ? ", " : "", file ? file : "");
    line_done();

    int indent = 1;
    #define START_LINE(at) do { \
        if ((at) >= 0) putf(DIM, "%04lx  %04x", (long)(at), INSN[at]); \
        to_column(SOURCE_AT + 2 * (indent > 0 ? indent : 0)); \
    } while (0)

    for(long at=from; at < to; at++) {
        for(int n=count_at(&thens, at); n > 0; n--) {
            indent--;
            START_LINE(-1); put(STEER, "then"); line_done();
        }
        for(int n=count_at(&begins, at); n > 0; n--) {
            START_LINE(-1); put(STEER, "begin"); line_done();
            indent++;
        }
        uint16_t c = INSN[at];
        int r = role[at - from];
        if (r == UNTIL || r == AGAIN || r == REPEAT || r == ELSE) indent--;
        START_LINE(at);
        int last = at == to - 1;

        if (c == 0) {
            put(STEER, "halt");
        } else if (is_variable(c)) {
            number(VAR_BASE + (c & 0xfff), from);
            put(PLAIN_STYLE, (c & 0x4000) ? " !" : " @");
        } else if (adds(c)) {
            number(number_in(c), from);
            put(PLAIN_STYLE, " +");
        } else if (pushes(c)) {
            // A number in pieces is one number, said on its first line; and
            // a word's place pushed and then execute called is that word.
            uint64_t n = number_in(c);
            long end = at + 1, callee;
            while (end < to && adds(INSN[end])) n += number_in(INSN[end++]);
            int far = far_call(at, to, &end, &callee);
            if (far) link_to((long)n, CALL);
            else number(n, from);
            fields(at);
            line_done();
            for(long more=at + 1; more < end || (far && more == end); more++) {
                START_LINE(more);
                put(DIM, far && more == end ? (kind_of(INSN[more]) == OP_TYPE_JMP ? "(on another page, and away)" : "(on another page)") : "...");
                fields(more);
                line_done();
            }
            at = far ? end : end - 1;
            continue;
        } else if (kind_of(c) == OP_TYPE_CALL) {
            link_to(target_of(at), CALL);
        } else if (kind_of(c) == OP_TYPE_ALU) {
            if (c == 0x7403) {
                put(STEER, last ? ";" : "exit");
            } else if ((c & 0x1000) && (c & 3) == 3) {  // a return folded in
                alu_words(c & ~0x1003);
                put(PLAIN_STYLE, " ");
                put(STEER, last ? ";" : "exit");
            } else {
                alu_words(c);
            }
        } else {
            static const char* SAID[] = { "", "if", "else", "until", "again", "repeat", "while", "" };
            if (r == AWAY) {
                link_to(target_of(at), CALL);
                put(PLAIN_STYLE, " ");
                put(DIM, "(and away)");
            } else {
                put(STEER, SAID[r]);
            }
        }
        fields(at);
        line_done();
        if (r == IF || r == ELSE) indent++;
    }
    for(int n=count_at(&thens, to); n > 0; n--) {       // a then at the very end
        indent--;
        START_LINE(-1); put(STEER, "then"); line_done();
    }
    line_done();
    free(role); free(thens.at); free(begins.at); free(whiles.at);
}

// ---- the layout: where each page starts and ends and what it has left,
// the calls that go to another page, and the data by cell

static layout LAID;             // the image laid out (layout.h): what `examine` prints of it

static long bytes_of(long instructions) { return(instructions * (long)sizeof(instruction)); }

static void page_starts(long page) {
    const code_page* has = &LAID.pages[page];
    printf("==== page %ld, from %04lx: %ld words, %ld bytes of code, %ld of tables; %ld bytes left ====\n\n",
           page, page * PAGE, has->words, bytes_of(has->code), bytes_of(has->tables), bytes_of(PAGE - has->end));
}

// What is between two words of a page, or after its last: nothing, or what
// is no word's, which is a table or the two instructions an image starts
// with.
static void between(long from, long to) {
    if (to <= from) return;
    const uint16_t* as_it_is = (const uint16_t*)BYTES;
    long some = 0;
    for(long at=from; at < to; at++) some += as_it_is[at] != 0;
    printf("            ---- %ld bytes %s, from %04lx ----\n\n", bytes_of(to - from), some ? "that are no word's" : "of nothing", from);
}

static void page_ends(long page, long code_to) {
    const code_page* has = &LAID.pages[page];
    between(code_to, page * PAGE + has->end);
    printf("==== the end of page %ld: %ld bytes left, from %04lx ====\n\n",
           page, bytes_of(PAGE - has->end), page * PAGE + has->end);
}

// The calls from one word to a word on another page: in how many places,
// how often they were run, and how many instructions those runs cost over
// a call within a page, which is one instruction.
typedef struct { long from, to, places, longer; double ran, more; } crossing;

static int most_run(const void* a, const void* b) {
    const crossing* x = a;
    const crossing* y = b;
    if (x->ran != y->ran) return(x->ran < y->ran ? 1 : -1);
    if (x->places != y->places) return(x->places < y->places ? 1 : -1);
    return(x->from != y->from ? (x->from < y->from ? -1 : 1) : (x->to < y->to ? -1 : x->to > y->to));
}

static void name_to(char* out, size_t room, long at) {
    int label;
    const char* name = code_name(at, &label);
    if (name) snprintf(out, room, "%s", name); else snprintf(out, room, "$%04lx", at);
}

// A word that is called from another page: from which pages, in how many
// places of how many words, and how often those calls were run; and the
// same of the calls that reach it with one instruction, from its own page
// or from the kernel's, which are what moving it to another page would
// make longer.
typedef struct {
    long at, far_places, far_words, near_places;
    double far_ran, near_ran;
    unsigned long from_pages;           // a bit for each page that calls it from afar
    long longer;                        // the instructions those places are over calls of one instruction,
    double more;                        // and the instructions the work ran for them over such calls
    long out_places;                    // the calls it makes itself to words of its own page, not the kernel's,
    double out_ran;                     // which would be to another page if it were in the kernel's
} called;

// How long the word at a place is, in instructions.
static long size_at(long at) {
    for(int idx=0; idx < NWORDS; idx++) if (WORDS[idx].at == at && WORDS[idx].size) return(WORDS[idx].size);
    return(0);
}

static int most_run_to(const void* a, const void* b) {
    const called* x = a;
    const called* y = b;
    if (x->far_ran != y->far_ran) return(x->far_ran < y->far_ran ? 1 : -1);
    if (x->far_places != y->far_places) return(x->far_places < y->far_places ? 1 : -1);
    return(x->at < y->at ? -1 : x->at > y->at);
}

static int most_places_to(const void* a, const void* b) {
    const called* x = a;
    const called* y = b;
    if (x->far_places != y->far_places) return(x->far_places < y->far_places ? 1 : -1);
    if (x->far_ran != y->far_ran) return(x->far_ran < y->far_ran ? 1 : -1);
    return(x->at < y->at ? -1 : x->at > y->at);
}

static void show_called(const called* all, long n, const char* by) {
    printf("==== the words called from another page, by %s ====\n", by);
    printf("(its bytes; the pages that call it; in how many places, of how many words%s; what those calls are over\n"
           " calls of one instruction, in bytes of code%s; the calls that reach it with one\n"
           " instruction, from its own page or the kernel's, which moving it to another page would make longer; and the\n"
           " calls it makes itself to words of its own page, which would be to another page if it were in the kernel's)\n\n",
           RAN ? ", run how often" : "", RAN ? " and in instructions the work ran" : "");
    printf("  %-22s page  bytes  from pages      places  words%s   over: bytes%s   with one: places%s   it calls: places%s\n",
           "word", RAN ? "          run" : "", RAN ? "     the work" : "", RAN ? "          run" : "", RAN ? "          run" : "");
    for(long k=0; k < n; k++) {
        char name[NAME_ROOM], pages[8 * sizeof(unsigned long) * 3 + 1];
        name_to(name, sizeof(name), all[k].at);
        int len = 0;
        pages[0] = 0;
        for(int page=0; page < (int)(8 * sizeof(unsigned long)); page++) {
            if (all[k].from_pages >> page & 1) len += sprintf(pages + len, "%s%d", len ? " " : "", page);
        }
        printf("  %-22s %4ld %6ld  %-14s  %6ld %6ld", name, all[k].at / PAGE, bytes_of(size_at(all[k].at)), pages,
               all[k].far_places, all[k].far_words);
        if (RAN) printf(" %12.0f", all[k].far_ran);
        printf("   %11ld", bytes_of(all[k].longer));
        if (RAN) printf(" %12.0f", all[k].more);
        printf("   %16ld", all[k].near_places);
        if (RAN) printf(" %12.0f", all[k].near_ran);
        printf("   %16ld", all[k].out_places);
        if (RAN) printf(" %12.0f", all[k].out_ran);
        printf("\n");
    }
    printf("\n");
}

static void show_crossings(void) {
    crossing* all = NULL;
    long n = 0, room = 0, execute_is = 0;
    for(int idx=0; idx < NWORDS; idx++) if (WORDS[idx].at == EXECUTE && WORDS[idx].size) execute_is = WORDS[idx].size;
    for(int idx=0; idx < NWORDS; idx++) {
        const word* w = &WORDS[idx];
        for(long at=w->at; at < w->at + w->size; at++) {
            long call_at, callee;
            if (!far_call(at, w->at + w->size, &call_at, &callee)) continue;
            long k = 0;
            while (k < n && (all[k].from != w->at || all[k].to != callee)) k++;
            if (k == n) {
                ONE_MORE(all, n, room);
                all[n++] = (crossing){ w->at, callee, 0, 0, 0, 0 };
            }
            double ran = RAN && call_at < RAN_PLACES ? RAN[call_at] : 0;
            all[k].places++;
            all[k].longer += call_at - at;                              // the pushes: the call is there either way
            all[k].ran += ran;
            all[k].more += ran * (double)(call_at - at + execute_is);   // the pushes and execute's own, over the call
            at = call_at;
        }
    }
    qsort(all, n, sizeof(crossing), most_run);
    double work = 0, ran = 0, more = 0;
    long places = 0, longer = 0;
    for(long at=0; RAN && at < RAN_PLACES; at++) work += RAN[at];
    printf("==== calls to a word on another page: the word's place pushed, and a call of execute ====\n\n");
    printf("  %-22s page  %-22s page  places%s\n", "from", "to", RAN ? "          run" : "");
    for(long k=0; k < n; k++) {
        char from[NAME_ROOM], to[NAME_ROOM];
        name_to(from, sizeof(from), all[k].from);
        name_to(to, sizeof(to), all[k].to);
        printf("  %-22s %4ld  %-22s %4ld  %6ld", from, all[k].from / PAGE, to, all[k].to / PAGE, all[k].places);
        if (RAN) printf(" %12.0f", all[k].ran);
        printf("\n");
        places += all[k].places, longer += all[k].longer, ran += all[k].ran, more += all[k].more;
    }
    printf("\n  %ld places in %ld pairs of words, which are %ld bytes of code more than calls within a page would be",
           places, n, bytes_of(longer));
    if (RAN && work > 0) {
        printf("; run %.0f times in the work's %.0f instructions, which is %.0f instructions over calls within a page: "
               "%.2f to each hundred", ran, work, more, 100 * more / work);
    }
    printf("\n\n");

    // The same by the word that is called.  The calls that reach a word
    // with one instruction are counted over all the code: a call, and a
    // jump to another word's start, that is not the call of execute a far
    // call ends with.
    long* near_places = room_for(INSTRUCTIONS + 1, sizeof(long));
    double* near_ran = room_for(INSTRUCTIONS + 1, sizeof(double));
    long* out_places = room_for(INSTRUCTIONS + 1, sizeof(long));        // by the word that calls: to its own page
    double* out_ran = room_for(INSTRUCTIONS + 1, sizeof(double));
    for(int idx=0; idx < NWORDS; idx++) {
        const word* w = &WORDS[idx];
        for(long at=w->at; at < w->at + w->size; at++) {
            long call_at, callee;
            if (far_call(at, w->at + w->size, &call_at, &callee)) { at = call_at; continue; }
            uint16_t c = INSN[at];
            int kind = kind_of(c);
            if (!c || (kind != OP_TYPE_CALL && kind != OP_TYPE_JMP)) continue;
            long to = target_of(at);
            int label;
            if (to < 0 || to >= INSTRUCTIONS || (to >= w->at && to < w->at + w->size)) continue;
            if (kind == OP_TYPE_JMP && !code_name(to, &label)) continue;
            near_places[to]++;
            near_ran[to] += RAN && at < RAN_PLACES ? RAN[at] : 0;
            if (to / PAGE == w->at / PAGE && to / PAGE) {
                out_places[w->at]++;
                out_ran[w->at] += RAN && at < RAN_PLACES ? RAN[at] : 0;
            }
        }
    }
    called* to = NULL;
    long targets = 0, to_room = 0;
    for(long k=0; k < n; k++) {
        long t = 0;
        while (t < targets && to[t].at != all[k].to) t++;
        if (t == targets) {
            ONE_MORE(to, targets, to_room);
            to[targets++] = (called){ all[k].to, 0, 0, near_places[all[k].to], 0, near_ran[all[k].to], 0, 0, 0,
                                      out_places[all[k].to], out_ran[all[k].to] };
        }
        to[t].far_places += all[k].places;
        to[t].far_words++;
        to[t].far_ran += all[k].ran;
        to[t].longer += all[k].longer;
        to[t].more += all[k].more;
        to[t].from_pages |= 1ul << (all[k].from / PAGE);
    }
    if (RAN) {
        qsort(to, targets, sizeof(called), most_run_to);
        show_called(to, targets, "how often those calls are run");
    }
    qsort(to, targets, sizeof(called), most_places_to);
    show_called(to, targets, "how many places call them");
    free(to); free(near_places); free(near_ran); free(out_places); free(out_ran);
    free(all);
}

// What the calls would be were some words in the kernel's page (--near).
// A call into the kernel's page is one instruction from anywhere, so every
// call to a word put there is; but a call from the kernel's page to any
// other is the long one, so every call such a word makes to a word that
// is not put there too becomes one.  For each word: its bytes, how often
// the calls to it that are long now were run, how often the calls it would
// then make long were, and to which words those are, which are what has to
// come with it for the move to be worth making.  Then all of it together.
// It moves nothing: it is asked before an order is changed.
typedef struct { long from, to; double ran; int far; } a_call;

static void show_near(const char** names, int nnames) {
    char* near = room_for(INSTRUCTIONS + 1, 1);         // of each place: a word that is to be put there starts here
    long bytes = 0;
    for(int k=0; k < nnames; k++) {
        int found = 0;
        for(int idx=0; idx < NWORDS; idx++) {
            if (!WORDS[idx].size || strcmp(WORDS[idx].name, names[k])) continue;
            if (!near[WORDS[idx].at]) bytes += bytes_of(WORDS[idx].size);
            near[WORDS[idx].at] = 1;
            found++;
        }
        if (!found) { fprintf(stderr, "disasm_hex: --near: the image has no word %s\n", names[k]); exit(EXIT_FAILURE); }
    }
    a_call* calls = NULL;
    long n = 0, room = 0;
    for(int idx=0; idx < NWORDS; idx++) {
        const word* w = &WORDS[idx];
        for(long at=w->at; at < w->at + w->size; at++) {
            long call_at, callee;
            if (far_call(at, w->at + w->size, &call_at, &callee)) {
                ONE_MORE(calls, n, room);
                calls[n++] = (a_call){ w->at, callee, RAN && call_at < RAN_PLACES ? RAN[call_at] : 0, 1 };
                at = call_at;
                continue;
            }
            uint16_t c = INSN[at];
            int kind = kind_of(c), label;
            if (!c || (kind != OP_TYPE_CALL && kind != OP_TYPE_JMP)) continue;
            long to = target_of(at);
            if (to < 0 || to >= INSTRUCTIONS || (to >= w->at && to < w->at + w->size)) continue;
            if (kind == OP_TYPE_JMP && !code_name(to, &label)) continue;
            ONE_MORE(calls, n, room);
            calls[n++] = (a_call){ w->at, to, RAN && at < RAN_PLACES ? RAN[at] : 0, 0 };
        }
    }
    #define PAGE_THEN(at) (near[at] ? 0 : (at) / PAGE)
    #define FAR_THEN(c) (PAGE_THEN((c)->to) && PAGE_THEN((c)->from) != PAGE_THEN((c)->to))
    long places_now = 0, places_then = 0;
    double ran_now = 0, ran_then = 0;
    for(long k=0; k < n; k++) {
        places_now += calls[k].far, ran_now += calls[k].far ? calls[k].ran : 0;
        places_then += FAR_THEN(&calls[k]), ran_then += FAR_THEN(&calls[k]) ? calls[k].ran : 0;
    }
    printf("==== were these words in the kernel's page: %ld bytes ====\n\n", bytes);
    printf("  %-22s page  bytes   long calls to it: places%s   calls it makes long: places%s   to\n", "word",
           RAN ? "        run" : "", RAN ? "        run" : "");
    for(int idx=0; idx < NWORDS; idx++) {
        const word* w = &WORDS[idx];
        if (!w->size || !near[w->at]) continue;
        long in_places = 0, out_places = 0;
        double in_ran = 0, out_ran = 0;
        for(long k=0; k < n; k++) {
            if (calls[k].to == w->at && calls[k].far) in_places++, in_ran += calls[k].ran;
            if (calls[k].from == w->at && !calls[k].far && FAR_THEN(&calls[k])) out_places++, out_ran += calls[k].ran;
        }
        printf("  %-22s %4ld %6ld   %24ld", w->name, w->at / PAGE, bytes_of(w->size), in_places);
        if (RAN) printf(" %10.0f", in_ran);
        printf("   %26ld", out_places);
        if (RAN) printf(" %10.0f", out_ran);
        printf("  ");
        for(long k=0; k < n; k++) {
            if (calls[k].from != w->at || calls[k].far || !FAR_THEN(&calls[k])) continue;
            int again = 0;                              // a word it calls in several places is said once, with all of them
            for(long j=0; j < k; j++) again |= calls[j].from == w->at && calls[j].to == calls[k].to && !calls[j].far;
            if (again) continue;
            double ran = 0;
            for(long j=k; j < n; j++) if (calls[j].from == w->at && calls[j].to == calls[k].to && !calls[j].far) ran += calls[j].ran;
            char name[NAME_ROOM];
            name_to(name, sizeof(name), calls[k].to);
            printf(" %s", name);
            if (RAN) printf(" %.0f", ran);
        }
        printf("\n");
    }
    printf("\n  calls to a word on another page: %ld places now, %ld then", places_now, places_then);
    if (RAN) printf("; run %.0f times now, %.0f then", ran_now, ran_then);
    printf("\n\n");
    #undef PAGE_THEN
    #undef FAR_THEN
    free(calls);
    free(near);
}

// The data by cell: every word that only pushes where its data is, and
// every label of data, with how many cells there are to the next and
// whether one instruction reaches it (the 4,096 cells from VAR_BASE).
typedef struct { uint64_t cell; char* name; } datum;

static int by_cell(const void* a, const void* b) {
    const datum* x = a;
    const datum* y = b;
    if (x->cell != y->cell) return(x->cell < y->cell ? -1 : 1);
    return(strcmp(x->name, y->name));
}

static void show_data(void) {
    datum* all = NULL;
    long n = 0, room = 0;
    #define A_DATUM(at, called) do { \
        ONE_MORE(all, n, room); \
        all[n].cell = (at); \
        all[n].name = room_for(strlen(called) + 1, 1); \
        strcpy(all[n++].name, (called)); \
    } while (0)
    for(int idx=0; idx < NWORDS; idx++) {               // a word with a header or a label that pushes a place of data
        uint64_t value;
        int label;
        if (!WORDS[idx].size || !pushes_only(WORDS[idx].at, &value) || value < VAR_BASE) continue;
        if (value >= LEN / CELL_BYTES) continue;
        const char* name = code_name(WORDS[idx].at, &label);
        if (name) A_DATUM(value, name);
    }
    for(long idx=0; idx < NDATA; idx++) A_DATUM((uint64_t)DATA[idx].at, DATA[idx].name);
    qsort(all, n, sizeof(datum), by_cell);
    printf("==== the data, by cell: what is at each, the cells to the next, and whether one instruction reaches it ====\n\n");
    printf("    cell   cells  reached  name\n");
    for(long k=0; k < n; k++) {
        if (k && all[k].cell == all[k - 1].cell && !strcmp(all[k].name, all[k - 1].name)) continue;    // said twice: a header and a label
        long next = k + 1;
        while (next < n && all[next].cell == all[k].cell) next++;
        printf("  %6llx", (unsigned long long)all[k].cell);
        if (next < n) printf(" %7llu", (unsigned long long)(all[next].cell - all[k].cell)); else printf("       -");
        printf("  %-7s  %s\n", all[k].cell - VAR_BASE < 4096 ? "yes" : "no", all[k].name);
    }
    printf("\n");
    for(long k=0; k < n; k++) free(all[k].name);
    free(all);
}

// The file of that kind beside an image: its name with that ending.
static char* beside(const char* image, const char* ending) {
    char* path = room_for(strlen(image) + strlen(ending) + 1, 1);
    strcpy(path, image);
    char* dot = strrchr(path, '.');
    strcpy(dot && !strchr(dot, '/') ? dot : path + strlen(path), ending);
    return(path);
}

static int numeric(const char* text, long* value) {
    char* end;
    *value = strtol(text, &end, 0);
    return(*text && !*end);
}

// The image as the machine has it when a file has been typed at the
// nucleus's prompt (test/nucleus.c): what is typed is compiled by the
// nucleus's own compiler, and its words are in the dictionary after.  It
// stops if the nucleus said anything was wrong.  `had` is the places the
// image's words were at before, so that those typed can be told.
static void type_at_it(const char* image, const char* tables, const char* file, long** had, int* nhad) {
    word* before;
    *nhad = words_of(BYTES, LEN, tables, &before);
    *had = room_for(*nhad + 1, sizeof(long));
    for(int idx=0; idx < *nhad; idx++) (*had)[idx] = before[idx].at;
    free(before);

    FILE* in = fopen(file, "r");
    if (!in) { perror(file); exit(EXIT_FAILURE); }
    nucleus_is(image);
    start();
    char* text = NULL;
    long room = 0;
    while (line_of(in, &text, &room)) {
        text[strcspn(text, "\r\n")] = 0;
        line("%s", text);
    }
    fclose(in);
    free(text);
    type();
    if (strstr(said, "error")) {
        fprintf(stderr, "disasm_hex: the nucleus, typed %s, said:\n%s\n", file, said);
        exit(EXIT_FAILURE);
    }
    memcpy(BYTES, machine.memory, LEN);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s IMAGE.hex [NAME... | FROM TO] [LABELS.lst] [--typed FILE.fs] [--counts WORK]"
                        " [--color | --no-color]\n", argv[0]);
        return(1);
    }
    COLOR = isatty(STDOUT_FILENO) && !getenv("NO_COLOR");
    const char* wanted[argc];
    const char* near[argc];
    const char* typed = NULL;
    const char* memory = NULL;
    const char* own = NULL;
    int nwanted = 0, nnear = 0, laid_out = 0, headers = 0;
    long range[2];
    int nrange = 0;
    for(int a=2; a < argc; a++) {
        size_t len = strlen(argv[a]);
        long value;
        if (!strcmp(argv[a], "--color")) COLOR = 1;
        else if (!strcmp(argv[a], "--no-color")) COLOR = 0;
        else if (!strcmp(argv[a], "--layout")) laid_out = 1;
        else if (!strcmp(argv[a], "--headers")) headers = 1;
        else if (!strcmp(argv[a], "--near") && a + 1 < argc) near[nnear++] = argv[++a];
        else if (!strcmp(argv[a], "--typed") && a + 1 < argc) typed = argv[++a];
        else if (!strcmp(argv[a], "--memory") && a + 1 < argc) memory = argv[++a];
        else if (!strcmp(argv[a], "--own") && a + 1 < argc) own = argv[++a];
        else if (!strcmp(argv[a], "--counts") && a + 1 < argc) RAN_PLACES = work_read(argv[++a], &RAN, NULL);
        else if (len > 4 && !strcmp(argv[a] + len - 4, ".lst")) read_listing(argv[a]);
        else if (numeric(argv[a], &value) && nrange < 2) range[nrange++] = value;
        else wanted[nwanted++] = argv[a];
    }
    if (nrange == 1) { fprintf(stderr, "disasm_hex: a range is two places, FROM and TO\n"); return(1); }
    init_opcodes(FORTH_WORDS);

    LEN = load_image(argv[1], &BYTES);
    if (!LEN) { fprintf(stderr, "disasm_hex: %s is not an image\n", argv[1]); return(1); }
    if (memory) {               // the machine's memory when it stopped, in the image's place
        uint8_t* is;
        size_t got = file_bytes(memory, &is);
        if (got < LEN) { fprintf(stderr, "disasm_hex: %s is not that machine's memory\n", memory); return(1); }
        memcpy(BYTES, is, LEN);
        free(is);
    }
    INSTRUCTIONS = LEN / sizeof(uint16_t);
    char* tables = beside(argv[1], ".tables");          // the names beside the image,
    char* sizes = beside(argv[1], ".sizes");            // and the files its code is from
    if (access(sizes, R_OK) == 0) NFILES = files_of(sizes, &FILES);
    long* was = NULL;                                   // with a file typed: where the image's words were before
    int nwas = 0;
    if (typed) type_at_it(argv[1], tables, typed, &was, &nwas);

    PLAIN = room_for(LEN + CELL_BYTES, 1);
    memcpy(PLAIN, BYTES, LEN);
    word* with_tables;
    int had = words_of(BYTES, LEN, tables, &with_tables);
    without_tables(PLAIN, LEN, with_tables, had);
    word* kept = NULL;                                  // with --own: the names the machine kept of what it compiled
    long nkept = 0, kept_room = 0;
    if (own) {
        uint8_t* disk;
        size_t disk_len = file_bytes(own, &disk);
        nkept = kept_of(BYTES, LEN, with_tables, had, disk, disk_len, &kept, 0, &kept_room);
        free(disk);
    }
    free(with_tables);
    NWORDS = words_and(PLAIN, LEN, tables, kept, nkept, &WORDS);
    INSN = (const uint16_t*)PLAIN;
    long data_room;
    NDATA = data_of(tables, &DATA, &data_room);
    read_dictionary();
    if (headers) { show_headers(); return(0); }
    read_fused();
    if (laid_out) {
        if (typed || nwanted || nrange) {
            fprintf(stderr, "disasm_hex: --layout is of the whole image as it was built\n");
            return(1);
        }
        lay_out(&LAID, argv[1], tables);
        printf("%s, cells of %d bits\n\n", argv[1], CELL_BITS);
        printf("pages of code     from   words    code  tables    left\n");
        for(long page=0; page < LAID.npages; page++) {
            const code_page* has = &LAID.pages[page];
            if (!has->words) continue;
            printf("  page %-6ld     %04lx %7ld %7ld %7ld %7ld\n", page, page * PAGE, has->words, bytes_of(has->code),
                   bytes_of(has->tables), bytes_of(PAGE - has->end));
        }
        printf("\n");
        if (nnear) {                                    // only what the calls would be: nothing else is asked
            show_near(near, nnear);
            return(0);
        }
        show_crossings();
        show_data();
    }

    int shown = 0;
    long on_page = -1, code_to = 0;                     // with the layout: the page being shown, and where its code has got to
    for(int idx=0; idx < NWORDS; idx++) {
        const word* w = &WORDS[idx];
        if (!w->size) continue;
        int label;
        const char* name = code_name(w->at, &label);
        int want = !nwanted && !nrange;
        if (want && typed) {                            // with a file typed and no word named: the words it made
            for(int k=0; k < nwas && want; k++) want = was[k] != w->at;
        }
        for(int k=0; k < nwanted && name; k++) want |= !strcmp(name, wanted[k]);
        if (nrange) want |= w->at <= range[1] && w->at + w->size > range[0];
        if (!want) continue;
        if (laid_out) {
            if (w->at / PAGE != on_page) {
                if (on_page >= 0) page_ends(on_page, code_to);
                on_page = w->at / PAGE;
                code_to = on_page * PAGE;
                page_starts(on_page);
            }
            between(code_to, w->at);
            code_to = w->at + w->size;
        }
        show_word(w);
        shown++;
    }
    if (!shown) {
        fprintf(stderr, "disasm_hex: %s has no such word\n", argv[1]);
        return(1);
    }
    if (laid_out) page_ends(on_page, code_to);
    return(0);
}
