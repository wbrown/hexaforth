// tb.cpp - Verilator test bench for hexaforth.v.
//
//   tb cases CASES.bin     replay the cases dumped by baremetal/difftest_dump
//                          (every test in test/tests.h, every ALU encoding,
//                          paged code) and compare the halted state with vm.c
//   tb run IMAGE.hex [DISK.img [MEMORY.out]]
//                          boot an image; stdin is the UART; prints what it
//                          sends, then cycle and instruction counts.
//                          DISK.img is a file that serves as the disk, or
//                          several files separated by commas that serve as
//                          disk 0, disk 1 and so on.  A disk the machine
//                          wrote a block to is written back to its file
//                          when the machine has stopped.  MEMORY.out is
//                          the file the memory is written to then
//
// The memory here is a plain array with the timing the CPU expects: one
// read port, whose address is sampled on the clock edge and whose data
// comes back in the next cycle (or the one after, where LAT is 2), and one
// write port.  rtl/hf_mem.v, the real memory, is checked against the same
// model by tb_mem.cpp.
//
// "run" is also used to time a piece of work (bench/).  Each time the
// machine sends the character MARK, the test bench records the cycles and
// instructions so far.  At the end it prints the cycles and instructions
// between each mark and the next, with the memory accesses made in that
// time: instructions fetched, cells read, and cells stored.  So
//     30 emit  the work  30 emit
// times the work.
//
// The CPU has no port for a disk, and the system reads a file one block at
// a time, as each block is needed.  So the disk is reached over the
// console's line.  A word that is put in the vector 'blk-read sends the
// character ASK and then eight more bytes: the block's number and the
// address of the cell the block goes to, four bytes each, lowest byte
// first.  When the eighth byte has arrived, the test bench puts the
// block's bytes in memory starting at that cell, as the reference VM's
// disk does.  A word that is put in the vector 'blk-write sends the
// character GIVE and the same eight bytes, and the test bench then puts
// the block's bytes, from memory starting at that cell, on the disk.  A
// read or a write costs the CPU what sending nine characters costs, and
// the disk itself takes no time.  MARK, ASK, GIVE and the eight bytes
// after ASK or GIVE are not printed.  A block's number says which disk the
// block is on, as it does for any driver (MODEL.md): the top four of its
// sixteen bits are the disk.  A block of a disk that is not present reads
// as zeros, and a write to such a disk does nothing.

#include "Vhexaforth.h"
#include "Vhexaforth___024root.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// Built once for each machine size: W is the cell width and AB the log2 of
// the memory size in cells.
#ifndef W
#define W 64
#endif
#ifndef AB
#define AB 14
#endif
static const size_t CELLS = (size_t)1 << AB;
static const size_t MEM = CELLS * (W / 8);
static const uint64_t CELL_MASK = W == 64 ? ~0ull : (1ull << W) - 1;

#define MARK        30      // the character that tells the test bench to record a mark
#define ASK         29      // the character that asks the disk for a block
#define GIVE        28      // the character that gives the disk a block to keep
#define DISK_BLOCK  1024    // the number of bytes in a block
#define DISK_BLOCK_BITS 12  // the bits of a block's number that are the block; the four bits above them are the disk.
                            // DISK_BLOCK and DISK_BLOCK_BITS repeat the values in vm_constants.h

struct Mark { uint64_t cycles, executed, fetches, reads, writes; };

