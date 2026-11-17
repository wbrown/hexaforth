// ramfb.S - the bridge from the machine's display to QEMU's ramfb.
//
// The machine's display is a bitmap in its memory, one bit to a pixel (see
// MODEL.md).  QEMU has no such display: ramfb shows a buffer of 32-bit
// pixels.  This file stands between the two.  It is not part of the machine
// and the kernel does not depend on what is in it: a kernel built with
// -DBRIDGE makes two calls, and another display needs only another file
// that answers them.
//
//   bridge_init   x0 = host address of cell 0, x1 = free host memory.
//                 Called once at boot.  May use x0-x17.
//   bridge_poll   Called whenever the machine asks whether a key is
//                 waiting, or waits for one.  May use x0-x7 and nothing
//                 else, not even x30 beyond returning through it.
//
// Here bridge_init tells QEMU where the pixels are, through the firmware
// configuration device, and bridge_poll copies the bitmap into them, at
// most 32 times a second.  It keeps a copy of the bitmap as it last saw
// it, and only a cell that has changed is drawn again, so a screen that is
// not changing costs a compare for each cell.
//
// The display it copies is the hosted machine's: the leftmost pixel of a
// cell is its lowest bit, a set bit is black, and the rows follow one
// another with no gap.  The size and the base are those of vm_constants.h.
//
// SCALE makes the picture bigger and keeps it exact: each pixel of the
// bitmap is drawn SCALE pixels across and SCALE down, and ramfb is told a
// width and a height SCALE times as great.  QEMU can stretch a picture to
// fit a window, but not by a whole number, and a dither stretched by
// anything else comes out in bands.  The pixels take
// DISP_W * DISP_H * 4 * SCALE * SCALE bytes of the free memory.

#ifndef DISP_W
#define DISP_W    768
#define DISP_H    576
#define DISP_BASE 0x4000
#endif
#ifndef SCALE
#define SCALE     1
#endif

        .equ    FWCFG,    0x09020000    // data, selector at +8, DMA address at +16
        .equ    CELLS,    DISP_W * DISP_H / 64
        .equ    ROWCELLS, DISP_W / 64           // cells in a row of the bitmap
        .equ    STRIDE,   DISP_W * SCALE * 4    // bytes in a row of pixels
        .equ    FBBYTES,  STRIDE * DISP_H * SCALE
        .equ    CELLSTEP, 64 * SCALE * 4        // bytes of pixels a cell is across
        .equ    ROWSTEP,  STRIDE * SCALE        // and from a row of the bitmap to the next

        // The state block.
        .equ    SRC,      0             // the bitmap
        .equ    FB,       8             // the pixels ramfb shows
        .equ    SHADOW,   16            // the bitmap as last drawn
        .equ    LAST,     24            // the timer when it was last drawn
        .equ    INTERVAL, 32            // timer ticks between drawings
        .equ    DMA,      40            // request: control, length, address
        .equ    CFG,      56            // ramfb: address, format, flags, width, height, stride
        .equ    DIR,      88            // one entry of the firmware's file directory

        .text
        .globl  bridge_init, bridge_poll

bridge_init:
        adr     x2, state
        add     x3, x0, #(DISP_BASE * 8)
        str     x3, [x2, #SRC]
        str     x1, [x2, #FB]
        ldr     x4, =FBBYTES
        add     x5, x1, x4
        str     x5, [x2, #SHADOW]
        mov     w6, #0x00ffffff                 // an empty bitmap is a white screen
1:      str     w6, [x1], #4
        cmp     x1, x5
        b.lo    1b
        mov     x7, #(CELLS * 8)
        add     x7, x5, x7
2:      str     xzr, [x5], #8
        cmp     x5, x7
        b.lo    2b
        mrs     x6, cntfrq_el0
        lsr     x6, x6, #5
        str     x6, [x2, #INTERVAL]

        // Find the file etc/ramfb in the firmware's directory.  Everything
        // the firmware says or is told is big-endian.
        mov     x8, #FWCFG
        mov     w9, #0x1900                     // select 0x0019, the directory
        strh    w9, [x8, #8]
        mov     w10, #0                         // how many entries
        mov     w11, #4
3:      ldrb    w12, [x8]
        orr     w10, w12, w10, lsl #8
        subs    w11, w11, #1
        b.ne    3b
4:      cbz     w10, 9f                         // no ramfb: nothing to show on
        sub     w10, w10, #1
        add     x13, x2, #DIR                   // size, select, reserved, name
        mov     w11, #64
5:      ldrb    w12, [x8]
        strb    w12, [x13], #1
        subs    w11, w11, #1
        b.ne    5b
        ldr     x14, [x2, #(DIR + 8)]
        ldr     x15, =0x666d61722f637465        // "etc/ramf"
        cmp     x14, x15
        b.ne    4b
        ldrh    w14, [x2, #(DIR + 16)]          // "b" and the end of the name
        cmp     w14, #'b'
        b.ne    4b
        ldrh    w14, [x2, #(DIR + 4)]
        rev16   w14, w14                        // the file's selector

        ldr     x15, [x2, #FB]
        rev     x15, x15
        str     x15, [x2, #CFG]
        ldr     w15, =0x34325258                // XRGB8888
        rev     w15, w15
        str     w15, [x2, #(CFG + 8)]
        str     wzr, [x2, #(CFG + 12)]
        ldr     w15, =(DISP_W * SCALE)
        rev     w15, w15
        str     w15, [x2, #(CFG + 16)]
        ldr     w15, =(DISP_H * SCALE)
        rev     w15, w15
        str     w15, [x2, #(CFG + 20)]
        ldr     w15, =STRIDE
        rev     w15, w15
        str     w15, [x2, #(CFG + 24)]

        lsl     w14, w14, #16                   // select the file and write
        orr     w14, w14, #0x18
        rev     w14, w14
        str     w14, [x2, #DMA]
        mov     w15, #28                        // 28 bytes
        rev     w15, w15
        str     w15, [x2, #(DMA + 4)]
        add     x15, x2, #CFG                   // from the configuration
        rev     x15, x15
        str     x15, [x2, #(DMA + 8)]
        add     x15, x2, #DMA
        rev     x15, x15
        str     x15, [x8, #16]
9:      ret

bridge_poll:
        adr     x1, state
        mrs     x0, cntvct_el0
        ldp     x2, x3, [x1, #LAST]             // and the interval
        sub     x4, x0, x2
        cmp     x4, x3
        b.lo    9f
        str     x0, [x1, #LAST]
        ldr     x2, [x1, #SRC]                  // the bitmap
        ldr     x4, [x1, #SHADOW]
        mov     x5, #CELLS
1:      ldr     x6, [x2], #8
        ldr     x7, [x4]
        cmp     x6, x7
        b.eq    4f
        str     x6, [x4]

        // Where the cell's pixels start: from how far into the copy the
        // cell is, its row of the bitmap and how far along the row.
        adr     x1, state
        ldp     x3, x0, [x1, #FB]               // the pixels, and the copy
        sub     x0, x4, x0
        lsr     x0, x0, #3                      // the cell's number
        mov     x7, #ROWCELLS
        udiv    x1, x0, x7                      // its row
        msub    x0, x1, x7, x0                  // and its place in the row
        ldr     x7, =ROWSTEP
        madd    x3, x1, x7, x3
        mov     x7, #CELLSTEP
        madd    x3, x0, x7, x3

        mov     w7, #0x00ffffff
        .rept   SCALE                           // each row of pixels the cell is on
        ldr     x6, [x4]                        // the cell, from the copy
        mov     x0, #64
2:      tst     x6, #1
        csel    w1, wzr, w7, ne                 // a set bit is black
        .rept   SCALE
        str     w1, [x3], #4
        .endr
        lsr     x6, x6, #1
        subs    x0, x0, #1
        b.ne    2b
        ldr     x0, =(STRIDE - CELLSTEP)        // the same place in the next row
        add     x3, x3, x0
        .endr
4:      add     x4, x4, #8
        subs    x5, x5, #1
        b.ne    1b
9:      ret

        .ltorg
        .balign 8
state:  .space  152
