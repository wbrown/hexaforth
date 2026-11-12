// hf_mem.v - byte-addressed memory, one read port and one write port.
//
// The memory is one byte-wide lane for each byte of a cell: 8 lanes for a
// 64-bit cell, 4 for 32, 2 for 16.  Lane k holds every byte whose address is
// k modulo the cell size.  The bytes of a cell at any address always fall
// one in each lane, so an unaligned access is an ordinary access on every
// lane, in its own word or the next, followed by a rotation.  No access
// takes two cycles.
//
// The read port serves instructions and data alike.  An instruction is the
// low 16 bits of a read at an even address.
//
// With ALIGNED set, cells are only read and written at cell-aligned
// addresses, which is all the Forth nucleus does.  The lanes then never
// need the next word, and only the low 16 bits are rotated, for an
// instruction that is not at the start of a cell.
//
// With PLANES set, each lane is built from eight memories one bit wide.  A
// block RAM is then used at its deepest and narrowest, so that far fewer of
// them have to be chosen between on the way out.  This form cannot be
// loaded from INIT.
//
// Only the low address bits are used, so an address past the end wraps.
// rok low makes a read return zero and wok low makes a write do nothing;
// the CPU uses them for device-port accesses that fall outside the memory.
// A read sees the memory as it was before a write in the same cycle.

module hf_mem #(
    parameter W  = 64,              // cell width
    parameter MB = 17,              // log2 of the size in bytes
    parameter INIT = "",            // if set, lane k is loaded from INIT<k>.hex
    parameter ALIGNED = 0,          // if set, cells are only accessed aligned
    parameter PLANES = 0,           // if set, each lane is eight one-bit memories
    parameter LAT = 1               // cycles from address to data: 1, or 2 with
                                    // the output registered
) (
    input  wire        clk,
    input  wire [W-1:0] raddr,      // byte address
    input  wire        rok,
    output wire [W-1:0] rdata,
    input  wire [W-1:0] waddr,
    input  wire        wok,
    input  wire [W/8-1:0] dwe,
    input  wire [W-1:0] dwdata
);
    localparam LANES = W / 8;
    localparam LB = $clog2(LANES);  // address bits that pick a lane
    localparam AW = MB - LB;        // address bits that pick a word

    wire [LB-1:0] roff  = raddr[LB-1:0];
    wire [AW-1:0] rword = raddr[MB-1:LB];
    wire [LB-1:0] woff  = ALIGNED ? {LB{1'b0}} : waddr[LB-1:0];
    wire [AW-1:0] wword = waddr[MB-1:LB];

    reg  [LB-1:0] roff_q;
    wire [W-1:0]  dq;               // lane outputs, lane 0 in the low byte

    genvar k;
    generate
        for (k = 0; k < LANES; k = k + 1) begin : lane
            wire [7:0] q;
            // Lanes before the offset hold bytes from the next word.
            /* verilator lint_off CMPCONST */
            wire [AW-1:0] rw = rword + {{(AW-1){1'b0}}, !ALIGNED && k[LB-1:0] < roff};
            wire [AW-1:0] ww = wword + {{(AW-1){1'b0}}, k[LB-1:0] < woff};
            /* verilator lint_on CMPCONST */
            // The data byte this lane takes: byte (k - offset) modulo the lanes.
            wire [LB-1:0] j = k[LB-1:0] - woff;
            if (PLANES) begin : planes
                genvar b;
                for (b = 0; b < 8; b = b + 1) begin : plane
                    reg m [0:(1<<AW)-1];
                    reg qb;
                    always @(posedge clk) begin
                        if (dwe[j] && wok) m[ww] <= dwdata[8*j + b];
                        qb <= rok ? m[rw] : 1'b0;
                    end
                    assign q[b] = qb;
                end
            end else begin : bytes
                reg [7:0] m [0:(1<<AW)-1];
                reg [7:0] qq;
                initial if (INIT != "") $readmemh($sformatf("%s%0d.hex", INIT, k), m);
                always @(posedge clk) begin
                    if (dwe[j] && wok) m[ww] <= dwdata[8*j +: 8];
                    qq <= rok ? m[rw] : 8'd0;
                end
                assign q = qq;
            end
            assign dq[8*k +: 8] = q;
        end
    endgenerate

    always @(posedge clk) roff_q <= roff;

    // Rotate the lanes back into address order.
    wire [2*W-1:0] twice = {dq, dq};
    wire [W-1:0]   rot = twice[8*roff_q +: W];
    localparam [W-1:0] LOW16 = 'hffff;
    wire [W-1:0]   rd1 = ALIGNED ? ((dq & ~LOW16) | (rot & LOW16)) : rot;
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
