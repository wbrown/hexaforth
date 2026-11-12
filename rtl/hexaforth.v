// hexaforth.v - the hexaforth CPU.
//
// Same instruction set and semantics as vm.c.  A 64-bit datapath, 16-bit
// instructions, and no decode stage: the instruction's fields select the
// input mux, the ALU operation, the output mux and the stack deltas
// directly.
//
// The structure follows James Bowman's J1a in four ways:
//
//   - The ALU sees only registers.  A memory read is latched before it is
//     used, so no path runs from the memory, through arithmetic, and back
//     to a memory address in one cycle.
//   - The stacks are shift registers, not memories.  T, N and R are
//     registers, and the cells beneath them sit at fixed positions with no
//     pointer arithmetic or addressing in front of them.
//   - One adder serves +, =, < and u<.
//   - The costly operations take extra cycles instead of setting the clock
//     period: shifts are computed into a register first, and the multiply
//     is done in two steps of 32-bit products.
//
// Timing.  The next instruction is fetched while the current one executes.
//
//     +  dup  >r  swap  !  literals  jumps  calls      1 cycle
//     shifts                                           2 cycles
//     *                                                3 cycles
//     @  +!  @+   (one memory read)                    4 cycles
//     @@          (two memory reads)                   7 cycles
//
// The data memory port is registered on the way out as well as on the way
// back, so that neither the instruction decode nor the ALU is in the path to
// the memory.  A store therefore lands one cycle after its instruction.
//
// IO ports, as in vm.c:
//     0xf1 write   UART transmit         0xe0 read   UART receive (waits)
//     0xe1 read    true if a byte is waiting
//     0xc0 write   latch an address      0xc0 read   0 (memory starts at 0)
//     0xc1 0xc2 0xc4 0xc8   read or write 8/16/32/64 bits at the latch
//     0xd0 0xd1    read or set the data / return stack depth

