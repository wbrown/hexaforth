// hexaforth.v - the hexaforth CPU.
//
// Same instruction set and semantics as vm.c.  A 64-bit datapath, 16-bit
// instructions, and no decode stage: the instruction's fields select the
// input mux, the ALU operation, the output mux and the stack deltas
// directly.
//
// Timing.  Memory is synchronous: an address goes out in one cycle and the
// data comes back in the next.  The next instruction is fetched while the
// current one executes, so an instruction that touches no memory, or only
// writes it, takes one cycle.  Each memory read adds one:
//
//     +  dup  >r  swap  !  literals  jumps  calls      1 cycle
//     @  +!  @+   (input mux [T], or ALU [IN])         2 cycles
//     @@          (both)                               3 cycles
//
// Stacks.  T and N (top and next of the data stack) and R (top of the
// return stack) are registers.  The rest of each stack is a small memory.
// No instruction writes either memory more than once.
//
// IO ports, as in vm.c:
//     0xf1 write   UART transmit         0xe0 read   UART receive (waits)
//     0xe1 read    true if a byte is waiting
//     0xc0 write   latch an address      0xc0 read   0 (memory starts at 0)
//     0xc1 0xc2 0xc4 0xc8   read or write 8/16/32/64 bits at the latch
//     0xd0 0xd1    read or set the data / return stack depth

