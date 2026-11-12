// hf_mem.v - byte-addressed memory with unaligned 64-bit access in one cycle.
//
// The memory is eight byte-wide lanes; lane k holds every byte whose address
// is k modulo 8.  Eight consecutive bytes always fall one in each lane, so
// an unaligned 64-bit access is eight ordinary lane accesses, in lanes'
// own word or the next, followed by a rotation.  No access ever takes two
// cycles, which is what the dictionary search and the 16-bit fields in
// headers rely on.
//
// Each lane has a second port for instruction fetch.  An instruction is two
// bytes at an even address, so it comes from two neighbouring lanes of one
// word.  Reads on both ports are registered.  Addresses past the end read
// as zero and are not written.

module hf_mem #(
    parameter AW = 14,              // log2 of the size in 64-bit words
    parameter INIT = ""             // if set, lane k is loaded from INIT<k>.hex
) (
    input  wire        clk,
    input  wire [63:0] daddr,       // byte address
    input  wire        dre,
    input  wire [7:0]  dwe,
    input  wire [63:0] dwdata,
    output wire [63:0] drdata,
    input  wire [AW+1:0] iaddr,     // 16-bit word address
    output wire [15:0] idata
);
    wire [2:0]  off  = daddr[2:0];
    wire [60:0] word = daddr[63:3];

    reg  [2:0]  off_q;
    reg  [1:0]  isel_q;
    wire [63:0] dq;                 // lane outputs, lane 0 in the low byte
    wire [63:0] iq;

    genvar k;
    generate
        for (k = 0; k < 8; k = k + 1) begin : lane
            reg [7:0] m [0:(1<<AW)-1];
            reg [7:0] q, qi;
            initial if (INIT != "") $readmemh($sformatf("%s%0d.hex", INIT, k), m);
            // Lanes before the offset hold bytes from the next word.
            /* verilator lint_off CMPCONST */
            wire [60:0] w  = word + (k[2:0] < off ? 61'd1 : 61'd0);
            /* verilator lint_on CMPCONST */
            wire        ok = w[60:AW] == 0;
            // The data byte this lane takes: byte (k - off) mod 8.
            wire [2:0]  j  = k[2:0] - off;
            always @(posedge clk) begin
                if (dwe[j] && ok) m[w[AW-1:0]] <= dwdata[8*j +: 8];
                if (dre) q <= ok ? m[w[AW-1:0]] : 8'd0;     // held between reads
                qi <= m[iaddr[AW+1:2]];
            end
            assign dq[8*k +: 8] = q;
            assign iq[8*k +: 8] = qi;
        end
    endgenerate

    always @(posedge clk) begin
        if (dre) off_q <= off;      // held with the data between reads
        isel_q <= iaddr[1:0];
    end

    // Rotate the lanes back into address order.
    wire [127:0] twice = {dq, dq};
    assign drdata = twice[8*off_q +: 64];
    assign idata  = iq[16*isel_q +: 16];
endmodule
