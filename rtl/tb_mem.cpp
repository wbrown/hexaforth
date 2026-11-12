// tb_mem.cpp - check hf_mem.v against a plain byte array.
//
// Random unaligned reads on the read port and random unaligned writes with
// random byte enables on the write port, in the same cycle, including
// addresses that wrap at the end of the memory and accesses with rok or
// wok low.

#include "Vhf_mem.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

static const uint64_t SIZE = 1ull << 17;

static uint64_t address() {
    return (uint64_t)rand() % 3 == 0 ? SIZE - 12 + rand() % 24
                                     : ((uint64_t)rand() << 8 ^ rand()) % SIZE;
}

int main() {
    Vhf_mem top;
    std::vector<uint8_t> model(SIZE);
    uint64_t want = 0;
    bool have = false;
    srand(1);
    long checks = 0;
    for (long cycle = 0; cycle < 2000000; cycle++) {
        uint64_t ra = address(), wa = rand() % 4 == 0 ? ra + rand() % 16 - 8 : address();
        bool rok = rand() % 8 != 0, wok = rand() % 8 != 0;
        uint8_t we = rand() % 2 ? 0 : (uint8_t)(rand() % 4 == 0 ? rand() : 0xff >> (rand() % 8));
        uint64_t data = ((uint64_t)rand() << 40) ^ ((uint64_t)rand() << 20) ^ rand();
        top.raddr = ra;  top.rok = rok;
        top.waddr = wa;  top.wok = wok;  top.dwe = we;  top.dwdata = data;
        top.clk = 0;  top.eval();
        if (have && top.rdata != want) {
            printf("FAIL at cycle %ld: read %016llx want %016llx\n",
                   cycle, (unsigned long long)top.rdata, (unsigned long long)want);
            return 1;
        }
        checks++;
        // What the next cycle must show: a read sees memory before this write.
        want = 0;
        for (int i = 0; i < 8; i++)
            if (rok) want |= (uint64_t)model[(ra + i) % SIZE] << (8 * i);
        for (int i = 0; i < 8; i++)
            if ((we >> i & 1) && wok) model[(wa + i) % SIZE] = data >> (8 * i);
        have = true;
        top.clk = 1;  top.eval();
    }
    printf("hf_mem matches a byte array over %ld random cycles\n", checks);
    return 0;
}
