// soc.v - the CPU with its memory, for synthesis.  The UART is left as pins.

module soc #(
    parameter W = 64,               // cell width: 64, 32 or 16
    parameter MB = 17,              // log2 of the memory size in bytes
    parameter PCW = MB - 1,         // program counter width, in words
    parameter DEPTH = 64,           // cells in each stack
    parameter INIT = "",            // memory image, one hex file per lane
    parameter ALIGNED = 0,          // the memory takes only aligned cell accesses
    parameter STACKRAM = 1,         // stacks in distributed RAM, or shift registers
    parameter PLANES = 0            // memory built from one-bit-wide block RAMs
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
    wire [W-1:0] raddr, rdata, waddr, dwdata;
    wire         rok, wok;
    wire [W/8-1:0] dwe;

    hexaforth #(.W(W), .PCW(PCW), .DEPTH(DEPTH), .MB(MB), .STACKRAM(STACKRAM)) cpu (
        .clk(clk), .rst(rst), .start_pc({PCW{1'b0}}),
        .raddr(raddr), .rok(rok), .rdata(rdata),
        .waddr(waddr), .wok(wok), .dwe(dwe), .dwdata(dwdata),
        .tx_valid(tx_valid), .tx_data(tx_data), .tx_ready(tx_ready),
        .rx_valid(rx_valid), .rx_data(rx_data), .rx_ready(rx_ready),
        .halted(halted),
        .dbg_t(), .dbg_n(), .dbg_r(), .dbg_dsp(), .dbg_rsp(), .dbg_exec());

    hf_mem #(.W(W), .MB(MB), .INIT(INIT), .ALIGNED(ALIGNED), .PLANES(PLANES)) mem (
        .clk(clk),
        .raddr(raddr), .rok(rok), .rdata(rdata),
        .waddr(waddr), .wok(wok), .dwe(dwe), .dwdata(dwdata));
endmodule
