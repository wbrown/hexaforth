//
// disasm_hex IMAGE.hex [NAME... | FROM TO] [LABELS.lst] [--typed FILE.fs] [--counts WORK]
//                      [--layout [--near NAME]...] [--memory FILE [--own DISK.img]] [--headers]
//                      [--color | --no-color]
//
// Disassembles an image's code word by word, laid out the way the
// decompiler shows it (forth/see.fs), with each instruction's encoding
// beside it.
//
// --layout prints the whole image as it is laid out, for someone looking
// for a better layout.  It prints, in order:
//
//   The pages of code, as `examine` reports them.
//
//   The calls to a word on another page.  Such a call is a push and a call
//   of `execute`, where a call within a page or into the kernel's page is
//   one instruction.  With --counts, each call has the number of times it
//   ran, and the list ends with the share of the work those calls are.
//
//   The words that are called that way, ordered by how often those calls
//   ran, and again by how many places make them.  Beside each word are the
//   calls that reach it in one instruction, which would get longer if the
//   word moved.
//
//   The data by cell: each variable and table, the number of cells to the
//   next one, and whether one instruction reaches it.
//
//   Every word in address order.  A line marks where each page starts and
//   ends, with what the page holds and how much space it has left.  A line
//   also marks any gap between two words, which is zeros or bytes that
//   belong to no word.
//
// The targets listing-64 and the like write this (cmake/machines.cmake).
// With --near NAME, given once for each word, it prints instead what the
// calls would be if those words were in the kernel's page (show_near(),
// below, describes the output).  Use it before moving a word there.
//
// --memory FILE reads the machine's memory from FILE in place of the
// image.  FILE is the memory the reference VM wrote when it stopped.  It
// contains the code the machine compiled, which is shown under the names
// the machine's dictionary still has.  With --layout, the calls that code
// makes to other pages are counted together with the image's.
//
// --own DISK.img, with --memory, names the disk that machine booted from,
// as the machine left it.  The machine saves each file's private names on
// that disk (forth/files.fs), so the words its boot compiled under a
// private name are shown with their names too.
//
// --headers prints only the dictionary, from the header a search starts at
// to the header where the chain ends (show_headers()).
//
// --typed FILE.fs shows the image as the machine has it after FILE.fs has
// been typed at the nucleus's prompt.  The words the file defines are then
// present as the nucleus's own compiler compiled them.  If no word is
// named, those are the words shown.
//
// --counts WORK prints beside each instruction the number of times it ran.
// WORK is the file of counts by address that the counting reference VM
// writes (RUN_WORK, bench/counting.c).
//
// Each word starts with its name, or with all of its names if it has more
// than one header; naming any of them on the command line selects it.  Its
// code is laid out like source.  The body of an `if` or a `begin` is
// indented, and branches are shown as `if`, `else`, `then`, `begin`,
// `until`, `again`, `while` and `repeat`, which are worked out from the
// branch targets the way see.fs does it.  Each instruction is one line:
// its address, its value, the words for it, and its fields under the names
// vm_opcodes.h gives them.  An instruction is shown as follows:
//
//   a call            the word it calls, by its header or its label
//   a literal         the word that only pushes that number (a variable,
//                     or anything made with `create`), or the data label
//                     at that address; or ['] and a word, if the number is
//                     an execution token; or the number itself
//   a variable        a variable fetched or stored by one instruction, by
//                     its name
//   a call of execute after a literal that is a word's address: that word,
//                     which is on another page
//   an ALU            the words the image's own table has for it (`fused`,
//   instruction       made by util/fused.c, which gets them from the
//                     machine); or else its name in vm_opcodes.h; or else
//                     its value
//   a return          it is folded into the instruction before it, and is
//                     shown as `exit`, or as `;` at the end of the word
//
// The names come from the image's dictionary and from the labels in the
// names file that goes with it (IMAGE.tables, which util/words.h reads).
// The source file each word's code belongs to comes from the image's
// .sizes file (bench/files.h).  A listing (.lst) may also be given, for
// any names it has for code addresses.  util/words.h determines how long
// each word is.  With no word named and no range, every word of the image
// is shown.  A range selects the words that have code inside it.
//
// On a terminal the output is in colour, unless `NO_COLOR` is set or
// `--no-color` is given: a called word in cyan, and in italics if its name
// is a label; numbers in yellow; variables and data in magenta; control
// flow words in red; and the several words of a single instruction
// underlined together.  The address and the fields are dim.
//

