// hexaforth.v - the hexaforth CPU.
//
// Same instruction set and semantics as vm.c.  A datapath of W bits (the
// cell width: 64, 32 or 16), 16-bit instructions, and no decode stage: the instruction's fields select the
// input mux, the ALU operation, the output mux and the stack deltas
// directly.
//
// The structure follows James Bowman's J1a in four ways:
//
//   - The memory has one read port, and what comes out of it is whatever
//     was asked for in the cycle before: usually the next instruction,
//     fetched while this one executes, and sometimes data.  A fetch of
//     data borrows the port for a cycle while the instruction that wants
//     it waits in a register, and the data then goes straight into the
//     ALU, as an instruction goes straight into the decoder.  Nothing read
//     from the memory decides a memory address in the same cycle.
//   - The stacks are shift registers, not memories.  T, N and R are
//     registers, and the cells beneath them sit at fixed positions with no
//     pointer arithmetic or addressing in front of them.
//   - One adder serves +, =, < and u<.
//   - The costly operations take extra cycles instead of setting the clock
//     period: shifts are computed into a register first, and the multiply
//     is done in two steps of 32-bit products.
//
// Timing, with LAT = 1.  The next instruction is fetched while the current
// one executes.  With LAT = 2 the memory registers its output: a memory read
// takes a cycle more, and so does every jump, call, return or taken branch.
//
//     +  dup  >r  swap  !  literals  jumps  calls      1 cycle
//     shifts                                           2 cycles
//     *                                                3 cycles
//     @  +!  @+   (one memory read)                    2 cycles
//     @@          (two memory reads)                   4 cycles
//
// A store is registered on its way to the memory and lands one cycle after
// its instruction; a read of data waits for a store in flight.
//
// IO ports, as in vm.c:
//     0xf1 write   UART transmit         0xe0 read   UART receive (waits)
//     0xe1 read    true if a byte is waiting
//     0xc0 write   latch an address      0xc0 read   0 (memory starts at 0)
//     0xc1 0xc2 0xc4 0xc8   read or write 8/16/32/64 bits at the latch
//     0xd0 0xd1    read or set the data / return stack depth

