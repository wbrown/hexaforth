// tb_mem.cpp - check hf_mem.v against a plain array of cells.
//
// Random reads and writes in the same cycle, including a read of the cell
// being written, addresses past the end of the memory, reads with rok low,
// and reads that ask for one of a cell's instructions.

#include "Vhf_mem.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

static const uint64_t CELLS = 1ull << 14;

static uint64_t address() {
    return rand() % 3 == 0 ? CELLS - 4 + rand() % 8 : (uint64_t)rand() % CELLS;
}

int main() {
    Vhf_mem top;
    std::vector<uint64_t> model(CELLS);
    uint64_t want = 0;
    bool have = false;
    srand(1);
    long checks = 0;
    for (long cycle = 0; cycle < 2000000; cycle++) {
        uint64_t ra = address(), wa = rand() % 4 == 0 ? ra : address();
        bool rok = rand() % 8 != 0, we = rand() % 2;
        int slot = rand() % 2 ? 0 : rand() % 4;
        uint64_t data = ((uint64_t)rand() << 40) ^ ((uint64_t)rand() << 20) ^ rand();
        top.raddr = ra;  top.rok = rok;  top.rslot = slot;
        top.waddr = wa;  top.we = we;  top.wdata = data;
        top.clk = 0;  top.eval();
        if (have && top.rdata != want) {
            printf("FAIL at cycle %ld: read %016llx want %016llx\n",
                   cycle, (unsigned long long)top.rdata, (unsigned long long)want);
            return 1;
        }
        checks++;
        // What the next cycle must show: a read sees memory before this write.
        want = rok ? model[ra % CELLS] : 0;
        want = (want & ~0xffffull) | (want >> (16 * slot) & 0xffff);
        if (we) model[wa % CELLS] = data;
        have = true;
        top.clk = 1;  top.eval();
    }
    printf("hf_mem matches an array of cells over %ld random cycles\n", checks);
    return 0;
}