#include <unistd.h>
#include "../vm.c"
#include "image.h"
#include "words.h"
#include "layout.h"
#include "../bench/files.h"
#include "../bench/work.h"

// The parts of test/nucleus.c used here.  That file runs the nucleus and
// types at its prompt.  They are declared here because nucleus.h cannot be
// included: it declares a function with the same name as one in vm.c,
// which this file includes.
extern context machine;
extern char* said;
void nucleus_is(const char* path);
void start(void);
void line(const char* format, ...);
void type(void);

static int COLOR;
static double* RAN;             // the number of times the instruction at each address ran, or NULL without --counts
static long RAN_PLACES;         // the number of addresses RAN covers

// ---- the image

static uint8_t* BYTES;          // the image's bytes
static size_t LEN;
static const uint16_t* INSN;    // the same bytes as instructions
static long INSTRUCTIONS;
static word* WORDS;             // the image's words, from its headers and its labels (words.h)
static int NWORDS;
static word* DATA;              // its data labels
static long NDATA;
static span* FILES;             // the source files its code comes from (bench/files.h)
static int NFILES;

typedef struct { long at; char* name; } named;
static named* LISTED;           // names from a listing (.lst), for addresses that have no label
static long NLISTED, LISTED_ROOM;

typedef struct { long xt; long head; int pushes; uint64_t value; } header;
static header* HEADERS;         // the dictionary: for each header, the word's address, the header's own address,
static long NHEADERS, HEADER_ROOM;  // and the number the word pushes, if pushing it is all the word does

typedef fused_entry fusion;
static fusion* FUSED;           // the image's own table of the words for each instruction
static long NFUSED;

static long EXECUTE = -1;       // the address of `execute`; a pushed address and a call of it is a call to another page

static long target_of(long at) { return((long)branch(at + 1, INSN[at] & 0x1fff)); }
static int is_literal(uint16_t w) { return(w & 0x8000); }
static int is_variable(uint16_t w) { return(is_literal(w) && (w & 0x3000) == 0x3000); }
static int adds(uint16_t w) { return(is_literal(w) && !is_variable(w) && (w & 0x4000)); }
static int pushes(uint16_t w) { return(is_literal(w) && !is_variable(w) && !(w & 0x4000)); }
static uint64_t number_in(uint16_t w) { return((uint64_t)(w & 0xfff) << (((w >> 12) & 3) * LIT_BITS)); }
static int kind_of(uint16_t w) { return(is_literal(w) ? -1 : (w >> 13) & 3); }     // the OP_TYPE, or -1 for a literal

// Tests whether the code at an address only pushes a number: a literal,
// any literals that add to it, and then a plain exit (see.fs, hval).  If
// so, stores the number in `*value` and returns 1.
static int pushes_only(long at, uint64_t* value) {
    if (at < 0 || at >= INSTRUCTIONS || !pushes(INSN[at])) return(0);
    uint64_t n = number_in(INSN[at]);
    for(at++; at < INSTRUCTIONS && adds(INSN[at]); at++) n += number_in(INSN[at]);
    *value = n;
    return(at < INSTRUCTIONS && INSN[at] == 0x7403);
}

// Reads the dictionary into HEADERS, in the order it is searched, and
// finds the address of `execute`.
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

// Prints the dictionary in the order it is searched, starting from the
// header a search starts at.  Each line has the header's cell address, its
// link (the cell address of the header searched next), the word's
// address, and the name.  The last line is the header where the chain
// ends.  In an image that header's link is 0.  In a machine's memory, a
// chain that was broken ends at the header with the bad link, and the
// line shows that link.
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

static int immediate_head(long head) {
    return((cell_of(BYTES, head / CELL_BYTES + 1) >> (CELL_BITS - 1)) & 1);
}

static int immediate(long xt) {
    for(long idx=0; idx < NHEADERS; idx++) if (HEADERS[idx].xt == xt) return(immediate_head(HEADERS[idx].head));
    return(0);
}