struct Machine {
    Vhexaforth top;
    std::vector<uint8_t> mem = std::vector<uint8_t>(MEM);
    std::vector<uint8_t> in, out;
    size_t in_at = 0;
    uint64_t rdata = 0, rnext = 0;
    uint64_t cycles = 0, executed = 0;
    // Counts of memory accesses, by kind: an instruction fetched (the
    // CPU's k1 says the read it made was for an instruction), a cell of
    // data read (the CPU wanted `a`, or it wanted `b` and the read really
    // happened), and a cell stored.
    uint64_t fetches = 0, reads = 0, writes = 0;
    bool timing = false;            // true for "run": marks are recorded, and the disk answers
    std::vector<Mark> marks;
    std::vector<std::vector<uint8_t>> disks;
    std::vector<bool> written;      // for each disk, true if the machine wrote a block to it
    int asked = -1;                 // the number of the eight bytes after ASK or GIVE that have arrived, or -1 when no request is under way
    bool giving = false;            // true if the request under way began with GIVE
    uint8_t ask[8];

    // Handles a character the machine sent.  Returns true if it was a mark
    // or part of a request to the disk, which means it is not console
    // output.
    bool taken(uint8_t c) {
        if (!timing) return false;
        if (asked < 0) {
            if (c == MARK) {
                marks.push_back({cycles, executed, fetches, reads, writes});
                return true;
            }
            if ((c != ASK && c != GIVE) || disks.empty()) return false;
            asked = 0;
            giving = c == GIVE;
            return true;
        }
        ask[asked++] = c;
        if (asked == 8) {
            uint64_t blk = 0, cell = 0;
            for (int i = 3; i >= 0; i--) { blk = blk << 8 | ask[i];  cell = cell << 8 | ask[4 + i]; }
            size_t drive = blk >> DISK_BLOCK_BITS & 15;
            size_t on_disk = (blk & ((1 << DISK_BLOCK_BITS) - 1)) * DISK_BLOCK, in_memory = (cell % CELLS) * (W / 8);
            if (giving) {
                if (drive < disks.size() && in_memory + DISK_BLOCK <= MEM) {
                    std::vector<uint8_t> &disk = disks[drive];
                    if (disk.size() < on_disk + DISK_BLOCK) disk.resize(on_disk + DISK_BLOCK);
                    memcpy(&disk[on_disk], &mem[in_memory], DISK_BLOCK);
                    written[drive] = true;
                }
            } else {
                static const std::vector<uint8_t> none;
                const std::vector<uint8_t> &disk = drive < disks.size() ? disks[drive] : none;
                for (size_t i = 0; i < DISK_BLOCK && in_memory + i < MEM; i++)
                    mem[in_memory + i] = on_disk + i < disk.size() ? disk[on_disk + i] : 0;
            }
            asked = -1;
        }
        return true;
    }

    // Returns the cell at an address.  If `slot` is not 0, the cell's low
    // 16 bits are replaced by the instruction in that 16-bit slot of the
    // cell.
    uint64_t read(uint64_t a, int slot) {
        uint64_t v = 0;
        memcpy(&v, &mem[(a % CELLS) * (W / 8)], W / 8);
        if (slot) v = (v & ~0xffffull) | (v >> (16 * slot) & 0xffff);
        return v;
    }

    void tick() {
        top.rdata = rdata;
        top.rpre = LAT == 2 ? rnext : rdata;
        top.tx_ready = 1;
        top.rx_valid = in_at < in.size();
        top.rx_data = in_at < in.size() ? in[in_at] : 0;
        top.clk = 0;
        top.eval();
        uint64_t next_rdata = top.rok ? read(top.raddr, top.rslot) : 0;
        if (top.we) {
            uint64_t v = top.dwdata;
            memcpy(&mem[((uint64_t)top.waddr % CELLS) * (W / 8)], &v, W / 8);
        }
        if (top.tx_valid && !taken(top.tx_data)) out.push_back(top.tx_data);
        if (top.rx_ready) in_at++;
        executed += top.dbg_exec;
        reads += top.rootp->hexaforth__DOT__want_a || (top.rootp->hexaforth__DOT__want_b && top.rok);
        writes += top.we;
        top.clk = 1;
        top.eval();
        fetches += top.rootp->hexaforth__DOT__k1 == 1;      // K_INS
#if LAT == 2
        rdata = rnext;
        rnext = next_rdata;
#else
        rdata = next_rdata;
#endif
        cycles++;
    }