module hexaforth #(
    parameter PCW = 16,             // program counter width, in words
    parameter SD  = 8               // log2 of the stack depths
) (
    input  wire        clk,
    input  wire        rst,
    input  wire [PCW-1:0] start_pc,

    // Instruction memory: word address out, instruction back next cycle.
    output wire [PCW-1:0] iaddr,
    input  wire [15:0] idata,

    // Data memory: byte address, any alignment.  Read data back next cycle.
    output reg  [63:0] daddr,
    output reg         dre,
    output reg  [7:0]  dwe,         // one bit per byte written
    output reg  [63:0] dwdata,
    input  wire [63:0] drdata,

    output reg         tx_valid,
    output wire [7:0]  tx_data,
    input  wire        tx_ready,
    input  wire        rx_valid,
    input  wire [7:0]  rx_data,
    output reg         rx_ready,

    output wire        halted,

    // For test benches: the registers, and a pulse per instruction executed.
    output wire [63:0] dbg_t, dbg_n, dbg_r,
    output wire [SD-1:0] dbg_dsp, dbg_rsp,
    output wire        dbg_exec
);
    // ------------------------------------------------------------ state
    reg [PCW-1:0] pc;               // address of the instruction in idata
    reg [63:0] T, N, R;
    reg [SD-1:0] dsp, rsp;          // stack depths
    reg [63:0] ioa;                 // latched address for the device ports

    // Data stack cells 0 .. dsp-3 and return stack cells 0 .. rsp-2, each at
    // its own index.  T is cell dsp-1, N is cell dsp-2, R is cell rsp-1.
    reg [63:0] ds [0:(1<<SD)-1] /* verilator public_flat_rd */;
    reg [63:0] rs [0:(1<<SD)-1] /* verilator public_flat_rd */;

    localparam S_BOOT = 0, S_RUN = 1, S_HALT = 2,
               S_SP1 = 3, S_SP2 = 4, S_SP3 = 5,     // set data stack depth
               S_RSP1 = 6, S_RSP2 = 7;              // set return stack depth
    reg [2:0]  state;
    reg [1:0]  ph;                  // memory reads done for this instruction
    reg [63:0] a_val;               // [T], held while [IN] is read
    reg [63:0] newdepth;

    // ----------------------------------------------------------- decode
    wire [15:0] ins     = idata;
    wire        is_lit  = ins[15];
    wire [1:0]  op_type = ins[14:13];
    wire        is_alu  = !is_lit && op_type == 2'd3;
    wire        ret     = ins[12];
    wire [1:0]  in_mux  = ins[11:10];
    wire [1:0]  out_mux = ins[9:8];
    wire [3:0]  alu_op  = ins[7:4];
    wire [1:0]  dfield  = ins[3:2];             // 0: +0  1: +1  3: -1  2: -2
    wire [1:0]  rfield  = ins[1:0];

    // ------------------------------------------------- stack memory reads
    wire [63:0] ds3 = ds[dsp - 3];              // the cells under N
    wire [63:0] ds4 = ds[dsp - 4];
    wire [63:0] rs2 = rs[rsp - 2];              // the cells under R
    wire [63:0] rs3 = rs[rsp - 3];

    // ------------------------------------------------- input mux and ALU
    wire        need_a = is_alu && in_mux == 2'd2;
    wire [63:0] a      = (ph == 2'd1) ? drdata : a_val;     // [T]
    wire [63:0] in     = in_mux == 2'd0 ? N :
                         in_mux == 2'd1 ? T :
                         in_mux == 2'd2 ? a : R;

    wire        dev_port = in[63:4] == 60'hc && (in[3:0] == 1 || in[3:0] == 2 ||
                                                 in[3:0] == 4 || in[3:0] == 8);
    wire        need_b = is_alu && (alu_op == 4'd14 || (alu_op == 4'd15 && dev_port));
    wire [63:0] b      = drdata;                            // [IN], or a device read

    reg  [63:0] io_in;
    always @* begin
        io_in = 64'd0;
        case (in)
            64'he0: io_in = {56'd0, rx_data};
            64'he1: io_in = {64{rx_valid}};
            64'hc1: io_in = {56'd0, b[7:0]};
            64'hc2: io_in = {48'd0, b[15:0]};
            64'hc4: io_in = {32'd0, b[31:0]};
            64'hc8: io_in = b;
            64'hd0: io_in = {{(64-SD){1'b0}}, dsp};
            64'hd1: io_in = {{(64-SD){1'b0}}, rsp};
            default: ;
        endcase
    end

    reg [63:0] out;
    always @* begin
        case (alu_op)
            4'd0, 4'd1, 4'd2: out = in;                     // pass through
            4'd3:  out = in + N;
            4'd4:  out = in & N;
            4'd5:  out = in | N;
            4'd6:  out = in ^ N;
            4'd7:  out = in * N;
            4'd8:  out = ~in;
            4'd9:  out = {64{in == N}};
            4'd10: out = {64{$signed(N) < $signed(in)}};
            4'd11: out = {64{N < in}};
            4'd12: out = in >> T[5:0];
            4'd13: out = in << T[5:0];
            4'd14: out = b;
            default: out = io_in;
        endcase
    end

    // The side channel: T->N and T<>N move T into N while IN passes through.
    wire        t_to_n = is_alu && (alu_op == 4'd1 || alu_op == 4'd2);
    wire [63:0] n1 = t_to_n ? T : N;
    wire [63:0] t1 = (is_alu && alu_op == 4'd1) ? N : T;

    // ------------------------------------------------------- IO writes
    wire        io_wr   = is_alu && out_mux == 2'd2;
    wire [63:0] port    = t1;
    wire        tx_wr   = io_wr && port == 64'hf1;
    wire        dev_wr  = io_wr && port[63:4] == 60'hc &&
                          (port[3:0] == 1 || port[3:0] == 2 || port[3:0] == 4 || port[3:0] == 8);
    wire        rx_rd   = is_alu && alu_op == 4'd15 && in == 64'he0;

    // ------------------------------------------------- next state values
    wire [63:0] lit = {52'd0, ins[11:0]} << (ins[13:12] * 12);

    reg  [63:0] t_next, n_next, r_next;
    reg  [SD-1:0] dsp_next, rsp_next;
    reg         ds_we, rs_we;

    always @* begin
        t_next = T;  n_next = N;  r_next = R;
        dsp_next = dsp;  rsp_next = rsp;
        ds_we = 0;  rs_we = 0;
        if (is_lit) begin
            if (ins[14]) begin
                t_next = T + lit;
            end else begin
                ds_we = 1;  n_next = T;  t_next = lit;  dsp_next = dsp + 1;
            end
        end else if (op_type == 2'd1) begin             // 0branch pops
            t_next = N;  n_next = ds3;  dsp_next = dsp - 1;
        end else if (op_type == 2'd2) begin             // call pushes the return
            rs_we = 1;  r_next = {{(64-PCW){1'b0}}, pc} + 1;  rsp_next = rsp + 1;
        end else if (is_alu) begin
            case (dfield)
                2'd1: begin ds_we = 1;  n_next = t1;  t_next = t1;  dsp_next = dsp + 1; end
                2'd3: begin t_next = n1;  n_next = ds3;  dsp_next = dsp - 1; end
                2'd2: begin t_next = ds3;  n_next = ds4;  dsp_next = dsp - 2; end
                default: begin t_next = t1;  n_next = n1; end
            endcase
            case (rfield)
                2'd1: begin rs_we = 1;  rsp_next = rsp + 1; end
                2'd3: begin r_next = rs2;  rsp_next = rsp - 1; end
                2'd2: begin r_next = rs3;  rsp_next = rsp - 2; end
                default: ;
            endcase
            if (out_mux == 2'd0) t_next = out;
            if (out_mux == 2'd1) r_next = out;
        end
    end

    // Branch targets: bit 12 clear is the kernel; set is this page.
    wire [PCW-1:0] target = ins[12] ? {pc[PCW-1:12], ins[11:0]}
                                    : {{(PCW-12){1'b0}}, ins[11:0]};
    reg  [PCW-1:0] pc_next;
    always @* begin
        pc_next = pc + 1;
        if (!is_lit) begin
            case (op_type)
                2'd0: pc_next = target;
                2'd1: if (T == 64'd0) pc_next = target;
                2'd2: pc_next = target;
                2'd3: if (ret) pc_next = R[PCW-1:0];
            endcase
        end
    end

    // -------------------------------------------------- sequencing
    wire running  = state == S_RUN;
    wire is_halt  = running && ins == 16'd0;
    assign halted = state == S_HALT;

    // Memory reads still owed before this instruction can execute.
    wire want_a = running && !is_halt && need_a && ph == 2'd0;
    wire want_b = running && !is_halt && need_b && ph != 2'd2 && !want_a;
    wire waiting = (tx_wr && !tx_ready) || (rx_rd && !rx_valid);
    wire execute = running && !is_halt && !want_a && !want_b && !waiting;

    assign iaddr   = (state == S_BOOT) ? start_pc : execute ? pc_next : pc;
    assign dbg_t = T;  assign dbg_n = N;  assign dbg_r = R;
    assign dbg_dsp = dsp;  assign dbg_rsp = rsp;  assign dbg_exec = execute;
    assign tx_data = out[7:0];

    wire [3:0] wbytes = port[3:0];
    always @* begin
        daddr = 64'd0;  dre = 0;  dwe = 8'd0;  dwdata = out;
        tx_valid = 0;  rx_ready = 0;
        if (want_a) begin
            daddr = T;  dre = 1;
        end else if (want_b) begin
            daddr = (alu_op == 4'd14) ? in : ioa;  dre = 1;
        end else if (execute) begin
            if (is_alu && out_mux == 2'd3) begin
                daddr = t1;  dwe = 8'hff;
            end else if (dev_wr) begin
                daddr = ioa;
                dwe = wbytes == 1 ? 8'h01 : wbytes == 2 ? 8'h03 : wbytes == 4 ? 8'h0f : 8'hff;
            end
            tx_valid = tx_wr;
            rx_ready = rx_rd;
        end
    end

    always @(posedge clk) begin
        if (rst) begin
            state <= S_BOOT;  ph <= 0;
            pc <= start_pc;  T <= 0;  N <= 0;  R <= 0;
            dsp <= 0;  rsp <= 0;  ioa <= 0;
        end else case (state)
            S_BOOT: state <= S_RUN;

            S_RUN: begin
                if (is_halt) begin
                    state <= S_HALT;
                end else if (want_a) begin
                    ph <= 2'd1;
                end else if (want_b) begin
                    if (ph == 2'd1) a_val <= drdata;
                    ph <= 2'd2;
                end else if (execute) begin
                    ph <= 0;
                    pc <= pc_next;
                    T <= t_next;  N <= n_next;  R <= r_next;
                    dsp <= dsp_next;  rsp <= rsp_next;
                    if (ds_we) ds[dsp - 2] <= is_lit ? N : n1;
                    if (rs_we) rs[rsp - 1] <= R;
                    if (io_wr && port == 64'hc0) ioa <= out;
                    if (io_wr && port == 64'hd0) begin newdepth <= out;  state <= S_SP1; end
                    if (io_wr && port == 64'hd1) begin newdepth <= out;  state <= S_RSP1; end
                end
            end

            // Setting the data stack depth: put N and T into the stack
            // memory, where every other cell already is, then load the two
            // top cells of the new depth.
            S_SP1: begin ds[dsp - 2] <= N;  state <= S_SP2; end
            S_SP2: begin ds[dsp - 1] <= T;  dsp <= newdepth[SD-1:0];  state <= S_SP3; end
            S_SP3: begin T <= ds[dsp - 1];  N <= ds[dsp - 2];  state <= S_RUN; end

            S_RSP1: begin rs[rsp - 1] <= R;  rsp <= newdepth[SD-1:0];  state <= S_RSP2; end
            S_RSP2: begin R <= rs[rsp - 1];  state <= S_RUN; end

            default: ;
        endcase
    end
endmodule
