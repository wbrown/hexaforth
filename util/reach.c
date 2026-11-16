//
// reach.c - what reaches each word of an image, for the tools and the tests
// that have this in them.  It is had after cut.c, which cuts an image into
// its straight-line runs and the instructions that leave a run.
//
// A word is reached
//
//   by a call, a jump or a branch from another word that goes to where it
//   starts: a call to a word on another page, which is the word's place
//   pushed and a call of execute, is a call of that word (cut.c);
//
//   by its place being pushed as a number, which is how its execution token
//   is said.  A number that only happens to be a word's place is counted
//   too: nothing here can tell the two apart, which is why this is kept
//   apart from the calls;
//
//   by its place being in a cell of data, as a vector's is and a table of
//   words'.  The same goes for a cell that only happens to hold the number;
//
//   and by the word before it running on into it.
//
// A word that goes to its own start, as a loop does, is not reached by
// that.  A word with a header can be typed, whatever else reaches it.
//

typedef struct {
    long calls[2], pushed[2];           // from the kernel, and from the other pages
    long cells;                         // cells of data that hold its place
    int into;                           // the word before runs on into it
    int typed;                          // it has a header
} reached;

// The word that starts at a place: the one of that place that has the
// instructions, where a place has more names than one.  Or less than nothing.
static int starting_at(const cut_image* m, long at) {
    int lo = 0, hi = m->n;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (m->words[mid].at < at) lo = mid + 1; else hi = mid;
    }
    return(lo < m->n && m->words[lo].at == at && m->words[lo].size ? lo : -1);
}

// The word that the instruction at a place is in, or less than nothing.
static int holding(const cut_image* m, long at) {
    int lo = 0, hi = m->n;              // the first word that starts after it
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (m->words[mid].at <= at) lo = mid + 1; else hi = mid;
    }
    if (!lo) return(-1);
    int in = starting_at(m, m->words[lo - 1].at);
    return(in >= 0 && at < m->words[in].at + m->words[in].size ? in : -1);
}

// Does an instruction go on to the one after it?  A jump does not, and
// nor does one with the return in it.
static int runs_on(uint16_t w) {
    instruction ins = ins_of(w);
    if (ins.lit.lit_f) return(1);
    if (ins.jmp.op_type == OP_TYPE_JMP) return(0);
    return(!(ins.alu.op_type == OP_TYPE_ALU && ins.alu.r_eip));
}

// What reaches each word of an image: how[idx] is of its word idx, and
// there are as many as the image has words.
static void reach(const cut_image* m, reached* how) {
    const uint8_t* bytes = (const uint8_t*)m->code;
    size_t len = (size_t)m->instructions * 2;
    memset(how, 0, m->n * sizeof(reached));

    // Calls, jumps and branches from another word; and numbers pushed.
    for(long idx=0; idx < m->count; idx++) {
        const item* it = &m->items[idx];
        int far = it->at / PAGE != 0;
        if (!it->run) {
            int to = it->to >= 0 ? starting_at(m, it->to) : -1;
            if (to >= 0 && to != holding(m, it->at)) how[to].calls[far]++;
            continue;
        }
        const uint16_t* run = m->code + it->at;
        for(long at=0; at < it->len; ) {
            long end = at + 1;
            if (pushes_number(run[at], 0)) {
                while (end < it->len && pushes_number(run[end], 1)) end++;
                long pushed = 0;
                for(long k=at; k < end; k++) pushed += number_in(run[k]);
                int to = pushed > 0 ? starting_at(m, pushed) : -1;
                if (to >= 0) how[to].pushed[far]++;
            }
            at = end;
        }
    }

    // Cells of data: those of the pages that have no code, but for the
    // dictionary, where every header has its word's place.
    static unsigned char has_code[CUT_PLACES / PAGE];
    memset(has_code, 0, sizeof(has_code));
    for(int idx=0; idx < m->n; idx++) {
        if (m->words[idx].at < m->instructions) has_code[m->words[idx].at / PAGE] = 1;
    }
    size_t headers_from = len, headers_to = 0;
    for(uint32_t head = newest_header(bytes, len); head; ) {
        uint64_t xt;
        char name[256];
        size_t end = head + 2 * CELL_BYTES + decode_name(bytes + head + 2 * CELL_BYTES, len - head - 2 * CELL_BYTES, name);
        if (head < headers_from) headers_from = head;
        if (end > headers_to) headers_to = end;
        head = read_header(bytes, len, head, &xt, name);
        int is = xt < CUT_PLACES ? starting_at(m, (long)xt) : -1;
        if (is >= 0) how[is].typed = 1;
    }
    for(size_t at=0; at + CELL_BYTES <= len; at += CELL_BYTES) {
        if (has_code[at / 2 / PAGE] || (at >= headers_from && at < headers_to)) continue;
        uint64_t held = cell_of(bytes, at / CELL_BYTES);
        int to = held && held < CUT_PLACES ? starting_at(m, (long)held) : -1;
        if (to >= 0) how[to].cells++;
    }

    // The word before running on into it.
    for(int idx=0; idx < m->n; idx++) {
        long at = m->words[idx].at;
        if (!m->words[idx].size || at % PAGE == 0 || at >= m->instructions) continue;
        how[idx].into = holding(m, at - 1) >= 0 && runs_on(m->code[at - 1]);
    }
}

// Does anything come to a word, other than its being typed?
static int is_reached(const reached* how) {
    return(how->calls[0] || how->calls[1] || how->pushed[0] || how->pushed[1] || how->cells || how->into);
}
