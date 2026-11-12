// hexaforth.S - bare-metal AArch64 hexaforth: boot stub, UART, inner loop.
//
// Same instruction semantics as vm.c.  The Forth image is appended at
// `image` and becomes VM byte address 0.
//
// VM address space (byte addresses, relative to `image`):
//   0x00000-0x01fff  page 0: kernel code
//   0x04000-0x0ffff  dictionary and data
//   0x10000-0x1ffff  code pages for definitions made at run time
//   DSTACK_BASE      data stack, grows up
//   RSTACK_BASE      return stack, grows up
//
// Code is paged.  The program counter is a flat word address and a page is
// 4096 words (8KB).  A 13-bit branch target with bit 12 clear is an absolute
// address in the kernel, reachable from anywhere; with bit 12 set it is an
// offset within the page the branch itself is in.  Return addresses and
// execution tokens are full program counters, so `exit` and `execute` cross
// pages with no extra state.
//
// IO ports:
//   0xf1 write   UART transmit            (emit)
//   0xe0 read    UART receive, blocking   (key)
//   0xe1 read    true if a character is waiting   (key?)
//   0xc0 write   latch a machine address  read: machine address of VM byte 0
//   0xc1/c2/c4/c8  read or write 8/16/32/64 bits at the latched address
//   0xd0 read    data stack depth      write: set it, reloading T
//   0xd1 read    return stack depth    write: set it, reloading R

        .equ    UART_BASE,   0x09000000      // PL011 on QEMU `virt`
        .equ    UART_FR,     0x18
        .equ    DSTACK_BASE, 0x20000         // 256 cells each
        .equ    RSTACK_BASE, 0x20800
        .equ    MEM_TOP,     0x21000         // zeroed up to here at boot

        .equ    IO_EMIT,     0xf1
        .equ    IO_KEY,      0xe0
        .equ    IO_KEYQ,     0xe1
        .equ    IO_ADDR,     0xc0
        .equ    IO_SP,       0xd0
        .equ    IO_RSP,      0xd1

// Built with -DSTRICT, a fetch or store that is not at a cell-aligned address
// sends '!' and halts.  The Forth system does not make any.
        .macro  ALIGNED reg
#ifdef STRICT
        tst     \reg, #7
        b.ne    unaligned
#endif
        .endm

MEM     .req    x19     // host address of VM byte 0
IOA     .req    x20     // latched machine address for the device ports
PC      .req    x21     // word address of next instruction
T       .req    x22     // top of data stack
R       .req    x23     // top of return stack
DS      .req    x24     // data stack pointer; DS[-1] is N
RS      .req    x25     // return stack pointer
UART    .req    x26
INS     .req    x9      // current instruction
INSw    .req    w9
IO      .req    x10     // IN on the way into the ALU, OUT on the way out
N       .req    x11
D       .req    x13     // signed data stack delta
RD      .req    x14     // signed return stack delta
OLDT    .req    x15

        .text
        .globl  _start, next, halt
_start:
        adr     MEM, image
        adr     x0, image_end
        add     x1, MEM, #MEM_TOP
1:      str     xzr, [x0], #8
        cmp     x0, x1
        b.lo    1b
        mov     UART, #UART_BASE
        add     DS, MEM, #DSTACK_BASE
        add     RS, DS, #(RSTACK_BASE - DSTACK_BASE)
        mov     PC, #0
        mov     IOA, MEM
        mov     T, #0
        mov     R, #0

