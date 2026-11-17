//
// disasm_hex IMAGE.hex [NAME... | FROM TO] [LABELS.lst] [--color | --no-color]
//
// An image's code, word by word, as the decompiler shows it (forth/see.fs),
// with the machine's form of every instruction beside it.
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
// beside it (IMAGE.tables, which util/words.c reads), and the file a
// word's code is from is from the .sizes beside it (bench/files.c).  A
// listing (.lst) may be given too, for whatever it names at a place of
// code.  How long a word is, is util/words.c's to say.
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
#include "words.c"
#include "../bench/files.c"

static int COLOR;

// ---- the image

static uint8_t* BYTES;          // the image as it is,
static uint8_t* PLAIN;          // and with the nucleus's tables made nothing
static size_t LEN;
static const uint16_t* INSN;    // its instructions
static long INSTRUCTIONS;
static word* WORDS;             // its words, by header and by label (words.c)
static int NWORDS;
static word* DATA;              // its labels of data
static long NDATA;
static span* FILES;             // the source files its code is from (bench/files.c)
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
    for(uint32_t head = newest_header(BYTES, LEN); head; ) {
        uint64_t xt;
        char name[NAME_ROOM];
        uint32_t before = read_header(BYTES, LEN, head, &xt, name);
        ONE_MORE(HEADERS, NHEADERS, HEADER_ROOM);
        header* h = &HEADERS[NHEADERS++];
        h->xt = (long)xt, h->head = head;
        h->pushes = pushes_only(h->xt, &h->value);
        if (!strcmp(name, "execute")) EXECUTE = h->xt;
        head = before;
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

// The table `fused` (see.fs): cells of the instruction, how many words,
// and the address of each word's header, to a cell that is nothing.
static void read_fused(void) {
    long at = place_of(WORDS, NWORDS, "fused");
    uint64_t cells;
    if (at < 0 || !pushes_only(at, &cells)) return;
    size_t room = LEN / CELL_BYTES;
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
#define FIELDS_AT 58            // and where the fields do

// An instruction's fields, as vm_opcodes.h names them.
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
            long end = at + 1;
            while (end < to && adds(INSN[end])) n += number_in(INSN[end++]);
            int far = end < to && EXECUTE >= 0 && (kind_of(INSN[end]) == OP_TYPE_CALL || kind_of(INSN[end]) == OP_TYPE_JMP) &&
                      INSN[end] && target_of(end) == EXECUTE && n < (uint64_t)INSTRUCTIONS && code_name((long)n, &label);
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

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s IMAGE.hex [NAME... | FROM TO] [LABELS.lst] [--color | --no-color]\n", argv[0]);
        return(1);
    }
    COLOR = isatty(STDOUT_FILENO) && !getenv("NO_COLOR");
    const char* wanted[argc];
    int nwanted = 0;
    long range[2];
    int nrange = 0;
    for(int a=2; a < argc; a++) {
        size_t len = strlen(argv[a]);
        long value;
        if (!strcmp(argv[a], "--color")) COLOR = 1;
        else if (!strcmp(argv[a], "--no-color")) COLOR = 0;
        else if (len > 4 && !strcmp(argv[a] + len - 4, ".lst")) read_listing(argv[a]);
        else if (numeric(argv[a], &value) && nrange < 2) range[nrange++] = value;
        else wanted[nwanted++] = argv[a];
    }
    if (nrange == 1) { fprintf(stderr, "disasm_hex: a range is two places, FROM and TO\n"); return(1); }
    init_opcodes(FORTH_WORDS);

    LEN = load_image(argv[1], &BYTES);
    if (!LEN) { fprintf(stderr, "disasm_hex: %s is not an image\n", argv[1]); return(1); }
    INSTRUCTIONS = LEN / sizeof(uint16_t);
    char* tables = beside(argv[1], ".tables");          // the names beside the image,
    char* sizes = beside(argv[1], ".sizes");            // and the files its code is from
    if (access(sizes, R_OK) == 0) NFILES = files_of(sizes, &FILES);

    PLAIN = room_for(LEN + CELL_BYTES, 1);
    memcpy(PLAIN, BYTES, LEN);
    word* with_tables;
    int had = words_of(BYTES, LEN, tables, &with_tables);
    without_tables(PLAIN, LEN, with_tables, had);
    free(with_tables);
    NWORDS = words_of(PLAIN, LEN, tables, &WORDS);
    INSN = (const uint16_t*)PLAIN;
    long data_room;
    NDATA = data_of(tables, &DATA, &data_room);
    read_dictionary();
    read_fused();

    int shown = 0;
    for(int idx=0; idx < NWORDS; idx++) {
        const word* w = &WORDS[idx];
        if (!w->size) continue;
        int label;
        const char* name = code_name(w->at, &label);
        int want = !nwanted && !nrange;
        for(int k=0; k < nwanted && name; k++) want |= !strcmp(name, wanted[k]);
        if (nrange) want |= w->at <= range[1] && w->at + w->size > range[0];
        if (!want) continue;
        show_word(w);
        shown++;
    }
    if (!shown) {
        fprintf(stderr, "disasm_hex: %s has no such word\n", argv[1]);
        return(1);
    }
    return(0);
}
