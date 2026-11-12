// tb.cpp - Verilator test bench for hexaforth.v.
//
//   tb cases CASES.bin     replay the cases dumped by baremetal/difftest_dump
//                          (every test in test/tests.h, every ALU encoding,
//                          paged code) and compare the halted state with vm.c
//   tb run IMAGE.hex       boot an image; stdin is the UART; prints what it
//                          sends, then cycle and instruction counts
//
// The memory here is a plain byte array with the timing the CPU expects:
// addresses are sampled on the clock edge and data is back in the next
// cycle.  rtl/hf_mem.v, the real memory, is checked against the same model
// by tb_mem.cpp.

#include "Vhexaforth.h"
#include "Vhexaforth___024root.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// Built once for each machine size: W is the cell width and MB the log2 of
// the memory size, matching the CPU's parameters.
#ifndef W
#define W 64
#endif
#ifndef MB
#define MB 17
#endif
static const size_t MEM = (size_t)1 << MB;
static const uint64_t CELL_MASK = W == 64 ? ~0ull : (1ull << W) - 1;

struct Machine {
    Vhexaforth top;
    std::vector<uint8_t> mem = std::vector<uint8_t>(MEM);
    std::vector<uint8_t> in, out;
    size_t in_at = 0;
    uint16_t idata = 0;
    uint64_t rdata = 0;
    uint64_t cycles = 0, executed = 0;

    uint64_t read(uint64_t a, int n) {
        uint64_t v = 0;
        for (int i = 0; i < n; i++)
            if (a + i < MEM) v |= (uint64_t)mem[a + i] << (8 * i);
        return v;
    }

    void tick() {
        top.idata = idata;
        top.drdata = rdata;
        top.tx_ready = 1;
        top.rx_valid = in_at < in.size();
        top.rx_data = in_at < in.size() ? in[in_at] : 0;
        top.clk = 0;
        top.eval();
        uint16_t next_idata = (uint16_t)read((uint64_t)top.iaddr * 2, 2);
        uint64_t next_rdata = top.dre ? (top.dok ? read(top.daddr, W / 8) : 0) : rdata;
        for (int i = 0; i < W / 8; i++)
            if ((top.dwe >> i & 1) && top.dok && top.daddr + i < MEM)
                mem[top.daddr + i] = (uint64_t)top.dwdata >> (8 * i);
        if (top.tx_valid) out.push_back(top.tx_data);
        if (top.rx_ready) in_at++;
        executed += top.dbg_exec;
        top.clk = 1;
        top.eval();
        idata = next_idata;
        rdata = next_rdata;
        cycles++;
    }

    void reset(uint32_t pc) {
        top.rst = 1;
        top.start_pc = pc;
        idata = 0;
        rdata = 0;
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

    // The stacks are shift registers: cell i from the bottom is found by
    // its distance from the top.
    int64_t dcell(int i) {
        int sp = top.dbg_dsp;
        if (i == sp - 1) return top.dbg_t;
        if (i == sp - 2) return top.dbg_n;
        return top.rootp->hexaforth__DOT__dtail[sp - 3 - i];
    }

    int64_t rcell(int i) {
        int sp = top.dbg_rsp;
        return i == sp - 1 ? (int64_t)top.dbg_r : (int64_t)top.rootp->hexaforth__DOT__rtail[sp - 2 - i];
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

static int run_image(const char *path) {
    Machine *m = new Machine;
    FILE *f = fopen(path, "r");
    if (!f) { perror(path); return 2; }
    char line[64];
    for (size_t at = 0; fgets(line, sizeof line, f) && at + 4 <= MEM; at += 4) {
        uint32_t v = strtoul(line, 0, 16);
        memcpy(&m->mem[at], &v, 4);
    }
    fclose(f);
    for (int c; (c = getchar()) != EOF;) m->in.push_back(c);
    std::vector<uint8_t> input = m->in;
    m->reset(0);
    m->in = input;
    bool halted = m->run(2000000000ull);
    fwrite(m->out.data(), 1, m->out.size(), stdout);
    fprintf(stderr, "\n%s after %llu cycles, %llu instructions: %.3f cycles per instruction\n",
            halted ? "halted" : "stopped", (unsigned long long)m->cycles,
            (unsigned long long)m->executed, (double)m->cycles / (double)m->executed);
    return halted ? 0 : 1;
}

int main(int argc, char **argv) {
    Verilated::commandArgs(argc, argv);
    if (argc == 3 && !strcmp(argv[1], "cases")) return run_cases(argv[2]);
    if (argc == 3 && !strcmp(argv[1], "run")) return run_image(argv[2]);
    fprintf(stderr, "usage: tb cases CASES.bin | tb run IMAGE.hex\n");
    return 2;
}
