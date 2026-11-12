// tb_soc.cpp - boot the synthesisable system (soc.v: CPU plus hf_mem) with
// the memory image built into it.  stdin is the UART; what it sends is
// printed.

#include "Vsoc.h"
#include <cstdio>
#include <vector>

int main() {
    Vsoc top;
    std::vector<unsigned char> in;
    for (int c; (c = getchar()) != EOF;) in.push_back(c);
    size_t at = 0;
    unsigned long long cycles = 0;
    top.rst = 1;
    top.clk = 0;  top.eval();  top.clk = 1;  top.eval();
    top.rst = 0;
    while (!top.halted && cycles < 2000000000ull) {
        top.tx_ready = 1;
        top.rx_valid = at < in.size();
        top.rx_data = at < in.size() ? in[at] : 0;
        top.clk = 0;  top.eval();
        if (top.tx_valid) putchar(top.tx_data);
        if (top.rx_ready) at++;
        top.clk = 1;  top.eval();
        cycles++;
    }
    fprintf(stderr, "\n%s after %llu cycles\n", top.halted ? "halted" : "stopped", cycles);
    return top.halted ? 0 : 1;
}
