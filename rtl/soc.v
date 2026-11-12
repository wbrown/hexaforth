// soc.v - the CPU with its memory, for synthesis.  The UART is left as pins.

module soc #(
    parameter AW = 14,              // 2^AW 64-bit words: 128KB at 14
    parameter SD = 8,               // log2 of the stack depths
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
    wire [15:0] iaddr;
    wire [15:0] idata;
    wire [63:0] daddr, dwdata, drdata;
    wire        dre;
    wire [7:0]  dwe;

    hexaforth #(.PCW(16), .SD(SD)) cpu (
        .clk(clk), .rst(rst), .start_pc(16'd0),
        .iaddr(iaddr), .idata(idata),
        .daddr(daddr), .dre(dre), .dwe(dwe), .dwdata(dwdata), .drdata(drdata),
        .tx_valid(tx_valid), .tx_data(tx_data), .tx_ready(tx_ready),
        .rx_valid(rx_valid), .rx_data(rx_data), .rx_ready(rx_ready),
        .halted(halted),
        .dbg_t(), .dbg_n(), .dbg_r(), .dbg_dsp(), .dbg_rsp(), .dbg_exec());

    hf_mem #(.AW(AW), .INIT(INIT)) mem (
        .clk(clk),
        .daddr(daddr), .dre(dre), .dwe(dwe), .dwdata(dwdata), .drdata(drdata),
        .iaddr(iaddr[AW+1:0]), .idata(idata));
endmodule
