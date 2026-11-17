| Grays for AtkRow from pixels of three samples, and for a picture at half
| its size.
|
| A gray is 77 of red, 150 of green and 29 of blue in 256, white the high
| value.  Nothing is multiplied: three tables of 256 words say what each
| sample comes to (run68k.c makes them), and the high byte of the three
| together is the gray.
|
|   a2, a3, a4   the three tables: red, green, blue
|   a5           a word to take the high byte of a sum through
|
| GrayRow: a0 the pixels, a1 where the grays go, d7 one less than how many.
| It leaves a0 after the last pixel.

        .text
        .globl  GrayRow, GrayHalf, GrayHalf1
GrayRow:
        moveq   #0,d0
1:      move.b  (a0)+,d0                        | red
        add.w   d0,d0
        move.w  0(a2,d0.w),d1
        moveq   #0,d0
        move.b  (a0)+,d0                        | green
        add.w   d0,d0
        add.w   0(a3,d0.w),d1
        moveq   #0,d0
        move.b  (a0)+,d0                        | blue
        add.w   d0,d0
        add.w   0(a4,d0.w),d1
        move.w  d1,(a5)
        move.b  (a5),(a1)+                      | the high byte of the sum
        moveq   #0,d0
        dbra    d7,1b
        rts

| GrayHalf: the same for a picture at half its size.  A gray is that of
| four pixels, two of each of two rows, to the nearest: the four and two
| more, over four.  a0 is the first of the two rows and a6 the second, a1
| where the grays go, d7 one less than how many grays.

        .macro  pixel src
        moveq   #0,d0
        move.b  (\src)+,d0
        add.w   d0,d0
        move.w  0(a2,d0.w),d1
        moveq   #0,d0
        move.b  (\src)+,d0
        add.w   d0,d0
        add.w   0(a3,d0.w),d1
        moveq   #0,d0
        move.b  (\src)+,d0
        add.w   d0,d0
        add.w   0(a4,d0.w),d1
        move.w  d1,(a5)
        moveq   #0,d0
        move.b  (a5),d0
        add.w   d0,d2
        .endm

GrayHalf:
1:      moveq   #0,d2
        pixel   a0
        pixel   a0
        pixel   a6
        pixel   a6
        addq.w  #2,d2
        lsr.w   #2,d2
        move.b  d2,(a1)+
        dbra    d7,1b
        rts

| GrayHalf1: the same from grays that are given, a byte each: only the
| four are added.

GrayHalf1:
        moveq   #0,d0
1:      moveq   #0,d2
        move.b  (a0)+,d2
        move.b  (a0)+,d0
        add.w   d0,d2
        move.b  (a6)+,d0
        add.w   d0,d2
        move.b  (a6)+,d0
        add.w   d0,d2
        addq.w  #2,d2
        lsr.w   #2,d2
        move.b  d2,(a1)+
        dbra    d7,1b
        rts
