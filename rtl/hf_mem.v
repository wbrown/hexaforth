// hf_mem.v - the machine's memory: an array of cells, with one read port
// and one write port.
//
// An address is the number of a cell.  A read gives the whole cell in the
// next cycle, or in the one after with LAT = 2.
//
// The read port serves instructions and data alike.  An instruction is 16
// bits, so a cell wider than that holds several, the first in the low bits.
// rslot says which is wanted, and it arrives in the low 16 bits of the
// data in place of what was there; for a read of data rslot is zero and
// the cell arrives as it is.
//
// Only the low AB bits of an address are used, so an address past the end
// wraps.  rok low makes a read return zero; the CPU uses it for a read that
// is not of memory.  A read sees the memory as it was before a write in
// the same cycle.

module hf_mem #(
    parameter W  = 64,              // cell width
    parameter AB = 14,              // log2 of the size in cells
    parameter INIT = "",            // if set, a hex file with a cell on each line
    parameter LAT = 1               // cycles from address to data: 1, or 2 with
                                    // the output registered
) (
    input  wire        clk,
    input  wire [W-1:0] raddr,
    input  wire [1:0]  rslot,
    input  wire        rok,
    output wire [W-1:0] rdata,
    output wire [W-1:0] rpre,       // rdata before the output register
    input  wire [W-1:0] waddr,
    input  wire        we,
    input  wire [W-1:0] wdata
);
    reg [W-1:0] m [0:(1<<AB)-1];
    reg [W-1:0] q;
    reg [1:0]   slot_q;
    initial if (INIT != "") $readmemh(INIT, m);

    always @(posedge clk) begin
        if (we) m[waddr[AB-1:0]] <= wdata;
        q <= rok ? m[raddr[AB-1:0]] : {W{1'b0}};
        slot_q <= rslot;
    end

    wire [W-1:0] rd1;
    generate
        if (W == 16) begin : one
            assign rd1 = q;
        end else begin : several
            wire [W-1:0] down = q >> {slot_q, 4'b0000};
            assign rd1 = {q[W-1:16], down[15:0]};
        end
    endgenerate
    assign rpre = rd1;
    generate
        if (LAT == 2) begin : outreg
            reg [W-1:0] rd2;
            always @(posedge clk) rd2 <= rd1;
            assign rdata = rd2;
        end else begin : direct
            assign rdata = rd1;
        end
    endgenerate
endmodule