    void reset(uint32_t pc) {
        top.rst = 1;
        top.start_pc = pc;
        rdata = 0;
        rnext = 0;
        tick();
        top.rst = 0;
        out.clear();
        in_at = 0;
    }

    bool run(uint64_t limit) {
        for (uint64_t i = 0; i < limit && !top.halted; i++) tick();
        tick();                     // let a store by the last instruction land
        return top.halted;
    }

    // dcell() returns cell i of the data stack and rcell() cell i of the
    // return stack, counted from the bottom.  The top cells are in
    // registers, and the rest are in the stack memories at index i.
    int64_t dcell(int i) {
        int sp = top.dbg_dsp;
        if (i == sp - 1) return top.dbg_t;
        if (i == sp - 2) return top.dbg_n;
        if (i == sp - 3) return top.rootp->hexaforth__DOT__s3;
        return top.rootp->hexaforth__DOT__dmem[i];
    }

    int64_t rcell(int i) {
        int sp = top.dbg_rsp;
        if (i == sp - 1) return top.dbg_r;
        if (i == sp - 2) return top.rootp->hexaforth__DOT__r2;
        return top.rootp->hexaforth__DOT__rmem[i];
    }
};

static uint32_t u32(FILE *f) {
    uint32_t v = 0;
    if (fread(&v, 4, 1, f) != 1) return 0xffffffff;
    return v;
}

static std::vector<uint8_t> bytes(FILE *f, size_t n) {
    std::vector<uint8_t> v(n);
    if (n && fread(v.data(), 1, n, f) != n) v.clear();
    return v;
}

static int run_cases(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return 2; }
    Machine *m = new Machine;
    int total = 0, failed = 0;
    for (;;) {
        uint32_t n = u32(f);
        if (n == 0xffffffff) break;
        std::vector<uint8_t> label = bytes(f, n);
        uint32_t eip = u32(f);
        uint32_t mem_len = u32(f);
        std::vector<uint8_t> image = bytes(f, mem_len);
        m->in = bytes(f, u32(f));
        int32_t sp = (int32_t)u32(f), rsp = (int32_t)u32(f);
        std::vector<int64_t> want_d(sp > 0 ? sp : 0), want_r(rsp > 0 ? rsp : 0);
        if (!want_d.empty()) fread(want_d.data(), 8, want_d.size(), f);
        if (!want_r.empty()) fread(want_r.data(), 8, want_r.size(), f);
        std::vector<uint8_t> want_out = bytes(f, u32(f));
        std::vector<uint8_t> want_mem = bytes(f, mem_len);

        if (mem_len > MEM) continue;        // does not fit this machine
        // Ports 0xc1 to 0xc8 reach devices, and this machine has none.
        if (label.size() > 6 && (!memcmp(label.data(), "io 19", 5) || !memcmp(label.data(), "io 200", 6)))
            continue;
        total++;
        std::fill(m->mem.begin(), m->mem.end(), 0);
        memcpy(m->mem.data(), image.data(), mem_len);
        m->reset(eip);
        std::string problem;
        if (!m->run(200000)) problem += " did not halt;";
        if ((int)m->top.dbg_dsp != (sp & 0xff)) problem += " data depth;";
        else for (int i = 0; i < sp; i++)
            if ((m->dcell(i) ^ want_d[i]) & CELL_MASK) { problem += " data stack;"; break; }
        if ((int)m->top.dbg_rsp != (rsp & 0xff)) problem += " return depth;";
        else for (int i = 0; i < rsp; i++)
            if ((m->rcell(i) ^ want_r[i]) & CELL_MASK) { problem += " return stack;"; break; }
        if (memcmp(m->mem.data(), want_mem.data(), mem_len)) problem += " memory;";
        if (m->out != want_out) problem += " output;";
        if (!problem.empty() && ++failed <= 20)
            printf("FAIL %.*s:%s\n", (int)label.size(), label.data(), problem.c_str());
    }
    printf("%d/%d cases match vm.c\n", total - failed, total);
    printf("%.2f cycles per instruction over these cases\n",
           (double)m->cycles / (double)m->executed);
    return failed != 0;
}

