// hf_mem.v - byte-addressed memory with unaligned cell access in one cycle.
//
// The memory is one byte-wide lane for each byte of a cell: 8 lanes for a
// 64-bit cell, 4 for 32, 2 for 16.  Lane k holds every byte whose address is
// k modulo the cell size.  The bytes of a cell at any address always fall
// one in each lane, so an unaligned access is an ordinary access on every
// lane, in its own word or the next, followed by a rotation.  No access
// takes two cycles, which is what the dictionary search and the 16-bit
// fields in headers rely on.
//
// Each lane has a second port for instruction fetch.  An instruction is two
// bytes at an even address, so it comes from two neighbouring lanes of one
// word.  Reads on both ports are registered.
//
// Only the low address bits are used, so an address past the end wraps.
// `dok` low makes a read return zero and a write do nothing; the CPU uses it
// for device-port accesses that fall outside the memory.

module hf_mem #(
    parameter W  = 64,              // cell width
    parameter MB = 17,              // log2 of the size in bytes
    parameter INIT = "",            // if set, lane k is loaded from INIT<k>.hex
    parameter ALIGNED = 0           // if set, data accesses are whole aligned cells
) (
    input  wire        clk,
    input  wire [W-1:0] daddr,      // byte address
    input  wire        dok,
    input  wire        dre,
    input  wire [W/8-1:0] dwe,
    input  wire [W-1:0] dwdata,
    output wire [W-1:0] drdata,
    input  wire [MB-2:0] iaddr,     // 16-bit word address
    output wire [15:0] idata
);
    localparam LANES = W / 8;
    localparam LB = $clog2(LANES);  // address bits that pick a lane
    localparam AW = MB - LB;        // address bits that pick a word

    // With ALIGNED the offset is zero, and the lane adders and the rotation
    // below reduce to wires: the lanes become one cell-wide memory.
    wire [LB-1:0] off  = ALIGNED ? {LB{1'b0}} : daddr[LB-1:0];
    wire [AW-1:0] word = daddr[MB-1:LB];
    wire [MB-1:0] ibyte = {iaddr, 1'b0};

    reg  [LB-1:0] off_q, ioff_q;
    wire [W-1:0]  dq;               // lane outputs, lane 0 in the low byte
    wire [W-1:0]  iq;

    genvar k;
    generate
        for (k = 0; k < LANES; k = k + 1) begin : lane
            reg [7:0] m [0:(1<<AW)-1];
            reg [7:0] q, qi;
            initial if (INIT != "") $readmemh($sformatf("%s%0d.hex", INIT, k), m);
            // Lanes before the offset hold bytes from the next word.
            /* verilator lint_off CMPCONST */
            wire [AW-1:0] w = word + {{(AW-1){1'b0}}, k[LB-1:0] < off};
            /* verilator lint_on CMPCONST */
            // The data byte this lane takes: byte (k - off) modulo the lanes.
            wire [LB-1:0] j = k[LB-1:0] - off;
            always @(posedge clk) begin
                if (dwe[j] && dok) m[w] <= dwdata[8*j +: 8];
                if (dre) q <= dok ? m[w] : 8'd0;            // held between reads
                qi <= m[ibyte[MB-1:LB]];
            end
            assign dq[8*k +: 8] = q;
            assign iq[8*k +: 8] = qi;
        end
    endgenerate

    always @(posedge clk) begin
        if (dre) off_q <= off;      // held with the data between reads
        ioff_q <= ibyte[LB-1:0];
    end

    // Rotate the lanes back into address order.
    wire [2*W-1:0] twice = {dq, dq};
    assign drdata = twice[8*off_q +: W];
    assign idata  = iq[8*ioff_q +: 16];
endmodule