// Reads the decompiler's table (fused.fs) from the image (words.h).
static void read_fused(void) {
    NFUSED = fused_of(BYTES, LEN, WORDS, NWORDS, &FUSED);
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

// Returns 1 if a header for the word at that address has that name.
static int headed_as(long at, const char* name) {
    for(long h=0; h < NHEADERS; h++) if (HEADERS[h].xt == at && !strcmp(name_at_head(HEADERS[h].head), name)) return(1);
    return(0);
}

// Returns the name of the word that starts at an address, or NULL if no
// word starts there.  `*label` is set to 1 if the name is a label and to 0
// if it is a header's.  Where an address has both a label and a header,
// the label names the word, and words.h sorts it first there (by_place).
// The header then belongs to a word with no instructions that falls
// through into this one.
static const char* code_name(long at, int* label) {
    int lo = 0, hi = NWORDS;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (WORDS[mid].at < at) lo = mid + 1; else hi = mid;
    }
    for(int idx=lo; idx < NWORDS && WORDS[idx].at == at; idx++) {
        *label = !headed_as(at, WORDS[idx].name);
        if (!*label) {                                  // return the name of the first header for this address
            for(long h=0; h < NHEADERS; h++) if (HEADERS[h].xt == at) return(name_at_head(HEADERS[h].head));
        }
        return(WORDS[idx].name);
    }
    for(long idx=0; idx < NLISTED; idx++) {
        if (LISTED[idx].at == at) { *label = 1; return(LISTED[idx].name); }
    }
    return(NULL);
}

// Returns 1 if `name` is one of the names of the word that starts at an
// address.  A word can have more than one header (`1+` and `cell+`,
// nuc.fs), and each is a name for it.  Any of them selects the word, and
// code_name() returns the first.
static int is_named(long at, const char* name) {
    for(long h=0; h < NHEADERS; h++) if (HEADERS[h].xt == at && !strcmp(name_at_head(HEADERS[h].head), name)) return(1);
    for(int idx=0; idx < NWORDS; idx++) if (WORDS[idx].at == at && !strcmp(WORDS[idx].name, name)) return(1);
    for(long idx=0; idx < NLISTED; idx++) if (LISTED[idx].at == at && !strcmp(LISTED[idx].name, name)) return(1);
    return(0);
}

// ---- output: a line is built from pieces, each with a style

enum { PLAIN_STYLE, DIM, CALL, LABEL, NUMBER, DATA_STYLE, DATA_LABEL, STEER, UNDER, RAW, HEAD, HEAD_LABEL };
static const char* LOOK[] = {
    "", "\033[2m", "\033[36m", "\033[3;36m", "\033[33m", "\033[35m", "\033[3;35m", "\033[1;31m", "\033[4;32m",
    "\033[2m", "\033[1;36m", "\033[1;3;36m" };

static char* LINE;
static long LINE_ROOM, LINE_LEN, SHOWN;         // the line's buffer size, its length, and its width without the colour codes

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
    int len = vsnprintf(NULL, 0, format, args);         // measure the text first, then format it
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

#define SOURCE_AT 13            // the column where the source starts
#define FIELDS_AT 58            // the column where the fields start
#define RAN_AT    96            // the column where the run count starts

// Prints an instruction's fields under the names vm_opcodes.h gives them,
// and then the number of times it ran, if --counts was given.
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
        static const int DELTA[] = { 0, 1, 3, 2 };     // maps a stack field's value to its index in the table of names
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

// Prints the name of the word at an address, or the address if it has no
// name.
static void link_to(long at, int style_if_named) {
    int label;
    const char* name = code_name(at, &label);
    if (name) put(label ? LABEL : style_if_named, name);
    else putf(RAW, "$%04lx", at);
}

// Prints a number as what it refers to (see.fs, snum): the word that
// pushes it, the data label at that address, ['] and a word if it is an
// execution token, or else the number itself.
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
    if (n >= 4095) putf(DIM, " $%llx", (unsigned long long)n);    // a mask or an address is easier to read in hex
}

// Prints the words for an ALU instruction that does not return.
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

// Tests whether a call to a word on another page starts at `at` and ends
// before `to`.  Such a call is the word's address pushed, in one
// instruction or more, followed by a call of `execute`, or by a jump to
// `execute` where the call is the last thing a word does.  If so, stores
// the address of the call of `execute` in `*call_at` and the word's
// address in `*callee`, and returns 1.
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

// ---- a word: its branch targets first, then each instruction

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

    // Classify the word's branches by their targets, as see.fs does in its
    // first pass:
    //
    //   a conditional branch forward is an `if`, or a `while` if the
    //   instruction before its target is a jump backward
    //   a jump forward from just before an `if`'s target is an `else`
    //   a conditional branch backward is an `until`
    //   a jump backward within the word is a `repeat` if a `while` lands
    //   just after it, and an `again` otherwise
    //   any other jump goes to another word, and does not return
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
    putf(DIM, "%ld instructions from %04lx%s%s%s", w->size, from, !label && immediate(from) ? ", immediate" : "",
         file ? ", " : "", file ? file : "");
    line_done();
    // Print the word's other headers.  Each is a second name for the same
    // instructions.  Where a label names the word, every header at the
    // address is printed: each belongs to a word with no instructions that
    // falls through into this one.
    for(long h=0, headed=label; h < NHEADERS; h++) {
        if (HEADERS[h].xt != from || !headed++) continue;
        put(DIM, "            ");
        put(STEER, ":");
        put(PLAIN_STYLE, " ");
        put(HEAD, name_at_head(HEADERS[h].head));
        to_column(FIELDS_AT);
        putf(DIM, "%s%s", label ? "no instructions: it runs on into this word" : "the same word",
             label && immediate_head(HEADERS[h].head) ? ", immediate" : "");
        line_done();
    }

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
            // A number built by several literals is shown once, on the
            // line of the first.  A word's address pushed and followed by
            // a call of `execute` is shown as that word.
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
            } else if ((c & 0x1000) && (c & 3) == 3) {  // an instruction with a return folded into it
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
    for(int n=count_at(&thens, to); n > 0; n--) {       // a `then` whose target is the end of the word
        indent--;
        START_LINE(-1); put(STEER, "then"); line_done();
    }
    line_done();
    free(role); free(thens.at); free(begins.at); free(whiles.at);
}

// ---- the layout: where each page starts and ends and the space it has
// left, the calls to other pages, and the data by cell

static layout LAID;             // the image's layout (layout.h), which is what `examine` prints

static long bytes_of(long instructions) { return(instructions * (long)sizeof(instruction)); }

static void page_starts(long page) {
    const code_page* has = &LAID.pages[page];
    printf("==== page %ld, from %04lx: %ld words, %ld bytes of code; %ld bytes left ====\n\n",
           page, page * PAGE, has->words, bytes_of(has->code), bytes_of(PAGE - has->end));
}

// Prints a line for the gap between two words of a page, or after the
// page's last word.  The gap is either zeros or bytes that belong to no
// word: a table, or the two instructions an image starts with.
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

// The calls from one word to one word on another page:
//
//   places   how many places make the call
//   longer   how many more instructions those places take than calls
//            within a page, which are one instruction each
//   ran      how many times the calls ran
//   more     how many more instructions ran because of them
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

// A word that is called from another page.  The far_ fields describe the
// calls from other pages: how many places make them, in how many words,
// and how often they ran.  The near_ fields describe the calls that reach
// the word in one instruction, from its own page or from the kernel's.
// Those are the calls that would get longer if the word moved to another
// page.
typedef struct {
    long at, far_places, far_words, near_places;
    double far_ran, near_ran;
    unsigned long from_pages;           // one bit for each page that makes a far call to it
    long longer;                        // how many more instructions the far calls take than one-instruction calls
    double more;                        // how many more instructions ran because of them
    long out_places;                    // the calls it makes to words on its own page (not the kernel's page),
    double out_ran;                     // which would become far calls if it moved to the kernel's page
} called;

// Returns the length in instructions of the word at an address.
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
            all[k].longer += call_at - at;                              // count the pushes only: a near call has the call instruction too
            all[k].ran += ran;
            all[k].more += ran * (double)(call_at - at + execute_is);   // the pushes, plus the instructions of `execute` itself
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

    // The same calls again, grouped by the word that is called.  The calls
    // that reach a word in one instruction are counted over all the code.
    // They are every call, and every jump to the start of another word,
    // except the call of `execute` that ends a far call.
    long* near_places = room_for(INSTRUCTIONS + 1, sizeof(long));
    double* near_ran = room_for(INSTRUCTIONS + 1, sizeof(double));
    long* out_places = room_for(INSTRUCTIONS + 1, sizeof(long));        // indexed by the calling word: its calls to words on its own page
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