static int run_image(const char *path, const char *disk, const char *memory) {
    Machine *m = new Machine;
    FILE *f = fopen(path, "r");
    if (!f) { perror(path); return 2; }
    char line[64];
    for (size_t at = 0; fgets(line, sizeof line, f) && at + 4 <= MEM; at += 4) {
        uint32_t v = strtoul(line, 0, 16);
        memcpy(&m->mem[at], &v, 4);
    }
    fclose(f);
    std::vector<std::string> disk_files;
    for (std::string rest = disk ? disk : ""; !rest.empty();) {     // load each disk; the files are separated by commas
        size_t comma = rest.find(',');
        std::string one = rest.substr(0, comma);
        rest = comma == std::string::npos ? "" : rest.substr(comma + 1);
        f = fopen(one.c_str(), "rb");
        if (!f) { perror(one.c_str()); return 2; }
        fseek(f, 0, SEEK_END);
        std::vector<uint8_t> is(ftell(f));
        rewind(f);
        if (fread(is.data(), 1, is.size(), f) != is.size()) { perror(one.c_str()); return 2; }
        fclose(f);
        m->disks.push_back(is);
        m->written.push_back(false);
        disk_files.push_back(one);
    }
    for (int c; (c = getchar()) != EOF;) m->in.push_back(c);
    std::vector<uint8_t> input = m->in;
    m->reset(0);
    m->in = input;
    m->timing = true;
    bool halted = m->run(2000000000ull);
    fwrite(m->out.data(), 1, m->out.size(), stdout);
    fprintf(stderr, "\n");
    for (size_t i = 1; i < m->marks.size(); i++) {
        const Mark &a = m->marks[i - 1], &b = m->marks[i];
        fprintf(stderr, "mark %zu to %zu: %llu cycles, %llu instructions; memory: %llu instructions fetched, "
                "%llu cells read, %llu stored\n", i, i + 1,
                (unsigned long long)(b.cycles - a.cycles), (unsigned long long)(b.executed - a.executed),
                (unsigned long long)(b.fetches - a.fetches), (unsigned long long)(b.reads - a.reads),
                (unsigned long long)(b.writes - a.writes));
    }
    fprintf(stderr, "%s after %llu cycles, %llu instructions: %.3f cycles per instruction\n",
            halted ? "halted" : "stopped", (unsigned long long)m->cycles,
            (unsigned long long)m->executed, (double)m->cycles / (double)m->executed);
    if (memory) {
        f = fopen(memory, "wb");
        if (!f) { perror(memory); return 2; }
        fwrite(m->mem.data(), 1, MEM, f);
        fclose(f);
    }
    for (size_t i = 0; i < m->disks.size(); i++) {      // write back each disk the machine wrote to
        if (!m->written[i]) continue;
        f = fopen(disk_files[i].c_str(), "wb");
        if (!f) { perror(disk_files[i].c_str()); return 2; }
        fwrite(m->disks[i].data(), 1, m->disks[i].size(), f);
        fclose(f);
    }
    return halted ? 0 : 1;
}

int main(int argc, char **argv) {
    Verilated::commandArgs(argc, argv);
    if (argc == 3 && !strcmp(argv[1], "cases")) return run_cases(argv[2]);
    if (argc >= 3 && argc <= 5 && !strcmp(argv[1], "run"))
        return run_image(argv[2], argc > 3 ? argv[3] : 0, argc > 4 ? argv[4] : 0);
    fprintf(stderr, "usage: tb cases CASES.bin | tb run IMAGE.hex [DISK.img [MEMORY.out]]\n");
    return 2;
}
