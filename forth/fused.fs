\ fused.fs - the image's instructions as words, for see.fs
\ GENERATED from the image by util/fused.c whenever the image is
\ built (util/tables.cmake).  Do not edit.
\
\ Each entry is cells: the instruction, how many words it is,
\ and the address of each word's header.  The words are the
\ plain ones a programmer would write: one standard word, or
\ two, before the machine's own name for the instruction.  Those
\ the image has most of are first, since the table is searched
\ from the start.  A zero ends the table.  It is a table of
\ cells, reached a cell at a time: high.

high
header fused create fused
$6800 , $1 , hdr, @
$6020 , $1 , hdr, swap
$6004 , $1 , hdr, over
$6404 , $1 , hdr, dup
$6c07 , $1 , hdr, r>
$650d , $1 , hdr, >r
$600c , $1 , hdr, drop
$6308 , $1 , hdr, !
$6c04 , $1 , hdr, r@
$644c , $1 , hdr, and
$649c , $1 , hdr, =
$643c , $1 , hdr, +
$6480 , $1 , hdr, invert
$60cc , $1 , hdr, rshift
$60dc , $1 , hdr, lshift
$64ac , $1 , hdr, <
$6b38 , $1 , hdr, +!
$645c , $1 , hdr, or
$6c17 , $2 , hdr, swap hdr, r>
$6490 , $2 , hdr, over hdr, =
$6d08 , $1 , hdr, 2drop
$6494 , $2 , hdr, 2dup hdr, =
$6464 , $2 , hdr, 2dup hdr, xor
$647c , $1 , hdr, *
$6804 , $2 , hdr, dup hdr, @
$612d , $2 , hdr, swap hdr, >r
$64a0 , $2 , hdr, over hdr, >
$6430 , $2 , hdr, over hdr, +
$6414 , $1 , hdr, tuck
$60c0 , $3 , hdr, over hdr, swap hdr, rshift
$64bc , $1 , hdr, u<
$6440 , $2 , hdr, over hdr, and
$600f , $2 , hdr, r> hdr, 2drop
$683c , $2 , hdr, @ hdr, +
$642c , $1 , hdr, nip
$60d0 , $3 , hdr, over hdr, swap hdr, lshift
$646c , $1 , hdr, xor
$6830 , $3 , hdr, @ hdr, over hdr, +
$6501 , $2 , hdr, >r hdr, r@
$60e4 , $2 , hdr, over hdr, @
$6208 , $1 , hdr, io!
$6539 , $2 , hdr, + hdr, >r
$6014 , $2 , hdr, swap hdr, dup
$6403 , $1 , hdr, rdrop
$65ed , $2 , hdr, @ hdr, >r
$630c , $3 , hdr, over hdr, swap hdr, !
$6408 , $2 , hdr, nip hdr, nip
$6b58 , $1 , hdr, or!
$6111 , $3 , hdr, swap hdr, >r hdr, r@
$687c , $2 , hdr, @ hdr, *
$640c , $1 , hdr, nip
$6c0f , $2 , hdr, 2drop hdr, r>
$63e8 , $4 , hdr, swap hdr, @ hdr, swap hdr, !
$64f0 , $1 , hdr, io@
$68ac , $2 , hdr, @ hdr, <
$600b , $3 , hdr, r> hdr, 2drop hdr, nip
$686c , $2 , hdr, @ hdr, xor
$6c03 , $2 , hdr, r> hdr, nip
$6c34 , $3 , hdr, over hdr, r@ hdr, +
$632c , $2 , hdr, tuck hdr, !
$689c , $2 , hdr, @ hdr, =
$6ce4 , $2 , hdr, r@ hdr, @
$6f0f , $3 , hdr, r> hdr, swap hdr, !
$6008 , $2 , hdr, drop hdr, nip
$6084 , $2 , hdr, over hdr, invert
$6559 , $2 , hdr, or hdr, >r
$658d , $2 , hdr, invert hdr, >r
$6c14 , $2 , hdr, swap hdr, r@
$648c , $2 , hdr, invert hdr, nip
$64ec , $2 , hdr, nip hdr, @
$684c , $2 , hdr, @ hdr, and
$6b48 , $1 , hdr, and!
$6b68 , $1 , hdr, xor!
$6c13 , $2 , hdr, nip hdr, r>
$6c37 , $3 , hdr, over hdr, r> hdr, +
$6c87 , $2 , hdr, r> hdr, invert
$6cc3 , $3 , hdr, r> hdr, swap hdr, rshift
$6cd3 , $3 , hdr, r> hdr, swap hdr, lshift
$6410 , $2 , hdr, nip hdr, dup
$6434 , $2 , hdr, 2dup hdr, +
$64a4 , $2 , hdr, 2dup hdr, <
$6549 , $2 , hdr, and hdr, >r
$6880 , $2 , hdr, @ hdr, invert
$68a0 , $3 , hdr, @ hdr, over hdr, >
$68bc , $2 , hdr, @ hdr, u<
$6941 , $4 , hdr, over hdr, over@ hdr, and hdr, >r
$6c24 , $3 , hdr, nip hdr, dup hdr, r@
$6f03 , $3 , hdr, r> hdr, over hdr, !
$6860 , $3 , hdr, @ hdr, over hdr, xor
$6000 , $2 , hdr, drop hdr, dup
$6101 , $2 , hdr, over hdr, >r
$61c9 , $2 , hdr, rshift hdr, >r
$61cd , $4 , hdr, over hdr, swap hdr, rshift hdr, >r
$61d9 , $2 , hdr, lshift hdr, >r
$6438 , $2 , hdr, + hdr, nip
$6448 , $2 , hdr, and hdr, nip
$6470 , $2 , hdr, over hdr, *
$6484 , $2 , hdr, dup hdr, invert
$6488 , $3 , hdr, invert hdr, nip hdr, nip
$64b0 , $2 , hdr, over hdr, u>
$6515 , $3 , hdr, tuck hdr, >r hdr, dup
$68e0 , $2 , hdr, @ hdr, @
$6b3c , $3 , hdr, over hdr, swap hdr, +!
$6b8c , $1 , hdr, not!
$6cd0 , $3 , hdr, r@ hdr, swap hdr, lshift
$6121 , $3 , hdr, swap hdr, >r hdr, dup
$640f , $2 , hdr, nip hdr, rdrop
$64a8 , $2 , hdr, < hdr, nip
$6509 , $2 , hdr, nip hdr, >r
$6569 , $2 , hdr, xor hdr, >r
$6718 , $2 , hdr, swap hdr, !
$6890 , $3 , hdr, @ hdr, over hdr, =
$68b0 , $3 , hdr, @ hdr, over hdr, u>
$6b30 , $2 , hdr, 2dup hdr, +!
$6c30 , $4 , hdr, drop hdr, dup hdr, r@ hdr, +
$6f00 , $3 , hdr, r@ hdr, over hdr, !
$6105 , $3 , hdr, over hdr, >r hdr, dup
$630b , $2 , hdr, rdrop hdr, !
$6402 , $2 , hdr, rdrop hdr, rdrop
$6454 , $2 , hdr, 2dup hdr, or
$64c4 , $3 , hdr, dup hdr, dup hdr, rshift
$650c , $2 , hdr, rdrop hdr, >r
$6884 , $3 , hdr, dup hdr, @ hdr, invert
$6894 , $3 , hdr, 2dup hdr, @ hdr, =
$6b80 , $2 , hdr, dup hdr, not!
$6c00 , $2 , hdr, r@ hdr, nip
$6c27 , $3 , hdr, nip hdr, dup hdr, r>
$6c3f , $3 , hdr, r> hdr, nip hdr, +
$6c60 , $4 , hdr, drop hdr, dup hdr, r@ hdr, xor
$6ca4 , $3 , hdr, over hdr, r@ hdr, <
$6ca7 , $3 , hdr, over hdr, r> hdr, <
$6cc0 , $3 , hdr, r@ hdr, swap hdr, rshift
$6de1 , $3 , hdr, r@ hdr, @ hdr, >r
$6f13 , $3 , hdr, swapr> hdr, over hdr, !
$600a , $4 , hdr, drop hdr, nip hdr, rdrop hdr, rdrop
$600e , $3 , hdr, rdrop hdr, r> hdr, 2drop
$6013 , $2 , hdr, swap hdr, rdrop
$6024 , $2 , hdr, tuck hdr, swap
$60d8 , $2 , hdr, lshift hdr, nip
$60e7 , $3 , hdr, rdrop hdr, over hdr, @
$6110 , $3 , hdr, rdrop hdr, swap>r hdr, r@
$6115 , $3 , hdr, swap>r hdr, r@ hdr, r@
$611c , $3 , hdr, swap hdr, rdrop hdr, >r
$618d , $4 , hdr, drop hdr, dup hdr, invert hdr, >r
$6300 , $2 , hdr, 2dup hdr, !
$632f , $3 , hdr, tuck hdr, rdrop hdr, !
$63e0 , $3 , hdr, over@ hdr, over hdr, !
$63ec , $3 , hdr, over@ hdr, swap hdr, !
$643f , $2 , hdr, + hdr, rdrop
$6444 , $2 , hdr, 2dup hdr, and
$6450 , $2 , hdr, over hdr, or
$645f , $2 , hdr, or hdr, rdrop
$6460 , $2 , hdr, over hdr, xor
$6478 , $2 , hdr, * hdr, nip
$648b , $4 , hdr, invert hdr, nip hdr, nip hdr, rdrop
$64b8 , $2 , hdr, u< hdr, nip
$64d0 , $2 , hdr, dup hdr, lshift
$64d3 , $3 , hdr, dup hdr, rdrop hdr, lshift
$64d8 , $4 , hdr, dup hdr, lshift hdr, nip hdr, nip
$64dc , $3 , hdr, dup hdr, lshift hdr, nip
$64e5 , $3 , hdr, r@ hdr, >r hdr, dup@
$64e8 , $3 , hdr, nip hdr, nip hdr, @
$64eb , $4 , hdr, nip hdr, nip hdr, rdrop hdr, @
$6511 , $2 , hdr, tuck hdr, >r
$651d , $3 , hdr, nip hdr, >r hdr, r@
$6541 , $3 , hdr, 2dup hdr, and hdr, >r
$654d , $3 , hdr, over hdr, and hdr, >r
$6561 , $3 , hdr, 2dup hdr, xor hdr, >r
$6579 , $2 , hdr, * hdr, >r
$6581 , $3 , hdr, dup hdr, invert hdr, >r
$6599 , $2 , hdr, = hdr, >r
$65a9 , $2 , hdr, < hdr, >r
$65ad , $3 , hdr, over hdr, > hdr, >r
$65b9 , $2 , hdr, u< hdr, >r
$65e9 , $3 , hdr, nip hdr, @ hdr, >r
$6840 , $3 , hdr, @ hdr, over hdr, and
$6870 , $3 , hdr, @ hdr, over hdr, *
$688c , $3 , hdr, nip hdr, @ hdr, invert
$6898 , $3 , hdr, @ hdr, = hdr, nip
$68e8 , $4 , hdr, nip hdr, nip hdr, @ hdr, @
$6959 , $3 , hdr, @ hdr, or hdr, >r
$6999 , $3 , hdr, @ hdr, = hdr, >r
$6b18 , $3 , hdr, @ hdr, swap hdr, !
$6b40 , $2 , hdr, 2dup hdr, and!
$6b50 , $2 , hdr, 2dup hdr, or!
$6b6c , $3 , hdr, over hdr, swap hdr, xor!
$6b78 , $1 , hdr, *!
$6bec , $4 , hdr, dup@ hdr, @ hdr, swap hdr, !
$6c10 , $2 , hdr, nip hdr, r@
$6c47 , $3 , hdr, over hdr, r> hdr, and
$6c53 , $4 , hdr, drop hdr, dup hdr, r> hdr, or
$6c84 , $2 , hdr, r@ hdr, invert
$6c97 , $3 , hdr, over hdr, r> hdr, =
$6ca0 , $4 , hdr, drop hdr, dup hdr, r@ hdr, <
$6cb4 , $3 , hdr, over hdr, r@ hdr, u<
$6ce7 , $2 , hdr, r> hdr, @
$6cec , $3 , hdr, 2drop hdr, r@ hdr, @
$6d30 , $4 , hdr, over hdr, r> hdr, + hdr, >r
$6d3d , $4 , hdr, drop hdr, r@ hdr, bounds hdr, swap>r
$6dd0 , $4 , hdr, r> hdr, over hdr, lshift hdr, >r
$6f0b , $4 , hdr, nip hdr, r> hdr, swap hdr, !
$6f0e , $4 , hdr, r> hdr, swap hdr, rdrop hdr, !
$6f5b , $4 , hdr, swapr> hdr, or hdr, swap hdr, !
$6fe3 , $4 , hdr, r> hdr, @ hdr, over hdr, !
0 ,
unhigh

\ The names of an instruction's fields, as vm_opcodes.h has
\ them: where the ALU's input is from, the operation, and where
\ the result goes.  Each is counted strings laid end to end:
\ text, so bulk.
bulk
create f-in  ," N->IN" ," T->IN" ," [T]->IN" ," R->IN"
create f-op  ," IN->" ," T<>N,IN->" ," T->N,IN->" ," IN+N"
             ," IN&N" ," IN|N" ," IN^N" ," IN*N" ," ~IN"
             ," IN==N" ," N<IN" ," Nu<IN" ," IN>>T" ," IN<<T"
             ," [IN]" ," io[IN]"
create f-out  ," ->T" ," ->R" ," ->io[T]" ," ->[T]"
unbulk
