// soc.v - the CPU with its memory, for synthesis.  The UART is left as pins.

module soc #(
    parameter W = 64,               // cell width: 64, 32 or 16
    parameter MB = 17,              // log2 of the memory size in bytes
    parameter PCW = MB - 1,         // program counter width, in words
    parameter DEPTH = 64,           // cells in each stack
    parameter INIT = ""             // memory image, one hex file per lane
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
    wire [PCW-1:0] iaddr;
    wire [15:0]  idata;
    wire [W-1:0] daddr, dwdata, drdata;
    wire         dre, dok;
    wire [W/8-1:0] dwe;

    hexaforth #(.W(W), .PCW(PCW), .DEPTH(DEPTH), .MB(MB)) cpu (
        .clk(clk), .rst(rst), .start_pc({PCW{1'b0}}),
        .iaddr(iaddr), .idata(idata),
        .daddr(daddr), .dok(dok), .dre(dre), .dwe(dwe), .dwdata(dwdata), .drdata(drdata),
        .tx_valid(tx_valid), .tx_data(tx_data), .tx_ready(tx_ready),
        .rx_valid(rx_valid), .rx_data(rx_data), .rx_ready(rx_ready),
        .halted(halted),
        .dbg_t(), .dbg_n(), .dbg_r(), .dbg_dsp(), .dbg_rsp(), .dbg_exec());

    hf_mem #(.W(W), .MB(MB), .INIT(INIT)) mem (
        .clk(clk),
        .daddr(daddr), .dok(dok), .dre(dre), .dwe(dwe), .dwdata(dwdata), .drdata(drdata),
        .iaddr(iaddr[MB-2:0]), .idata(idata));
endmodule
