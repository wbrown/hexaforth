// soc.v - the CPU with its memory, for synthesis.  The UART is left as pins.

module soc #(
    parameter W = 64,               // cell width: 64, 32 or 16
    parameter AB = 14,              // log2 of the memory size in cells
    parameter PCW = 16,             // program counter width, in instructions
    parameter DEPTH = 256,          // cells in each stack
    parameter INIT = "",            // memory image: a hex file, a cell on each line
    parameter LAT = 1               // memory read latency: 1, or 2 with a registered output
) (
    input  wire       clk,
    input  wire       rst,
    output wire       tx_valid,
    output wire [7:0] tx_data,
    input  wire       tx_ready,
    input  wire       rx_valid,
    input  wire [7:0] rx_data,
    output wire       rx_ready,
    output wire       halted
);
    wire [W-1:0] raddr, rdata, rpre, waddr, dwdata;
    wire [1:0]   rslot;
    wire         rok, we;

    hexaforth #(.W(W), .PCW(PCW), .DEPTH(DEPTH), .LAT(LAT)) cpu (
        .clk(clk), .rst(rst), .start_pc({PCW{1'b0}}),
        .raddr(raddr), .rslot(rslot), .rok(rok), .rdata(rdata), .rpre(rpre),
        .waddr(waddr), .we(we), .dwdata(dwdata),
        .tx_valid(tx_valid), .tx_data(tx_data), .tx_ready(tx_ready),
        .rx_valid(rx_valid), .rx_data(rx_data), .rx_ready(rx_ready),
        .halted(halted),
        .dbg_t(), .dbg_n(), .dbg_r(), .dbg_dsp(), .dbg_rsp(), .dbg_exec());

    hf_mem #(.W(W), .AB(AB), .INIT(INIT), .LAT(LAT)) mem (
        .clk(clk),
        .raddr(raddr), .rslot(rslot), .rok(rok), .rdata(rdata), .rpre(rpre),
        .waddr(waddr), .we(we), .wdata(dwdata));
endmodule