module hexaforth #(
    parameter W     = 64,           // cell width: 64, 32 or 16
    parameter PCW   = 16,           // program counter width, in words; at most W
    parameter DEPTH = 64,           // cells in each stack below T, N and R
    parameter MB    = 17,           // log2 of the memory size in bytes
    parameter STACKRAM = 1,         // stacks in distributed RAM, or shift registers
    parameter LAT   = 1             // cycles from a read address to its data: 1 or 2
) (
    input  wire        clk,
    input  wire        rst,
    input  wire [PCW-1:0] start_pc,

    // Read port: a byte address out, the cell there back the next cycle.
    // An instruction is the low 16 bits of the cell at twice its address.
    // rok is low for a device-port read outside the memory, which must
    // read zero.
    output reg  [W-1:0] raddr,
    output reg         rok,
    input  wire [W-1:0] rdata,

    // Write port.  wok is low for a device-port write outside the memory,
    // which must write nothing.
    output reg  [W-1:0] waddr,
    output reg         wok,
    output reg  [W/8-1:0] dwe,      // one bit per byte written
    output reg  [W-1:0] dwdata,

    output reg         tx_valid,
    output wire [7:0]  tx_data,
    input  wire        tx_ready,
    input  wire        rx_valid,
    input  wire [7:0]  rx_data,
    output reg         rx_ready,

    output wire        halted,

    // For test benches: the registers, and a pulse per instruction executed.
    output wire [W-1:0] dbg_t, dbg_n, dbg_r,
    output wire [7:0]  dbg_dsp, dbg_rsp,
    output wire        dbg_exec
);
    // ------------------------------------------------------------ state
    reg [PCW-1:0] pc;               // address of the instruction being decoded
    reg [W-1:0] T, N, R;
    reg [7:0]  dsp, rsp;            // stack depths, for the depth ports
    reg [W-1:0] ioa;                 // latched address for the device ports

    // The cells under N (ds3, ds4) and under R (rs2, rs3) come from the
    // stacks below the registers, defined further down.
    wire [W-1:0] ds3, ds4, rs2, rs3;

    localparam S_BOOT = 0, S_RUN = 1, S_HALT = 2, S_SP = 3, S_RSP = 4;
    reg [2:0]  state;
    reg [7:0]  newdepth;

    // What was asked of the read port in the last cycle (k1) and the one
    // before (k2): nothing, an instruction, [T], or [IN] or a device read.
    // With LAT cycles of latency, what rdata shows now is k1 or k2.
    //
    // Instructions are asked for ahead of need, LAT of them at most, and
    // come back in order.  One that arrives while the one before it is
    // still unfinished waits in `nxt`, and an instruction that takes more
    // than a cycle moves to `hold`, so the read port never has to stand
    // still or be asked twice.
    localparam K_NONE = 2'd0, K_INS = 2'd1, K_A = 2'd2, K_B = 2'd3;
    reg [1:0]  k1, k2;
    wire [1:0] arr = LAT == 2 ? k2 : k1;
    reg        held;                // the current instruction is in `hold`
    reg [15:0] hold;
    reg        nxtv;                // the one after it is in `nxt`
    reg [15:0] nxt;
    reg [PCW-1:0] fpc;              // the next instruction to ask for

    // Work done ahead of executing the instruction.
    reg        have_a, have_b;      // memory reads latched
    reg [1:0]  slow_cnt;            // steps of a shift or multiply done
    reg [W-1:0] a_val, b_val;        // [T], and [IN] or a device read
    reg [W-1:0] sh_res, mul_ll, mul_res;
    reg [W/2-1:0] mul_x;

    // ----------------------------------------------------------- decode
    wire        have_cur = held || nxtv || arr == K_INS;
    wire [15:0] ins     = held ? hold : nxtv ? nxt : rdata[15:0];
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
    wire [W-1:0] a_now  = have_a ? a_val : rdata;    // [T], latched or just read
    wire [W-1:0] b_now  = have_b ? b_val : rdata;
    wire [W-1:0] in     = in_mux == 2'd0 ? N :
                         in_mux == 2'd1 ? T :
                         in_mux == 2'd2 ? a_now : R;

    // IN as it decides anything about sequencing or addressing: never
    // straight from the memory.  An instruction whose IN is [T] and which
    // then uses IN as an address or a port waits for [T] to be latched.
    wire        ab_op   = alu_op == 4'd14 || alu_op == 4'd15;
    wire [W-1:0] in_ctl = in_mux == 2'd0 ? N :
                         in_mux == 2'd1 ? T :
                         in_mux == 2'd2 ? a_val : R;

    // Port numbers are small: compare the low byte once the rest is zero.
    wire        in_small = in_ctl[W-1:8] == 0;
    wire        dev_port = in_small && in_ctl[7:4] == 4'hc &&
                           (in_ctl[3:0] == 1 || in_ctl[3:0] == 2 || in_ctl[3:0] == 4 || in_ctl[3:0] == 8);
    wire        need_b   = is_alu && (alu_op == 4'd14 || (alu_op == 4'd15 && dev_port));
    wire        is_shift = is_alu && (alu_op == 4'd12 || alu_op == 4'd13);
    wire        is_mul   = is_alu && alu_op == 4'd7;
    wire [1:0]  slow_need = is_mul ? 2'd2 : is_shift ? 2'd1 : 2'd0;

    // One adder: IN + N, or N - IN for the three comparisons.
    wire        sub = alu_op == 4'd9 || alu_op == 4'd10 || alu_op == 4'd11;
    wire [W:0]  sum = {1'b0, N} + {1'b0, sub ? ~in : in} + {{W{1'b0}}, sub};
    wire        ult = !sum[W];                              // N u< IN: a borrow
    wire        slt = (N[W-1] ^ in[W-1]) ? N[W-1] : ult;
    wire        eq  = sum[W-1:0] == 0;

    // Device reads are at most a cell wide.
    localparam [W-1:0] M8 = 64'hff, M16 = 64'hffff, M32 = 64'hffffffff;

    reg  [W-1:0] io_in;
    always @* begin
        io_in = 0;
        if (in_small) case (in_ctl[7:0])
            8'he0: io_in = {{(W-8){1'b0}}, rx_data};
            8'he1: io_in = {W{rx_valid}};
            8'hc1: io_in = b_now & M8;
            8'hc2: io_in = b_now & M16;
            8'hc4: io_in = b_now & M32;
            8'hc8: io_in = b_now;
            8'hd0: io_in = {{(W-8){1'b0}}, dsp};
            8'hd1: io_in = {{(W-8){1'b0}}, rsp};
            default: ;
        endcase
    end

    reg [W-1:0] out;
    always @* begin
        case (alu_op)
            4'd0, 4'd1, 4'd2: out = in;                     // pass through
            4'd3:  out = sum[W-1:0];
            4'd4:  out = in & N;
            4'd5:  out = in | N;
            4'd6:  out = in ^ N;
            4'd7:  out = mul_res;
            4'd8:  out = ~in;
            4'd9:  out = {W{eq}};
            4'd10: out = {W{slt}};
            4'd11: out = {W{ult}};
            4'd12, 4'd13: out = sh_res;
            4'd14: out = b_now;
            default: out = io_in;
        endcase
    end

    // The side channel: T->N and T<>N move T into N while IN passes through.
    wire        t_to_n = is_alu && (alu_op == 4'd1 || alu_op == 4'd2);
    wire [W-1:0] n1 = t_to_n ? T : N;
    wire [W-1:0] t1 = (is_alu && alu_op == 4'd1) ? N : T;

    // ------------------------------------------------------- IO writes
    wire        io_wr   = is_alu && out_mux == 2'd2;
    wire [W-1:0] port   = t1;
    wire        port_small = port[W-1:8] == 0;
    wire        tx_wr   = io_wr && port_small && port[7:0] == 8'hf1;
    wire        dev_wr  = io_wr && port_small && port[7:4] == 4'hc &&
                          (port[3:0] == 1 || port[3:0] == 2 || port[3:0] == 4 || port[3:0] == 8);
    wire        rx_rd   = is_alu && alu_op == 4'd15 && in_small && in_ctl[7:0] == 8'he0;

    // ------------------------------------------------- next state values
    wire [W-1:0] lit = {{(W-12){1'b0}}, ins[11:0]} << (ins[13:12] * 12);
    wire [W:0]   pc_wide = {{(W+1-PCW){1'b0}}, pc};

    localparam KEEP = 2'd0, PUSH = 2'd1, POP1 = 2'd2, POP2 = 2'd3;

    reg  [W-1:0] t_next, n_next, r_next;
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
            r_ins = PUSH;  r_next = pc_wide[W-1:0] + 1;
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
                2'd1: if (T == 0) pc_next = target;
                2'd2: pc_next = target;
                2'd3: if (ret) pc_next = R[PCW-1:0];
            endcase
        end
    end

    // -------------------------------------------------- sequencing
    wire running  = state == S_RUN;
    wire is_halt  = running && have_cur && ins == 16'd0;
    assign halted = state == S_HALT;

    // [T] can be used in the cycle it arrives, unless the instruction goes
    // on to use it as an address or a port number.  [IN] and device reads
    // can always be used as they arrive.
    wire ready     = running && have_cur && !is_halt;
    wire wr_busy   = dwe != 0;                  // a store is landing this cycle
    wire rd_flying = LAT == 2 && (k1 == K_A || k1 == K_B);
    wire a_here    = have_a || (arr == K_A && !ab_op);
    wire a_ok      = !need_a || a_here;
    wire b_here    = have_b || arr == K_B;
    wire b_ok      = !need_b || b_here;
    wire want_a    = ready && need_a && !a_here && arr != K_A && !rd_flying && !wr_busy;
    wire want_b    = ready && a_ok && !b_ok && !rd_flying && !wr_busy;
    wire want_slow = ready && a_ok && b_ok && slow_cnt != slow_need;
    wire waiting   = (tx_wr && !tx_ready) || (rx_rd && !rx_valid);
    wire execute   = ready && a_ok && b_ok && !want_slow && !waiting;

    wire       ioa_ok = (ioa >> MB) == 0;
    // An instruction that does not fall through to the next one: what was
    // asked for after it is dropped, and fetching starts again at its target.
    wire redirect  = !is_lit && (op_type == 2'd0 || op_type == 2'd2 ||
                                 (op_type == 2'd1 && T == 0) || (op_type == 2'd3 && ret));
    wire jump      = execute && redirect;

    // Instructions in hand or on their way.  Another is asked for whenever
    // that leaves fewer than LAT after this cycle and the port is free.
    wire [1:0] others = {1'b0, held && nxtv} + {1'b0, (held || nxtv) && arr == K_INS}
                      + {1'b0, LAT == 2 && k1 == K_INS};
    wire [1:0] total  = {1'b0, have_cur} + others;
    wire [1:0] left   = total - {1'b0, execute};
    wire       issue  = state != S_HALT && !want_a && !want_b && (jump || left < LAT);
    wire [1:0] kreq   = want_a ? K_A : want_b ? K_B : issue ? K_INS : K_NONE;

    wire [PCW-1:0] fetch = jump ? pc_next : fpc;
    always @* begin
        raddr = {{(W-PCW-1){1'b0}}, fetch, 1'b0};  rok = 1;
        if (want_a) raddr = T;
        else if (want_b) begin
            if (alu_op == 4'd14) raddr = in_ctl;
            else begin raddr = ioa;  rok = ioa_ok; end
        end
    end

    assign tx_data = out[7:0];
    assign dbg_t = T;  assign dbg_n = N;  assign dbg_r = R;
    assign dbg_dsp = dsp;  assign dbg_rsp = rsp;  assign dbg_exec = execute;

    // A store, registered before it leaves.  A device write wider than a
    // cell writes a cell.
    localparam [7:0] WB1 = 8'h01, WB2 = 8'h03, WB4 = 8'h0f, WB8 = 8'hff;
    wire [3:0] wbytes = port[3:0];
    reg  [W-1:0] daddr_c;
    reg         dok_c;
    reg  [W/8-1:0] dwe_c;
    always @* begin
        daddr_c = t1;  dok_c = 1;  dwe_c = 0;
        tx_valid = 0;  rx_ready = 0;
        if (execute) begin
            if (is_alu && out_mux == 2'd3) begin
                dwe_c = {(W/8){1'b1}};
            end else if (dev_wr) begin
                daddr_c = ioa;  dok_c = ioa_ok;
                dwe_c = wbytes == 1 ? WB1[W/8-1:0] : wbytes == 2 ? WB2[W/8-1:0] :
                        wbytes == 4 ? WB4[W/8-1:0] : WB8[W/8-1:0];
            end
            tx_valid = tx_wr;
            rx_ready = rx_rd;
        end
    end

    always @(posedge clk) begin
        waddr <= daddr_c;  wok <= dok_c;  dwdata <= out;
        dwe <= rst ? {(W/8){1'b0}} : dwe_c;
    end

    // What happens to the two tails this cycle: the instruction's own
    // effect, or one step towards a depth being set.
    reg [1:0]  d_op, r_op;
    reg [W-1:0] d_in;
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

    // The stacks below the registers, in one of two forms.
    //
    // As memories, read without a clock: cell i from the bottom of the data
    // stack is dmem[i], so the cell under N is at depth - 3, and a push
    // writes at depth - 2.  Only the depth changes on a pop.  On an FPGA
    // these are small LUT memories, not flip-flops.
    //
    // As shift registers: dtail[0] is the cell under N and rtail[0] the
    // cell under R, and a push or pop moves every cell along.
    localparam SB = $clog2(DEPTH);
    generate
        if (STACKRAM) begin : sram
            reg [W-1:0] dmem [0:DEPTH-1] /* verilator public_flat_rd */;
            reg [W-1:0] rmem [0:DEPTH-1] /* verilator public_flat_rd */;
            wire [SB-1:0] d3 = dsp[SB-1:0] - 2'd3, d4 = dsp[SB-1:0] - 3'd4;
            wire [SB-1:0] dw = dsp[SB-1:0] - 2'd2;
            wire [SB-1:0] r2 = rsp[SB-1:0] - 2'd2, r3 = rsp[SB-1:0] - 2'd3;
            wire [SB-1:0] rw = rsp[SB-1:0] - 2'd1;
            assign ds3 = dmem[d3];
            assign ds4 = dmem[d4];
            assign rs2 = rmem[r2];
            assign rs3 = rmem[r3];
            always @(posedge clk) begin
                if (d_op == PUSH) dmem[dw] <= d_in;
                if (r_op == PUSH) rmem[rw] <= R;
            end
        end else begin : sreg
            reg [W-1:0] dtail [0:DEPTH-1] /* verilator public_flat_rd */;
            reg [W-1:0] rtail [0:DEPTH-1] /* verilator public_flat_rd */;
            assign ds3 = dtail[0];
            assign ds4 = dtail[1];
            assign rs2 = rtail[0];
            assign rs3 = rtail[1];
            integer k;
            always @(posedge clk) begin
                for (k = 0; k < DEPTH; k = k + 1) begin
                    case (d_op)
                        PUSH: dtail[k] <= k == 0 ? d_in : dtail[k - 1];
                        POP1: dtail[k] <= k + 1 < DEPTH ? dtail[k + 1] : {W{1'b0}};
                        POP2: dtail[k] <= k + 2 < DEPTH ? dtail[k + 2] : {W{1'b0}};
                        default: ;
                    endcase
                    case (r_op)
                        PUSH: rtail[k] <= k == 0 ? R : rtail[k - 1];
                        POP1: rtail[k] <= k + 1 < DEPTH ? rtail[k + 1] : {W{1'b0}};
                        POP2: rtail[k] <= k + 2 < DEPTH ? rtail[k + 2] : {W{1'b0}};
                        default: ;
                    endcase
                end
            end
        end
    endgenerate

    wire [7:0] dsp_step = d_op == PUSH ? 8'd1 : d_op == POP1 ? 8'hff : d_op == POP2 ? 8'hfe : 8'd0;
    wire [7:0] rsp_step = r_op == PUSH ? 8'd1 : r_op == POP1 ? 8'hff : r_op == POP2 ? 8'hfe : 8'd0;

    always @(posedge clk) begin
        if (rst) begin
            state <= S_BOOT;
            have_a <= 0;  have_b <= 0;  slow_cnt <= 0;
            k1 <= K_NONE;  k2 <= K_NONE;  held <= 0;  nxtv <= 0;
            pc <= start_pc;  fpc <= start_pc;  T <= 0;  N <= 0;  R <= 0;
            dsp <= 0;  rsp <= 0;  ioa <= 0;
        end else begin
            dsp <= dsp + dsp_step;
            rsp <= rsp + rsp_step;

            // The read port.  Data is kept, in case the instruction does
            // not finish in the cycle it arrives.
            k1 <= kreq;
            k2 <= jump ? K_NONE : k1;
            if (issue) fpc <= fetch + 1;
            if (arr == K_A) begin a_val <= rdata;  have_a <= 1; end
            if (arr == K_B) begin b_val <= rdata;  have_b <= 1; end

            // Where the current instruction and the one after it wait.
            if (jump) begin
                held <= 0;  nxtv <= 0;
            end else begin
                if (execute) held <= 0;
                else if (have_cur) begin hold <= ins;  held <= 1; end
                if (arr == K_INS && (held || nxtv)) begin nxt <= rdata[15:0];  nxtv <= 1; end
                else if (!held) nxtv <= 0;
            end

            case (state)
            S_BOOT: state <= S_RUN;

            S_RUN: begin
                if (is_halt) begin
                    state <= S_HALT;
                end else if (want_slow) begin
                    slow_cnt <= slow_cnt + 1;
                    if (is_shift)
                        sh_res <= alu_op == 4'd12 ? in >> T[5:0] : in << T[5:0];
                    if (slow_cnt == 0) begin                // half-width products
                        mul_ll <= {{(W/2){1'b0}}, in[W/2-1:0]} * {{(W/2){1'b0}}, N[W/2-1:0]};
                        mul_x  <= in[W/2-1:0] * N[W-1:W/2] + in[W-1:W/2] * N[W/2-1:0];
                    end else begin
                        mul_res <= mul_ll + {mul_x, {(W/2){1'b0}}};
                    end
                end else if (execute) begin
                    have_a <= 0;  have_b <= 0;  slow_cnt <= 0;
                    pc <= pc_next;
                    T <= t_next;  N <= n_next;  R <= r_next;
                    if (io_wr && port_small && port[7:0] == 8'hc0) ioa <= out;
                    if (io_wr && port_small && port[7:0] == 8'hd0) begin newdepth <= out[7:0];  state <= S_SP; end
                    if (io_wr && port_small && port[7:0] == 8'hd1) begin newdepth <= out[7:0];  state <= S_RSP; end
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
