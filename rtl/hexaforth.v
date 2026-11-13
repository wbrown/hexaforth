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
//   - T, N and R are registers, and so are the next cell of each stack.
//     The cells beneath them are in two small memories read without a
//     clock, at addresses that come straight from the stack depths.
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
    parameter DEPTH = 256,          // cells in each stack: STACK_CELLS in vm_constants.h
    parameter LAT   = 1             // cycles from a read address to its data: 1 or 2
) (
    input  wire        clk,
    input  wire        rst,
    input  wire [PCW-1:0] start_pc,

    // Read port: the address of a cell out, the cell back the next cycle.
    // For an instruction rslot says which of the cell's instructions is
    // wanted, and it comes back in the low 16 bits.  rok is low for a read
    // of a device port, which must read zero: this machine has no devices
    // behind ports 0xc1 to 0xc8.
    output reg  [W-1:0] raddr,
    output reg  [1:0]  rslot,
    output reg         rok,
    input  wire [W-1:0] rdata,
    // With LAT = 2, the memory's output before its register: what rdata
    // will be in the next cycle.  The instruction register loads from it.
    input  wire [W-1:0] rpre,

    // Write port: a whole cell.
    output reg  [W-1:0] waddr,
    output reg         we,
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
    reg [2:0]  dstep;               // how far a depth being set has got

    // What was asked of the read port in the last cycle (k1): nothing, an
    // instruction, [T], or [IN] or a device read.  k2 is the same a cycle
    // on, for data when LAT = 2.
    //
    // LAT = 1.  rdata shows what k1 asked for.  An instruction is decoded
    // straight from rdata, and one that takes more than a cycle moves to
    // `hold` so that the port is free.
    //
    // LAT = 2.  Instructions are asked for two ahead and come back in
    // order.  The memory's output register for them is the instruction
    // register `ir` here, which the decoder reads directly; what goes into
    // it is chosen before the clock edge, not after.  An instruction that
    // comes back while `ir` is still busy waits in `nxt`.  Data comes back
    // through the memory's own output register, a cycle after k1.
    localparam K_NONE = 2'd0, K_INS = 2'd1, K_A = 2'd2, K_B = 2'd3;
    reg [1:0]  k1, k2;
    wire [1:0] arr = LAT == 2 ? k2 : k1;        // the data rdata shows, if any
    reg        held;                // LAT 1: the current instruction is in `hold`
    reg [15:0] hold;
    reg        irv;                 // LAT 2: the current instruction is in `ir`
    reg [15:0] ir;
    reg        nxtv;                // LAT 2: the one after it is in `nxt`
    reg [15:0] nxt;
    reg [PCW-1:0] fpc;              // the next instruction to ask for

    // Work done ahead of executing the instruction.
    reg        have_a, have_b;      // memory reads latched
    reg [1:0]  slow_cnt;            // steps of a shift or multiply done
    reg [W-1:0] a_val, b_val;        // [T], and [IN] or a device read
    reg [W-1:0] sh_res, mul_ll, mul_res;
    reg [W/2-1:0] mul_x;

    // ----------------------------------------------------------- decode
    wire        have_cur = LAT == 2 ? irv : (held || k1 == K_INS);
    wire [15:0] ins     = LAT == 2 ? ir : (held ? hold : rdata[15:0]);
    wire        is_lit  = ins[15];
    // A literal with a shift field of 3 is not a literal: it fetches one of
    // the 4,096 cells from VAR_BASE, or stores T in it.
    wire        is_var  = is_lit && ins[13:12] == 2'd3;
    wire        is_vf   = is_var && !ins[14];
    wire        is_vs   = is_var && ins[14];
    localparam [W-1:0] VAR_BASE = 'h4000 / (W / 8);
    wire [W-1:0] vaddr  = VAR_BASE + {{(W-12){1'b0}}, ins[11:0]};
    wire [1:0]  op_type = ins[14:13];
    wire        is_alu  = !is_lit && op_type == 2'd3;
    wire        ret     = ins[12];
    wire [1:0]  in_mux  = ins[11:10];
    wire [1:0]  out_mux = ins[9:8];
    wire [3:0]  alu_op  = ins[7:4];
    wire [1:0]  dfield  = ins[3:2];             // 0: +0  1: +1  3: -1  2: -2
    wire [1:0]  rfield  = ins[1:0];

    // ------------------------------------------------------- predecode
    // The selects of the datapath's muxes, worked out from an instruction.
    // With LAT = 2 this is done as the instruction is loaded and kept in a
    // register beside it, so that the datapath does not wait for it.
    localparam KEEP = 2'd0, PUSH = 2'd1, POP1 = 2'd2, POP2 = 2'd3;
    localparam TS_T = 3'd0, TS_N = 3'd1, TS_DS3 = 3'd2, TS_LIT = 3'd3, TS_ADD = 3'd4, TS_OUT = 3'd5,
               TS_A = 3'd6;
    localparam NS_N = 2'd0, NS_T = 2'd1, NS_DS3 = 2'd2, NS_DS4 = 2'd3;
    localparam RS_R = 3'd0, RS_PC = 3'd1, RS_RS2 = 3'd2, RS_RS3 = 3'd3, RS_OUT = 3'd4;
    localparam ES_IN = 3'd0, ES_LOGIC = 3'd1, ES_MUL = 3'd2, ES_SH = 3'd3, ES_B = 3'd4, ES_IO = 3'd5;
    localparam CW = 28;

    function [CW-1:0] dec(input [15:0] i);
        reg       lit, alu, swap, t2n, sub;
        reg [1:0] op, om, df, rf, fk, lop, nsel, dins, rins;
        reg [3:0] ao, insel;
        reg [2:0] early, tsel, rsel;
        begin
            lit = i[15];  op = i[14:13];  alu = !lit && op == 2'd3;
            om = i[9:8];  ao = i[7:4];  df = i[3:2];  rf = i[1:0];
            swap = alu && ao == 4'd1;
            t2n  = alu && (ao == 4'd1 || ao == 4'd2);
            sub  = ao == 4'd9 || ao == 4'd10 || ao == 4'd11;
            insel = 4'd1 << i[11:10];
            fk = ao[1:0] - 2'd1;                        // 9: =   10: <   11: u<
            case (ao)
                4'd4, 4'd5, 4'd6, 4'd8: early = ES_LOGIC;
                4'd7:         early = ES_MUL;
                4'd12, 4'd13: early = ES_SH;
                4'd14:        early = ES_B;
                4'd15:        early = ES_IO;
                default:      early = ES_IN;
            endcase
            lop = ao == 4'd4 ? 2'd0 : ao == 4'd5 ? 2'd1 : ao == 4'd6 ? 2'd2 : 2'd3;

            tsel = TS_T;  nsel = NS_N;  rsel = RS_R;  dins = KEEP;  rins = KEEP;
            if (lit && i[13:12] == 2'd3) begin          // a variable
                if (i[14]) begin dins = POP1;  tsel = TS_N;  nsel = NS_DS3; end     // stored: T goes
                else begin dins = PUSH;  nsel = NS_T;  tsel = TS_A; end             // fetched: pushed
            end else if (lit) begin
                if (i[14]) tsel = TS_ADD;
                else begin dins = PUSH;  nsel = NS_T;  tsel = TS_LIT; end
            end else if (op == 2'd1) begin              // 0branch pops
                dins = POP1;  tsel = TS_N;  nsel = NS_DS3;
            end else if (op == 2'd2) begin              // call pushes the return
                rins = PUSH;  rsel = RS_PC;
            end else if (alu) begin
                case (df)
                    2'd1: begin dins = PUSH;  nsel = swap ? NS_N : NS_T;  tsel = swap ? TS_N : TS_T; end
                    2'd3: begin dins = POP1;  tsel = t2n ? TS_T : TS_N;  nsel = NS_DS3; end
                    2'd2: begin dins = POP2;  tsel = TS_DS3;  nsel = NS_DS4; end
                    default: begin tsel = swap ? TS_N : TS_T;  nsel = t2n ? NS_T : NS_N; end
                endcase
                case (rf)
                    2'd1: rins = PUSH;
                    2'd3: begin rins = POP1;  rsel = RS_RS2; end
                    2'd2: begin rins = POP2;  rsel = RS_RS3; end
                    default: ;
                endcase
                if (om == 2'd0) tsel = TS_OUT;
                if (om == 2'd1) rsel = RS_OUT;
            end
            dec = {swap, t2n, rins, dins, rsel, nsel, tsel, lop, early, fk, sub, ao == 4'd3, insel, sub};
        end
    endfunction

    reg  [CW-1:0] pd, pdn;          // LAT 2: predecode of ir and of nxt
    wire [CW-1:0] dpre = dec(rpre[15:0]);
    wire [CW-1:0] c = LAT == 2 ? pd : dec(ins);
    wire        c_sub   = c[0];
    wire [3:0]  c_insel = c[4:1];               // one-hot: N, T, [T], R
    wire        c_osum  = c[5];
    wire        c_oflag = c[6];
    wire [1:0]  c_fk    = c[8:7];
    wire [2:0]  c_early = c[11:9];
    wire [1:0]  c_lop   = c[13:12];
    wire [2:0]  c_tsel  = c[16:14];
    wire [1:0]  c_nsel  = c[18:17];
    wire [2:0]  c_rsel  = c[21:19];
    wire [1:0]  d_ins   = c[23:22];             // what this instruction does to each tail
    wire [1:0]  r_ins   = c[25:24];
    wire        c_t2n   = c[26];
    wire        c_swap  = c[27];

    // ------------------------------------------------- input mux and ALU
    wire        need_a = (is_alu && in_mux == 2'd2) || is_vf;
    wire [W-1:0] a_now  = have_a ? a_val : rdata;    // [T], latched or just read
    wire [W-1:0] b_now  = have_b ? b_val : rdata;
    wire [W-1:0] in     = ({W{c_insel[0]}} & N) | ({W{c_insel[1]}} & T) |
                         ({W{c_insel[2]}} & a_now) | ({W{c_insel[3]}} & R);

    // IN as it decides anything about sequencing or addressing: never
    // straight from the memory.  An instruction whose IN is [T] and which
    // then uses IN as an address or a port waits for [T] to be latched.
    wire        ab_op   = is_alu && (alu_op == 4'd14 || alu_op == 4'd15);
    wire [W-1:0] in_ctl = ({W{c_insel[0]}} & N) | ({W{c_insel[1]}} & T) |
                         ({W{c_insel[2]}} & a_val) | ({W{c_insel[3]}} & R);

    // Port numbers are small: compare the low byte once the rest is zero.
    wire        in_small = in_ctl[W-1:8] == 0;
    wire        dev_port = in_small && in_ctl[7:4] == 4'hc &&
                           (in_ctl[3:0] == 1 || in_ctl[3:0] == 2 || in_ctl[3:0] == 4 || in_ctl[3:0] == 8);
    wire        need_b   = is_alu && (alu_op == 4'd14 || (alu_op == 4'd15 && dev_port));
    wire        is_shift = is_alu && (alu_op == 4'd12 || alu_op == 4'd13);
    wire        is_mul   = is_alu && alu_op == 4'd7;
    wire [1:0]  slow_need = is_mul ? 2'd2 : is_shift ? 2'd1 : 2'd0;

    // One adder: IN + N, or N - IN for the three comparisons.
    wire        sub = c_sub;
    wire [W:0]  sum = {1'b0, N} + {1'b0, sub ? ~in : in} + {{W{1'b0}}, sub};
    wire        ult = !sum[W];                              // N u< IN: a borrow
    wire        slt = (N[W-1] ^ in[W-1]) ? N[W-1] : ult;
    wire        eq  = N == in;                  // not through the adder

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

    // Everything but the adder's results is ready early, so those are
    // chosen first, and the sum or a comparison is chosen last.
    reg [W-1:0] logic_out, early;
    always @* begin
        case (c_lop)
            2'd0: logic_out = in & N;
            2'd1: logic_out = in | N;
            2'd2: logic_out = in ^ N;
            default: logic_out = ~in;
        endcase
        case (c_early)
            ES_LOGIC: early = logic_out;
            ES_MUL:   early = mul_res;
            ES_SH:    early = sh_res;
            ES_B:     early = b_now;
            ES_IO:    early = io_in;
            default:  early = in;                           // pass through
        endcase
    end
    wire        flag = c_fk == 2'd0 ? eq : c_fk == 2'd1 ? slt : ult;
    wire [W-1:0] out = c_osum ? sum[W-1:0] : c_oflag ? {W{flag}} : early;

    // The side channel: T->N and T<>N move T into N while IN passes through.
    wire [W-1:0] n1 = c_t2n ? T : N;
    wire [W-1:0] t1 = c_swap ? N : T;

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

    reg  [W-1:0] t_next, n_next, r_next;
    always @* begin
        case (c_tsel)
            TS_N:    t_next = N;
            TS_DS3:  t_next = ds3;
            TS_LIT:  t_next = lit;
            TS_ADD:  t_next = T + lit;
            TS_OUT:  t_next = out;
            TS_A:    t_next = a_now;
            default: t_next = T;
        endcase
        case (c_nsel)
            NS_T:    n_next = T;
            NS_DS3:  n_next = ds3;
            NS_DS4:  n_next = ds4;
            default: n_next = N;
        endcase
        case (c_rsel)
            RS_PC:   r_next = pc_wide[W-1:0] + 1;
            RS_RS2:  r_next = rs2;
            RS_RS3:  r_next = rs3;
            RS_OUT:  r_next = out;
            default: r_next = R;
        endcase
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
    wire wr_busy   = we;                        // a store is landing this cycle
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

    // An instruction that does not fall through to the next one: what was
    // asked for after it is dropped, and fetching starts again at its target.
    wire redirect  = !is_lit && (op_type == 2'd0 || op_type == 2'd2 ||
                                 (op_type == 2'd1 && T == 0) || (op_type == 2'd3 && ret));
    wire jump      = execute && redirect;

    // Instructions in hand or on their way.  Another is asked for whenever
    // that leaves fewer than LAT after this cycle and the port is free.
    wire [1:0] others = LAT == 2 ? {1'b0, nxtv} + {1'b0, k1 == K_INS} : 2'd0;
    wire [1:0] total  = {1'b0, have_cur} + others;
    wire [1:0] left   = total - {1'b0, execute};
    wire       room   = state != S_HALT && (jump || left < LAT);
    wire [PCW-1:0] fetch = jump ? pc_next : fpc;

    // The kernel ROM.  The first 2^KB instructions of the kernel are never
    // written, so they can also be had from logic, without a clock: an
    // instruction there is in the instruction register the cycle after it
    // is asked for, where the memory takes two.  A jump, call or return
    // into the ROM therefore costs nothing, and the memory's read port is
    // left free for data.  The memory still holds the same instructions,
    // for anything that reads them as data.
    //
    // Whether this instruction completes, and whether it jumps, are known
    // late in the cycle.  So the ROM is read twice, at the next address in
    // line and at the address a jump would go to, and the choice between
    // the two is the last thing made.
    //
    // The ROM is compiled in when HF_KROM is defined (and needs LAT = 2).
    // Its contents are the function kromf in krom.vh, which memimage.py
    // writes from a nucleus image: a function and not a memory, so that
    // the tools leave it as logic behind the address registers.
    localparam KB = 11;             // log2 of the ROM's size in instructions (4KB)
    wire [PCW-1:0] btarget = op_type == 2'd3 ? R[PCW-1:0] : target;
`ifdef HF_KROM
`include "krom.vh"
    wire        rom_s = fpc[PCW-1:KB] == 0;
    wire        rom_b = btarget[PCW-1:KB] == 0;
    wire [15:0] romq_s = kromf(fpc[KB-1:0]);
    wire [15:0] romq_b = kromf(btarget[KB-1:0]);
`else
    wire        rom_s = 0, rom_b = 0;
    wire [15:0] romq_s = 16'd0, romq_b = 16'd0;
`endif
    wire [CW-1:0] drom_s = dec(romq_s);
    wire [CW-1:0] drom_b = dec(romq_b);

    wire       in_rom    = jump ? rom_b : rom_s;
    wire       issue_rom = room && in_rom;
    wire       issue  = room && !in_rom && !want_a && !want_b;
    wire [1:0] kreq   = want_a ? K_A : want_b ? K_B : issue ? K_INS : K_NONE;

    // Instructions arriving at this clock edge when there is no jump,
    // oldest first: the one waiting in nxt, one from the memory, one from
    // the ROM.  There are never more than two, and never more than there
    // is room for.
    wire        a1v = k1 == K_INS;
    wire        a2v = state != S_HALT && left < LAT && rom_s;
    wire        firstv  = nxtv || a1v || a2v;
    wire [15:0] first   = nxtv ? nxt : a1v ? rpre[15:0] : romq_s;
    wire [CW-1:0] firstd = nxtv ? pdn : a1v ? dpre : drom_s;
    wire        secondv = (nxtv && a1v) || ((nxtv || a1v) && a2v);
    wire [15:0] second  = (nxtv && a1v) ? rpre[15:0] : romq_s;
    wire [CW-1:0] secondd = (nxtv && a1v) ? dpre : drom_s;

    // A cell holds 2^ISB instructions.
    localparam ISB = W == 64 ? 2 : W == 32 ? 1 : 0;
    localparam [1:0] SLOTS = (1 << ISB) - 1;
    wire [W-1:0] fetch_w = {{(W-PCW){1'b0}}, fetch};
    always @* begin
        raddr = fetch_w >> ISB;  rslot = fetch[1:0] & SLOTS;  rok = 1;
        if (want_a) begin raddr = is_vf ? vaddr : T;  rslot = 0; end
        else if (want_b) begin
            raddr = in_ctl;  rslot = 0;
            rok = alu_op == 4'd14;
        end
    end

    assign tx_data = out[7:0];
    assign dbg_t = T;  assign dbg_n = N;  assign dbg_r = R;
    assign dbg_dsp = dsp;  assign dbg_rsp = rsp;  assign dbg_exec = execute;

    // A store, registered before it leaves.
    wire we_c = execute && ((is_alu && out_mux == 2'd3) || is_vs);
    always @* begin
        tx_valid = execute && tx_wr;
        rx_ready = execute && rx_rd;
    end

    always @(posedge clk) begin
        waddr <= is_vs ? vaddr : t1;  dwdata <= is_vs ? T : out;
        we <= !rst && we_c;
    end

    // What happens to the two tails this cycle: the instruction's own
    // effect, or one step towards a depth being set.
    reg [1:0]  d_op, r_op;
    reg [W-1:0] d_in;
    always @* begin
        d_op = KEEP;  r_op = KEEP;  d_in = n1;
        if (execute) begin
            d_op = d_ins;  r_op = r_ins;
        end else if (state == S_SP) begin
            d_in = N;
            if (dstep < 3) d_op = PUSH;
            else if (dstep > 3) d_op = POP1;
        end else if (state == S_RSP) begin
            if (dstep < 2) r_op = PUSH;
            else if (dstep > 2) r_op = POP1;
        end
    end

    // The stacks below the registers.  Cell i from the bottom of the data
    // stack is dmem[i].  The cell under N and the cell under R are kept in
    // registers as well (s3 and r2), so the memories are read only to
    // refill those, at addresses that come straight from the depth and do
    // not wait for the instruction to be decoded; and they are written only
    // from those registers.  On an FPGA these are small LUT memories.
    //
    // So the top three cells of the data stack (T, N, s3) and the top two of
    // the return stack (R, r2) are in registers and not yet in the memory.
    localparam SB = $clog2(DEPTH);
    reg [W-1:0] dmem [0:DEPTH-1] /* verilator public_flat_rd */;
    reg [W-1:0] rmem [0:DEPTH-1] /* verilator public_flat_rd */;
    reg [W-1:0] s3 /* verilator public_flat_rd */;
    reg [W-1:0] r2 /* verilator public_flat_rd */;
    wire [SB-1:0] dp = dsp[SB-1:0], rp = rsp[SB-1:0];
    wire [SB-1:0] da3 = dp - 2'd3, da4 = dp - 3'd4, da5 = dp - 3'd5;
    wire [SB-1:0] ra2 = rp - 2'd2, ra3 = rp - 2'd3, ra4 = rp - 3'd4;
    wire [W-1:0] dq4 = dmem[da4], dq5 = dmem[da5];
    wire [W-1:0] rq3 = rmem[ra3], rq4 = rmem[ra4];
    assign ds3 = s3;
    assign ds4 = dq4;
    assign rs2 = r2;
    assign rs3 = rq3;
    always @(posedge clk) begin
        case (d_op)
            PUSH: begin dmem[da3] <= s3;  s3 <= d_in; end
            POP1: s3 <= dq4;
            POP2: s3 <= dq5;
            default: ;
        endcase
        case (r_op)
            PUSH: begin rmem[ra2] <= r2;  r2 <= R; end
            POP1: r2 <= rq3;
            POP2: r2 <= rq4;
            default: ;
        endcase
    end

    wire [7:0] dsp_step = d_op == PUSH ? 8'd1 : d_op == POP1 ? 8'hff : d_op == POP2 ? 8'hfe : 8'd0;
    wire [7:0] rsp_step = r_op == PUSH ? 8'd1 : r_op == POP1 ? 8'hff : r_op == POP2 ? 8'hfe : 8'd0;

    always @(posedge clk) begin
        if (rst) begin
            state <= S_BOOT;
            have_a <= 0;  have_b <= 0;  slow_cnt <= 0;
            k1 <= K_NONE;  k2 <= K_NONE;  held <= 0;  irv <= 0;  nxtv <= 0;
            pc <= start_pc;  fpc <= start_pc;  T <= 0;  N <= 0;  R <= 0;
            dsp <= 0;  rsp <= 0;  ioa <= 0;
        end else begin
            dsp <= dsp + dsp_step;
            rsp <= rsp + rsp_step;

            // The read port.  Data is kept, in case the instruction does
            // not finish in the cycle it arrives.
            k1 <= kreq;
            k2 <= (k1 == K_A || k1 == K_B) ? k1 : K_NONE;
            if (issue || issue_rom) fpc <= fetch + 1;
            if (arr == K_A) begin a_val <= rdata;  have_a <= 1; end
            if (arr == K_B) begin b_val <= rdata;  have_b <= 1; end

            // Where the current instruction and the one after it wait.
            if (LAT == 2) begin
                if (jump) begin                         // what follows is dropped;
                    ir <= romq_b;  pd <= drom_b;        // a target in the ROM is here already
                    irv <= rom_b;  nxtv <= 0;
                end else if (execute || !irv) begin     // ir is free: refill it
                    ir <= first;  pd <= firstd;  irv <= firstv;
                    nxt <= second;  pdn <= secondd;  nxtv <= secondv;
                end else if (firstv) begin
                    nxt <= first;  pdn <= firstd;  nxtv <= 1;
                end
            end else begin
                if (jump || execute) held <= 0;
                else if (have_cur) begin hold <= ins;  held <= 1; end
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
                    if (io_wr && port_small && port[7:0] == 8'hd0) begin newdepth <= out[7:0];  dstep <= 0;  state <= S_SP; end
                    if (io_wr && port_small && port[7:0] == 8'hd1) begin newdepth <= out[7:0];  dstep <= 0;  state <= S_RSP; end
                end
            end

            // Setting a stack depth changes only the depth; no cell changes.
            // The cells held in registers are first pushed down into the
            // memory, with three pushes of nothing in particular.  The depth
            // is then set three above the one asked for, and three pops
            // bring the registers back from the memory.  The return stack
            // keeps two cells in registers, so it takes two of each.
            S_SP: begin
                dstep <= dstep + 1;
                if (dstep < 3) N <= T;
                else if (dstep == 3) dsp <= newdepth + 8'd3;
                else begin
                    T <= N;  N <= ds3;
                    if (dstep == 6) state <= S_RUN;
                end
            end

            S_RSP: begin
                dstep <= dstep + 1;
                if (dstep == 2) rsp <= newdepth + 8'd2;
                else if (dstep > 2) begin
                    R <= rs2;
                    if (dstep == 4) state <= S_RUN;
                end
            end

            default: ;
            endcase
        end
    end
endmodule
