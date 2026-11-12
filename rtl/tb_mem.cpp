// tb_mem.cpp - check hf_mem.v against a plain byte array.
//
// Random unaligned reads and writes with random byte enables on the data
// port, and random fetches on the instruction port, including addresses at
// and past the end of the memory.

#include "Vhf_mem.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

int main() {
    const int AW = 14;
    const uint64_t SIZE = 8ull << AW;
    Vhf_mem top;
    std::vector<uint8_t> model(SIZE);
    uint64_t want_d = 0;
    uint16_t want_i = 0;
    bool have = false;
    srand(1);
    long checks = 0;
    for (long cycle = 0; cycle < 2000000; cycle++) {
        uint64_t a = (uint64_t)rand() % 3 == 0 ? SIZE - 12 + rand() % 24
                                              : ((uint64_t)rand() << 8 ^ rand()) % SIZE;
        bool re = rand() & 1;
        uint8_t we = re ? 0 : (uint8_t)(rand() % 4 == 0 ? rand() : 0xff >> (rand() % 8));
        uint64_t data = ((uint64_t)rand() << 40) ^ ((uint64_t)rand() << 20) ^ rand();
        uint32_t ia = rand() % (SIZE / 2);
        top.daddr = a;  top.dre = re;  top.dwe = we;  top.dwdata = data;  top.iaddr = ia;
        top.clk = 0;  top.eval();
        if (have && (top.drdata != want_d || top.idata != want_i)) {
            printf("FAIL at cycle %ld: data %016llx want %016llx, instruction %04x want %04x\n",
                   cycle, (unsigned long long)top.drdata, (unsigned long long)want_d,
                   top.idata, want_i);
            return 1;
        }
        checks++;
        // What the next cycle must show: reads see memory before this write.
        if (re) {
            want_d = 0;
            for (int i = 0; i < 8; i++)
                if (a + i < SIZE) want_d |= (uint64_t)model[a + i] << (8 * i);
        }
        want_i = model[2 * ia] | model[2 * ia + 1] << 8;
        for (int i = 0; i < 8; i++)
            if ((we >> i & 1) && a + i < SIZE) model[a + i] = data >> (8 * i);
        have = true;
        top.clk = 1;  top.eval();
    }
    printf("hf_mem matches a byte array over %ld random cycles\n", checks);
    return 0;
}