module hexaforth #(
    parameter PCW   = 16,           // program counter width, in words
    parameter DEPTH = 64,           // cells in each stack below T, N and R
    parameter MB    = 17            // log2 of the memory size in bytes
) (
    input  wire        clk,
    input  wire        rst,
    input  wire [PCW-1:0] start_pc,

    // Instruction memory: word address out, instruction back next cycle.
    output wire [PCW-1:0] iaddr,
    input  wire [15:0] idata,

    // Data memory: byte address, any alignment.  Read data back the cycle
    // after the address is presented.  dok is low for a device-port access
    // outside the memory, which must read zero and write nothing.
    output reg  [63:0] daddr,
    output reg         dok,
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
    output wire [7:0]  dbg_dsp, dbg_rsp,
    output wire        dbg_exec
);
    // ------------------------------------------------------------ state
    reg [PCW-1:0] pc;               // address of the instruction in idata
    reg [63:0] T, N, R;
    reg [7:0]  dsp, rsp;            // stack depths, for the depth ports
    reg [63:0] ioa;                 // latched address for the device ports

    // The stacks below the registers.  dtail[0] is the cell under N and
    // rtail[0] the cell under R; a push or pop moves every cell along.
    reg [63:0] dtail [0:DEPTH-1] /* verilator public_flat_rd */;
    reg [63:0] rtail [0:DEPTH-1] /* verilator public_flat_rd */;

    localparam S_BOOT = 0, S_RUN = 1, S_HALT = 2, S_SP = 3, S_RSP = 4;
    reg [2:0]  state;
    reg [7:0]  newdepth;

    // Work done ahead of executing the instruction in idata.
    reg        have_a, have_b;      // memory reads latched
    reg [1:0]  rd_wait;             // cycles until a read in flight is back
    reg        rd_is_b;
    reg [1:0]  slow_cnt;            // steps of a shift or multiply done
    reg [63:0] a_val, b_val;        // [T], and [IN] or a device read
    reg [63:0] sh_res, mul_ll, mul_res;
    reg [31:0] mul_x;

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

    // ------------------------------------------------- input mux and ALU
    wire        need_a = is_alu && in_mux == 2'd2;
    wire [63:0] in     = in_mux == 2'd0 ? N :
                         in_mux == 2'd1 ? T :
                         in_mux == 2'd2 ? a_val : R;

    wire        dev_port = in[63:4] == 60'hc && (in[3:0] == 1 || in[3:0] == 2 ||
                                                 in[3:0] == 4 || in[3:0] == 8);
    wire        need_b   = is_alu && (alu_op == 4'd14 || (alu_op == 4'd15 && dev_port));
    wire        is_shift = is_alu && (alu_op == 4'd12 || alu_op == 4'd13);
    wire        is_mul   = is_alu && alu_op == 4'd7;
    wire [1:0]  slow_need = is_mul ? 2'd2 : is_shift ? 2'd1 : 2'd0;

    // One adder: IN + N, or N - IN for the three comparisons.
    wire        sub = alu_op == 4'd9 || alu_op == 4'd10 || alu_op == 4'd11;
    wire [64:0] sum = {1'b0, N} + {1'b0, sub ? ~in : in} + {64'd0, sub};
    wire        ult = !sum[64];                             // N u< IN: a borrow
    wire        slt = (N[63] ^ in[63]) ? N[63] : ult;
    wire        eq  = sum[63:0] == 64'd0;

    reg  [63:0] io_in;
    always @* begin
        io_in = 64'd0;
        case (in)
            64'he0: io_in = {56'd0, rx_data};
            64'he1: io_in = {64{rx_valid}};
            64'hc1: io_in = {56'd0, b_val[7:0]};
            64'hc2: io_in = {48'd0, b_val[15:0]};
            64'hc4: io_in = {32'd0, b_val[31:0]};
            64'hc8: io_in = b_val;
            64'hd0: io_in = {56'd0, dsp};
            64'hd1: io_in = {56'd0, rsp};
            default: ;
        endcase
    end

    reg [63:0] out;
    always @* begin
        case (alu_op)
            4'd0, 4'd1, 4'd2: out = in;                     // pass through
            4'd3:  out = sum[63:0];
            4'd4:  out = in & N;
            4'd5:  out = in | N;
            4'd6:  out = in ^ N;
            4'd7:  out = mul_res;
            4'd8:  out = ~in;
            4'd9:  out = {64{eq}};
            4'd10: out = {64{slt}};
            4'd11: out = {64{ult}};
            4'd12, 4'd13: out = sh_res;
            4'd14: out = b_val;
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
    wire [63:0] ds3 = dtail[0];                 // the cells under N
    wire [63:0] ds4 = dtail[1];
    wire [63:0] rs2 = rtail[0];                 // the cells under R
    wire [63:0] rs3 = rtail[1];

    localparam KEEP = 2'd0, PUSH = 2'd1, POP1 = 2'd2, POP2 = 2'd3;

    reg  [63:0] t_next, n_next, r_next;
    reg  [1:0]  d_ins, r_ins;                   // what this instruction does to each tail

    always @* begin
        t_next = T;  n_next = N;  r_next = R;
        d_ins = KEEP;  r_ins = KEEP;
        if (is_lit) begin
            if (ins[14]) begin
                t_next = T + lit;
            end else begin
                d_ins = PUSH;  n_next = T;  t_next = lit;
            end
        end else if (op_type == 2'd1) begin             // 0branch pops
            d_ins = POP1;  t_next = N;  n_next = ds3;
        end else if (op_type == 2'd2) begin             // call pushes the return
            r_ins = PUSH;  r_next = {{(64-PCW){1'b0}}, pc} + 1;
        end else if (is_alu) begin
            case (dfield)
                2'd1: begin d_ins = PUSH;  n_next = t1;  t_next = t1; end
                2'd3: begin d_ins = POP1;  t_next = n1;  n_next = ds3; end
                2'd2: begin d_ins = POP2;  t_next = ds3;  n_next = ds4; end
                default: begin t_next = t1;  n_next = n1; end
            endcase
            case (rfield)
                2'd1: r_ins = PUSH;
                2'd3: begin r_ins = POP1;  r_next = rs2; end
                2'd2: begin r_ins = POP2;  r_next = rs3; end
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

    wire ready     = running && !is_halt && rd_wait == 0;
    wire want_a    = ready && need_a && !have_a;
    wire want_b    = ready && !want_a && need_b && !have_b;
    wire want_slow = ready && !want_a && !want_b && slow_cnt != slow_need;
    wire waiting   = (tx_wr && !tx_ready) || (rx_rd && !rx_valid);
    wire execute   = ready && !want_a && !want_b && !want_slow && !waiting;

    assign iaddr   = (state == S_BOOT) ? start_pc : execute ? pc_next : pc;
    assign tx_data = out[7:0];
    assign dbg_t = T;  assign dbg_n = N;  assign dbg_r = R;
    assign dbg_dsp = dsp;  assign dbg_rsp = rsp;  assign dbg_exec = execute;

    // The request to the data memory, registered before it leaves.
    wire [3:0] wbytes = port[3:0];
    wire       ioa_ok = ioa[63:MB] == 0;
    reg  [63:0] daddr_c;
    reg         dok_c, dre_c;
    reg  [7:0]  dwe_c;
    always @* begin
        daddr_c = T;  dok_c = 1;  dre_c = 0;  dwe_c = 8'd0;
        tx_valid = 0;  rx_ready = 0;
        if (want_a) begin
            dre_c = 1;
        end else if (want_b) begin
            if (alu_op == 4'd14) daddr_c = in;
            else begin daddr_c = ioa;  dok_c = ioa_ok; end
            dre_c = 1;
        end else if (execute) begin
            if (is_alu && out_mux == 2'd3) begin
                daddr_c = t1;  dwe_c = 8'hff;
            end else if (dev_wr) begin
                daddr_c = ioa;  dok_c = ioa_ok;
                dwe_c = wbytes == 1 ? 8'h01 : wbytes == 2 ? 8'h03 : wbytes == 4 ? 8'h0f : 8'hff;
            end
            tx_valid = tx_wr;
            rx_ready = rx_rd;
        end
    end

    always @(posedge clk) begin
        daddr <= daddr_c;  dok <= dok_c;  dwdata <= out;
        dre <= dre_c && !rst;
        dwe <= rst ? 8'd0 : dwe_c;
    end

    // What happens to the two tails this cycle: the instruction's own
    // effect, or one step towards a depth being set.
    reg [1:0]  d_op, r_op;
    reg [63:0] d_in;
    always @* begin
        d_op = KEEP;  r_op = KEEP;  d_in = is_lit ? N : n1;
        if (execute) begin
            d_op = d_ins;  r_op = r_ins;
        end else if (state == S_SP) begin
            d_in = N;
            if (dsp > newdepth) d_op = POP1;
            else if (dsp < newdepth) d_op = PUSH;
        end else if (state == S_RSP) begin
            if (rsp > newdepth) r_op = POP1;
            else if (rsp < newdepth) r_op = PUSH;
        end
    end

    integer k;
    always @(posedge clk) begin
        for (k = 0; k < DEPTH; k = k + 1) begin
            case (d_op)
                PUSH: dtail[k] <= k == 0 ? d_in : dtail[k - 1];
                POP1: dtail[k] <= k + 1 < DEPTH ? dtail[k + 1] : 64'd0;
                POP2: dtail[k] <= k + 2 < DEPTH ? dtail[k + 2] : 64'd0;
                default: ;
            endcase
            case (r_op)
                PUSH: rtail[k] <= k == 0 ? R : rtail[k - 1];
                POP1: rtail[k] <= k + 1 < DEPTH ? rtail[k + 1] : 64'd0;
                POP2: rtail[k] <= k + 2 < DEPTH ? rtail[k + 2] : 64'd0;
                default: ;
            endcase
        end
    end

    wire [7:0] dsp_step = d_op == PUSH ? 8'd1 : d_op == POP1 ? 8'hff : d_op == POP2 ? 8'hfe : 8'd0;
    wire [7:0] rsp_step = r_op == PUSH ? 8'd1 : r_op == POP1 ? 8'hff : r_op == POP2 ? 8'hfe : 8'd0;

    always @(posedge clk) begin
        if (rst) begin
            state <= S_BOOT;
            have_a <= 0;  have_b <= 0;  rd_wait <= 2'd0;  slow_cnt <= 0;
            pc <= start_pc;  T <= 0;  N <= 0;  R <= 0;
            dsp <= 0;  rsp <= 0;  ioa <= 0;
        end else begin
            dsp <= dsp + dsp_step;
            rsp <= rsp + rsp_step;
            case (state)
            S_BOOT: state <= S_RUN;

            S_RUN: begin
                if (is_halt) begin
                    state <= S_HALT;
                end else if (rd_wait != 0) begin
                    rd_wait <= rd_wait - 1;
                    if (rd_wait == 2'd1) begin              // the read data is here
                        if (rd_is_b) begin b_val <= drdata;  have_b <= 1; end
                        else         begin a_val <= drdata;  have_a <= 1; end
                    end
                end else if (want_a) begin
                    rd_wait <= 2'd2;  rd_is_b <= 0;
                end else if (want_b) begin
                    rd_wait <= 2'd2;  rd_is_b <= 1;
                end else if (want_slow) begin
                    slow_cnt <= slow_cnt + 1;
                    if (is_shift)
                        sh_res <= alu_op == 4'd12 ? in >> T[5:0] : in << T[5:0];
                    if (slow_cnt == 0) begin                // 32-bit products
                        mul_ll <= in[31:0] * N[31:0];
                        mul_x  <= in[31:0] * N[63:32] + in[63:32] * N[31:0];
                    end else begin
                        mul_res <= mul_ll + {mul_x, 32'd0};
                    end
                end else if (execute) begin
                    have_a <= 0;  have_b <= 0;  slow_cnt <= 0;
                    pc <= pc_next;
                    T <= t_next;  N <= n_next;  R <= r_next;
                    if (io_wr && port == 64'hc0) ioa <= out;
                    if (io_wr && port == 64'hd0) begin newdepth <= out[7:0];  state <= S_SP; end
                    if (io_wr && port == 64'hd1) begin newdepth <= out[7:0];  state <= S_RSP; end
                end
            end

            // Setting a stack depth: pop, or push, one cell a cycle until
            // the depth is the one asked for.
            S_SP: begin
                if (dsp > newdepth)      begin T <= N;  N <= ds3; end
                else if (dsp < newdepth) begin N <= T; end
                else state <= S_RUN;
            end

            S_RSP: begin
                if (rsp > newdepth) R <= rs2;
                else if (rsp == newdepth) state <= S_RUN;
            end

            default: ;
            endcase
        end
    end
endmodule