// Prints what the calls would be if the named words were in the kernel's
// page (--near).  A call into the kernel's page is one instruction from
// any page, so every call to a word placed there becomes a near call.  A
// call from the kernel's page to any other page is a far call, so every
// call such a word makes to a word that is not placed there too becomes a
// far call.  For each named word it prints:
//
//   its size in bytes
//   the far calls to it now: how many places, and how often they ran
//   the calls it makes that would become far: how many places, and how
//     often they ran
//   the words those calls go to.  These are the words that have to move
//     with it for the move to pay.
//
// The last line gives the totals for the whole image, now and after the
// move.  Nothing is moved: this is a report to read before the order of
// the source is changed.
typedef struct { long from, to; double ran; int far; } a_call;

static void show_near(const char** names, int nnames) {
    char* near = room_for(INSTRUCTIONS + 1, 1);         // 1 at the address of each word that would be placed in the kernel's page
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
            int again = 0;                              // a word it calls from several places is listed once, with their total
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

// Prints the data in cell order: every word that only pushes the address
// of its data, and every data label, with the number of cells to the next
// one and whether one instruction reaches it.  One instruction reaches
// the 4,096 cells starting at VAR_BASE.
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
    for(int idx=0; idx < NWORDS; idx++) {               // each named word that only pushes the address of some data
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
        if (k && all[k].cell == all[k - 1].cell && !strcmp(all[k].name, all[k - 1].name)) continue;    // skip a duplicate: the same name from a header and from a label
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

// Returns the path of a file that goes with an image: the image's path
// with its extension replaced by `ending`.
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

// Types a file at the nucleus's prompt (test/nucleus.c), and replaces the
// image with the machine's memory afterwards.  The nucleus's own compiler
// compiles what is typed, and the words it defines are then in the
// dictionary.  The program stops if the nucleus reported an error.  `*had`
// receives the addresses the image's words had before the file was typed,
// so that the newly defined words can be told apart from them.
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
    if (memory) {               // use the machine's memory from when it stopped, in place of the image
        uint8_t* is;
        size_t got = file_bytes(memory, &is);
        if (got < LEN) { fprintf(stderr, "disasm_hex: %s is not that machine's memory\n", memory); return(1); }
        memcpy(BYTES, is, LEN);
        free(is);
    }
    INSTRUCTIONS = LEN / sizeof(uint16_t);
    char* tables = beside(argv[1], ".tables");          // the names file that goes with the image
    char* sizes = beside(argv[1], ".sizes");            // the file that lists the source files its code comes from
    if (access(sizes, R_OK) == 0) NFILES = files_of(sizes, &FILES);
    long* was = NULL;                                   // with --typed: the addresses the image's words had before the file was typed
    int nwas = 0;
    if (typed) type_at_it(argv[1], tables, typed, &was, &nwas);

    word* kept = NULL;                                  // with --own: the names the machine saved for the words it compiled
    long nkept = 0, kept_room = 0;
    if (own) {
        word* its_own;                                  // the words the image has without those names
        int had = words_of(BYTES, LEN, tables, &its_own);
        uint8_t* disk;
        size_t disk_len = file_bytes(own, &disk);
        nkept = kept_of(BYTES, LEN, its_own, had, disk, disk_len, &kept, 0, &kept_room);
        free(disk);
        free(its_own);
    }
    NWORDS = words_and(BYTES, LEN, tables, kept, nkept, &WORDS);
    INSN = (const uint16_t*)BYTES;
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
        printf("pages of code     from   words    code    left\n");
        for(long page=0; page < LAID.npages; page++) {
            const code_page* has = &LAID.pages[page];
            if (!has->words) continue;
            printf("  page %-6ld     %04lx %7ld %7ld %7ld\n", page, page * PAGE, has->words, bytes_of(has->code),
                   bytes_of(PAGE - has->end));
        }
        printf("\n");
        if (nnear) {                                    // --near prints only the report on the calls
            show_near(near, nnear);
            return(0);
        }
        show_crossings();
        show_data();
    }

    int shown = 0;
    long on_page = -1, code_to = 0;                     // with --layout: the page being printed, and the address its code has reached
    for(int idx=0; idx < NWORDS; idx++) {
        const word* w = &WORDS[idx];
        if (!w->size) continue;
        int label;
        const char* name = code_name(w->at, &label);
        int want = !nwanted && !nrange;
        if (want && typed) {                            // with --typed and no word named, show only the words the file defined
            for(int k=0; k < nwas && want; k++) want = was[k] != w->at;
        }
        for(int k=0; k < nwanted && name; k++) want |= is_named(w->at, wanted[k]);
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