next:
        ldrh    INSw, [MEM, PC, lsl #1]
        add     PC, PC, #1
        tbnz    INSw, #15, literal
        ubfx    w10, INSw, #13, #2              // op_type
        cbnz    w10, 1f
        cbz     INSw, halt
jump:   tbnz    INSw, #12, 2f
        and     PC, INS, #0xfff                 // bit 12 clear: kernel, absolute
        b       next
2:      sub     PC, PC, #1                      // bit 12 set: same page as the branch
        bfxil   PC, INS, #0, #12
        b       next
1:      cmp     w10, #2
        b.eq    call
        b.hi    alu
        mov     x10, T                          // 0branch
        ldr     T, [DS, #-8]!
        cbz     x10, jump
        b       next

call:   str     R, [RS], #8
        mov     R, PC
        b       jump

literal:
        ubfx    w10, INSw, #12, #2              // shift selector
        and     x12, INS, #0xfff
        add     w10, w10, w10, lsl #1
        lsl     w10, w10, #2                    // * 12
        lsl     x12, x12, x10
        tbnz    INSw, #14, 1f                   // imm+ : add to T
        str     T, [DS], #8
        mov     T, #0
1:      add     T, T, x12
        b       next

alu:    ldur    N, [DS, #-8]
        ubfx    w12, INSw, #10, #2              // in_mux
        cmp     w12, #1
        csel    IO, N, T, lo                    // 0: N   1: T
        b.ls    1f
        mov     IO, R                           // 3: R
        tbnz    w12, #0, 1f
        ALIGNED T
        ldr     IO, [MEM, T]                    // 2: [T]
1:      ubfx    w12, INSw, #4, #4               // alu_op
        adr     x16, alu_ops
        add     x16, x16, x12, lsl #3
#ifdef STRICT
        cmp     w12, #14                        // [IN] fetches at IN
        b.ne    9f
        ALIGNED IO
9:
#endif
        cmp     N, IO                           // flags for the compare ops
        br      x16

        // Sixteen two-instruction slots, indexed by alu_op.
alu_ops:
        b       alu_done                        //  0  IN->
        nop
        stur    T, [DS, #-8]                    //  1  T<>N,IN->
        b       swap_t
        stur    T, [DS, #-8]                    //  2  T->N,IN->
        b       alu_done
        add     IO, IO, N                       //  3  IN+N
        b       alu_done
        and     IO, IO, N                       //  4  IN&N
        b       alu_done
        orr     IO, IO, N                       //  5  IN|N
        b       alu_done
        eor     IO, IO, N                       //  6  IN^N
        b       alu_done
        mul     IO, IO, N                       //  7  IN*N
        b       alu_done
        mvn     IO, IO                          //  8  ~IN
        b       alu_done
        csetm   IO, eq                          //  9  IN==N
        b       alu_done
        csetm   IO, lt                          // 10  N<IN
        b       alu_done
        csetm   IO, lo                          // 11  Nu<IN
        b       alu_done
        lsr     IO, IO, T                       // 12  IN>>T
        b       alu_done
        lsl     IO, IO, T                       // 13  IN<<T
        b       alu_done
        ldr     IO, [MEM, IO]                   // 14  [IN]
        b       alu_done
        cmp     IO, #IO_KEY                     // 15  io[IN]
        b.ne    io_read
key:    ldr     w12, [UART, #UART_FR]
        tbnz    w12, #4, key                    // RXFE: nothing received yet
        ldr     w10, [UART]
        and     IO, IO, #0xff
        b       alu_done
io_read:
        cmp     IO, #IO_KEYQ
        b.ne    7f
        ldr     w12, [UART, #UART_FR]
        tst     w12, #0x10                      // RXFE clear: something is waiting
        csetm   IO, eq
        b       alu_done
7:      sub     x12, IO, #IO_ADDR               // device ports 0xc0-0xc8
        cmp     x12, #8
        b.hi    5f
        mov     IO, MEM                         // 0xc0: where VM byte 0 is
        cbz     x12, alu_done
        mov     IO, #0
        tbnz    x12, #0, 3f
        tbnz    x12, #1, 4f
        tbz     x12, #2, 6f
        ldr     w10, [IOA]                      // 0xc4
        b       alu_done
6:      tbz     x12, #3, alu_done
        ldr     IO, [IOA]                       // 0xc8
        b       alu_done
3:      cmp     x12, #1
        b.ne    alu_done
        ldrb    w10, [IOA]                      // 0xc1
        b       alu_done
4:      cmp     x12, #2
        b.ne    alu_done
        ldrh    w10, [IOA]                      // 0xc2
        b       alu_done
5:      cmp     IO, #IO_SP
        b.eq    1f
        cmp     IO, #IO_RSP
        mov     IO, #0
        b.ne    alu_done
        sub     IO, RS, #(RSTACK_BASE - DSTACK_BASE)
        b       2f
1:      mov     IO, DS
2:      sub     IO, IO, MEM
        sub     IO, IO, #DSTACK_BASE
        asr     IO, IO, #3
        b       alu_done
swap_t: mov     T, N

alu_done:
        tst     INSw, #0x1000                   // RET: R->PC
        csel    PC, R, PC, ne
        sbfx    D, INS, #2, #2
        sbfx    RD, INS, #0, #2
        add     DS, DS, D, lsl #3
        add     RS, RS, RD, lsl #3
        cmp     D, #0
        b.le    1f
        stur    T, [DS, #-8]                    // pushed: old T becomes N
1:      cmp     RD, #0
        b.le    1f
        stur    R, [RS, #-8]
1:      mov     OLDT, T
        tbz     D, #63, 1f
        ldr     T, [DS]                         // popped: refill T
1:      ubfx    w12, INSw, #8, #2               // out_mux
        cbz     w12, out_t
        cmp     w12, #2
        b.eq    out_io
        b.hi    out_mem
        mov     R, IO                           // 1: ->R
        b       next
out_mem:
        ALIGNED OLDT
        str     IO, [MEM, OLDT]                 // 3: ->[T]
        b       fix_r
out_io: cmp     OLDT, #IO_EMIT                  // 2: ->io[T]
        b.ne    2f
1:      ldr     w12, [UART, #UART_FR]
        tbnz    w12, #5, 1b                     // TXFF: transmit FIFO full
        str     w10, [UART]
        b       fix_r
2:      sub     x12, OLDT, #IO_ADDR             // device ports 0xc0-0xc8
        cmp     x12, #8
        b.hi    8f
        cbnz    x12, 5f
        mov     IOA, IO                         // 0xc0: latch
        b       fix_r
5:      cmp     x12, #1
        b.ne    6f
        strb    w10, [IOA]                      // 0xc1
        b       fix_r
6:      cmp     x12, #2
        b.ne    7f
        strh    w10, [IOA]                      // 0xc2
        b       fix_r
7:      cmp     x12, #4
        b.ne    9f
        str     w10, [IOA]                      // 0xc4
        b       fix_r
9:      cmp     x12, #8
        b.ne    fix_r
        str     IO, [IOA]                       // 0xc8
        b       fix_r
8:      add     x12, MEM, IO, lsl #3            // new stack pointer, less its base
        cmp     OLDT, #IO_SP
        b.ne    4f
        add     DS, x12, #DSTACK_BASE
        ldr     T, [DS]
        b       fix_r
4:      cmp     OLDT, #IO_RSP
        b.ne    fix_r
        add     RS, x12, #DSTACK_BASE
        add     RS, RS, #(RSTACK_BASE - DSTACK_BASE)
        ldr     R, [RS]
        b       next
out_t:  mov     T, IO                           // 0: ->T
fix_r:  tbz     RD, #63, next
        ldr     R, [RS]                         // popped: refill R
        b       next

#ifdef STRICT
unaligned:
        mov     w10, #'!'
        str     w10, [UART]
#endif
halt:   wfi
        b       halt

        .balign 16
native_end:
image:  .incbin IMAGE
        .balign 8
image_end:
