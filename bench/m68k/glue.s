| A picture to a bitmap with AtkRow, a row at a time, as a program of
| MacPaint's kind would call it.
|
| Assembled with SAMPLES 1 the picture is grays, a byte each with white the
| high value, and with 3 it is red, green and blue, which gray.s makes
| grays of.  With HALF 1 it is dithered at half its size, a gray being that
| of four pixels.
|
| Whoever starts it (run68k.c) has put in
|   a0  the picture                     d0  the pixels in a row of it
|   a1  where the bitmap goes           d1  its rows
|   a2  the three tables of gray.s
|   a3  where a byte is stored when all of it is done
|
| While it runs:
|   d5  where the next pixels are       d6  the row
| AtkRow keeps those, and a2 to a6.

        .text
        .globl  _start
_start: move.l  a3,done
        move.l  a1,bitmap                       | baseAddr
        move.w  d0,across
        .if     HALF
        lsr.w   #1,d0
        lsr.w   #1,d1
        .endif
        move.w  d0,width
        move.w  d1,height
        move.w  d1,bitmap+10                    | bounds: bottom
        move.w  d0,bitmap+12                    |         right
        lsr.w   #3,d0
        move.w  d0,bitmap+4                     | rowBytes
        lea     512(a2),a3
        lea     1024(a2),a4
        lea     word,a5
        move.l  a0,d5
        moveq   #0,d6
rows:
        .if     (SAMPLES == 1) && (HALF == 0)
        move.l  d5,-(sp)                        | grays: the picture's own
        moveq   #0,d0
        move.w  across,d0
        add.l   d0,d5
        .else
        movea.l d5,a0
        lea     grays,a1
        move.w  width,d7
        subq.w  #1,d7
        .if     HALF
        move.w  across,d0                       | the second of the two rows
        .if     SAMPLES == 3
        mulu.w  #3,d0
        .endif
        movea.l a0,a6
        adda.w  d0,a6
        .if     SAMPLES == 1
        bsr     GrayHalf1
        .else
        bsr     GrayHalf
        .endif
        move.l  a6,d5
        .else
        bsr     GrayRow
        move.l  a0,d5
        .endif
        pea     grays                           | grays
        .endif
        move.l  this,-(sp)                      | thisRow
        move.l  next,-(sp)                      | nextRow
        move.l  last,-(sp)                      | lastRow
        pea     bitmap                          | dstBits
        move.w  d6,-(sp)                        | vStart
        move.w  width,-(sp)                     | hSize
        jsr     AtkRow
        move.l  this,d0                         | the rows move up one
        move.l  next,this
        move.l  last,next
        move.l  d0,last
        addq.w  #1,d6
        cmp.w   height,d6
        bne.w   rows
        movea.l done,a0
        move.b  #1,(a0)
1:      bra.s   1b

| What is kept between the rows.  A row of what is owed is a word a pixel,
| with a word before it; there is room for 1,024 pixels.
        .equ    MOST, 1024
done:   .long   0
across: .word   0                               | pixels in a row of the picture
width:  .word   0                               | and of what is dithered,
height: .word   0                               | and the rows of that
word:   .word   0
this:   .long   row0+2
next:   .long   row1+2
last:   .long   row2+2
bitmap: .long   0                               | baseAddr
        .word   0                               | rowBytes
        .word   0, 0, 0, 0                      | bounds: top, left, bottom, right
row0:   .space  2*MOST+4
row1:   .space  2*MOST+4
row2:   .space  2*MOST+4
grays:  .space  MOST
