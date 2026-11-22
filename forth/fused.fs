\ fused.fs - the image's instructions as words, for see.fs
\ GENERATED from the image by util/fused.c.
\ An entry is the instruction, how many words it is, and each
\ word's header.  A zero ends the table.

high
create fused
$6800 , $1 , hdr, @
$6010 , $1 , hdr, swap
$6004 , $1 , hdr, over
$6404 , $1 , hdr, dup
$6c07 , $1 , hdr, r>
$650d , $1 , hdr, >r
$600c , $1 , hdr, drop
$649c , $1 , hdr, =
$644c , $1 , hdr, and
$6308 , $1 , hdr, !
$6c04 , $1 , hdr, r@
$6480 , $1 , hdr, invert
$643c , $1 , hdr, +
$60cc , $1 , hdr, rshift

$60dc , $1 , hdr, lshift
$64ac , $1 , hdr, <
$6b38 , $1 , hdr, +!
$645c , $1 , hdr, or
$6490 , $2 , hdr, over hdr, =
$6d08 , $1 , hdr, 2drop
$6c17 , $2 , hdr, swap hdr, r>
$647c , $1 , hdr, *
$6494 , $2 , hdr, 2dup hdr, =
$6464 , $2 , hdr, 2dup hdr, xor
$640c , $1 , hdr, nip
$6804 , $2 , hdr, dup hdr, @
$64bc , $1 , hdr, u<
$611d , $2 , hdr, swap hdr, >r
$6430 , $2 , hdr, over hdr, +
$64a0 , $2 , hdr, over hdr, >

$6440 , $2 , hdr, over hdr, and
$6414 , $1 , hdr, tuck
$60c0 , $3 , hdr, over hdr, swap hdr, rshift
$600f , $2 , hdr, r> hdr, 2drop
$646c , $1 , hdr, xor
$6501 , $2 , hdr, >r hdr, r@
$683c , $2 , hdr, @ hdr, +
$6403 , $1 , hdr, rdrop
$6014 , $2 , hdr, swap hdr, dup
$6408 , $2 , hdr, nip hdr, nip
$60d0 , $3 , hdr, over hdr, swap hdr, lshift
$60e4 , $2 , hdr, over hdr, @
$6208 , $1 , hdr, io!
$6830 , $3 , hdr, @ hdr, over hdr, +
$68a0 , $3 , hdr, @ hdr, over hdr, >
$6111 , $3 , hdr, swap hdr, >r hdr, r@

$6539 , $2 , hdr, + hdr, >r
$630c , $3 , hdr, over hdr, swap hdr, !
$6b58 , $1 , hdr, or!
$690d , $2 , hdr, @ hdr, >r
$6c34 , $3 , hdr, over hdr, r@ hdr, +
$6c0f , $2 , hdr, 2drop hdr, r>
$63e8 , $4 , hdr, swap hdr, @ hdr, swap hdr, !
$64f0 , $1 , hdr, io@
$687c , $2 , hdr, @ hdr, *
$6c03 , $2 , hdr, r> hdr, nip
$689c , $2 , hdr, @ hdr, =
$6f0f , $3 , hdr, r> hdr, swap hdr, !
$600b , $3 , hdr, r> hdr, 2drop hdr, nip
$648c , $2 , hdr, invert hdr, nip
$686c , $2 , hdr, @ hdr, xor
$6008 , $2 , hdr, drop hdr, nip

$6434 , $2 , hdr, 2dup hdr, +
$680c , $2 , hdr, nip hdr, @
$68ac , $2 , hdr, @ hdr, <
$632c , $2 , hdr, tuck hdr, !
$6559 , $2 , hdr, or hdr, >r
$6c13 , $2 , hdr, nip hdr, r>
$6cc3 , $3 , hdr, r> hdr, swap hdr, rshift
$6cd3 , $3 , hdr, r> hdr, swap hdr, lshift
$6084 , $2 , hdr, over hdr, invert
$6101 , $2 , hdr, over hdr, >r
$6410 , $2 , hdr, nip hdr, dup
$64b0 , $2 , hdr, over hdr, u>
$658d , $2 , hdr, invert hdr, >r
$6718 , $2 , hdr, swap hdr, !
$6880 , $2 , hdr, @ hdr, invert
$6c14 , $2 , hdr, swap hdr, r@

$640f , $2 , hdr, nip hdr, rdrop
$6484 , $2 , hdr, dup hdr, invert
$6549 , $2 , hdr, and hdr, >r
$6840 , $3 , hdr, @ hdr, over hdr, and
$6890 , $3 , hdr, @ hdr, over hdr, =
$68bc , $2 , hdr, @ hdr, u<
$6b48 , $1 , hdr, and!
$6b68 , $1 , hdr, xor!
$6c37 , $3 , hdr, over hdr, r> hdr, +
$6c87 , $2 , hdr, r> hdr, invert
$6ce4 , $2 , hdr, r@ hdr, @
$6105 , $3 , hdr, over hdr, >r hdr, dup
$61d9 , $2 , hdr, lshift hdr, >r
$6488 , $3 , hdr, invert hdr, nip hdr, nip
$684c , $2 , hdr, @ hdr, and
$68b0 , $3 , hdr, @ hdr, over hdr, u>

$68e0 , $2 , hdr, @ hdr, @
$6941 , $4 , hdr, 2dup hdr, @ hdr, and hdr, >r
$6c24 , $3 , hdr, nip hdr, dup hdr, r@
$6f03 , $3 , hdr, r> hdr, over hdr, !
$6448 , $2 , hdr, and hdr, nip
$6460 , $2 , hdr, over hdr, xor
$64a4 , $2 , hdr, 2dup hdr, <
$6860 , $3 , hdr, @ hdr, over hdr, xor
$6884 , $3 , hdr, dup hdr, @ hdr, invert
$6894 , $3 , hdr, 2dup hdr, @ hdr, =
$6000 , $2 , hdr, drop hdr, dup
$611c , $3 , hdr, swap hdr, rdrop hdr, >r
$6121 , $3 , hdr, swap hdr, >r hdr, dup
$61c9 , $2 , hdr, rshift hdr, >r
$61cd , $4 , hdr, over hdr, swap hdr, rshift hdr, >r
$6438 , $2 , hdr, + hdr, nip

$64a8 , $2 , hdr, < hdr, nip
$6515 , $3 , hdr, tuck hdr, >r hdr, dup
$6b3c , $3 , hdr, over hdr, swap hdr, +!
$6b8c , $1 , hdr, not!
$6c60 , $4 , hdr, drop hdr, dup hdr, r@ hdr, xor
$6ce7 , $2 , hdr, r> hdr, @
$6024 , $2 , hdr, tuck hdr, swap
$6470 , $2 , hdr, over hdr, *
$648b , $4 , hdr, invert hdr, nip hdr, nip hdr, rdrop
$64d0 , $2 , hdr, dup hdr, lshift
$6511 , $2 , hdr, tuck hdr, >r
$6569 , $2 , hdr, xor hdr, >r
$6c30 , $4 , hdr, drop hdr, dup hdr, r@ hdr, +
$6f00 , $3 , hdr, r@ hdr, over hdr, !
$600e , $3 , hdr, rdrop hdr, r> hdr, 2drop
$60d8 , $2 , hdr, lshift hdr, nip

$630b , $2 , hdr, rdrop hdr, !
$6402 , $2 , hdr, rdrop hdr, rdrop
$6454 , $2 , hdr, 2dup hdr, or
$64c4 , $3 , hdr, dup hdr, dup hdr, rshift
$6505 , $3 , hdr, >r hdr, r@ hdr, r@
$6531 , $3 , hdr, 2dup hdr, + hdr, >r
$6561 , $3 , hdr, 2dup hdr, xor hdr, >r
$65a9 , $2 , hdr, < hdr, >r
$6814 , $2 , hdr, tuck hdr, @
$6870 , $3 , hdr, @ hdr, over hdr, *
$6b30 , $2 , hdr, 2dup hdr, +!
$6b40 , $2 , hdr, 2dup hdr, and!
$6b50 , $2 , hdr, 2dup hdr, or!
$6b80 , $2 , hdr, dup hdr, not!
$6c27 , $3 , hdr, nip hdr, dup hdr, r>
$6c3f , $3 , hdr, r> hdr, nip hdr, +

$6c47 , $3 , hdr, over hdr, r> hdr, and
$6ca4 , $3 , hdr, over hdr, r@ hdr, <
$6ca7 , $3 , hdr, over hdr, r> hdr, <
$6cc0 , $3 , hdr, r@ hdr, swap hdr, rshift
$6de1 , $3 , hdr, r@ hdr, @ hdr, >r
$6f13 , $4 , hdr, swap hdr, r> hdr, over hdr, !
$600a , $4 , hdr, drop hdr, nip hdr, rdrop hdr, rdrop
$6013 , $2 , hdr, swap hdr, rdrop
$60e7 , $3 , hdr, rdrop hdr, over hdr, @
$618d , $4 , hdr, drop hdr, dup hdr, invert hdr, >r
$61c1 , $3 , hdr, 2dup hdr, rshift hdr, >r
$6300 , $2 , hdr, 2dup hdr, !
$6324 , $4 , hdr, tuck hdr, tuck hdr, tuck hdr, !
$632f , $3 , hdr, tuck hdr, rdrop hdr, !
$63e0 , $4 , hdr, over hdr, @ hdr, over hdr, !
$63ec , $4 , hdr, over hdr, @ hdr, swap hdr, !

$640b , $3 , hdr, nip hdr, nip hdr, rdrop
$643f , $2 , hdr, + hdr, rdrop
$6444 , $2 , hdr, 2dup hdr, and
$6450 , $2 , hdr, over hdr, or
$645b , $3 , hdr, or hdr, nip hdr, rdrop
$645f , $2 , hdr, or hdr, rdrop
$6482 , $3 , hdr, invert hdr, rdrop hdr, rdrop
$6498 , $2 , hdr, = hdr, nip
$64b4 , $2 , hdr, 2dup hdr, u<
$64b8 , $2 , hdr, u< hdr, nip
$64d3 , $3 , hdr, dup hdr, rdrop hdr, lshift
$64d8 , $4 , hdr, dup hdr, lshift hdr, nip hdr, nip
$6541 , $3 , hdr, 2dup hdr, and hdr, >r
$654d , $3 , hdr, over hdr, and hdr, >r
$6579 , $2 , hdr, * hdr, >r
$6581 , $3 , hdr, dup hdr, invert hdr, >r

$6599 , $2 , hdr, = hdr, >r
$65ad , $3 , hdr, over hdr, > hdr, >r
$65b9 , $2 , hdr, u< hdr, >r
$670c , $2 , hdr, dup hdr, !
$6810 , $3 , hdr, nip hdr, dup hdr, @
$6838 , $3 , hdr, @ hdr, + hdr, nip
$6864 , $3 , hdr, 2dup hdr, @ hdr, xor
$6878 , $3 , hdr, @ hdr, * hdr, nip
$6898 , $3 , hdr, @ hdr, = hdr, nip
$68a4 , $3 , hdr, 2dup hdr, @ hdr, <
$68e8 , $4 , hdr, nip hdr, nip hdr, @ hdr, @
$6901 , $3 , hdr, dup hdr, @ hdr, >r
$6959 , $3 , hdr, @ hdr, or hdr, >r
$6999 , $3 , hdr, @ hdr, = hdr, >r
$699d , $4 , hdr, @ hdr, over hdr, = hdr, >r
$6b18 , $3 , hdr, @ hdr, swap hdr, !

$6b6c , $3 , hdr, over hdr, swap hdr, xor!
$6b78 , $1 , hdr, *!
$6c00 , $2 , hdr, r@ hdr, nip
$6c0b , $3 , hdr, + hdr, 2drop hdr, r>
$6c10 , $2 , hdr, nip hdr, r@
$6c53 , $4 , hdr, drop hdr, dup hdr, r> hdr, or
$6c54 , $3 , hdr, over hdr, r@ hdr, or
$6c84 , $2 , hdr, r@ hdr, invert
$6c97 , $3 , hdr, over hdr, r> hdr, =
$6ca0 , $4 , hdr, drop hdr, dup hdr, r@ hdr, <
$6cb4 , $3 , hdr, over hdr, r@ hdr, u<
$6cd0 , $3 , hdr, r@ hdr, swap hdr, lshift
$6cdf , $4 , hdr, nip hdr, r> hdr, swap hdr, lshift
$6cec , $3 , hdr, 2drop hdr, r@ hdr, @
$6d30 , $4 , hdr, over hdr, r> hdr, + hdr, >r
$6dd0 , $4 , hdr, r> hdr, over hdr, lshift hdr, >r

$6f0b , $4 , hdr, nip hdr, r> hdr, swap hdr, !
$6f0c , $3 , hdr, r@ hdr, swap hdr, !
$6f0e , $4 , hdr, r> hdr, swap hdr, rdrop hdr, !
$6fe3 , $4 , hdr, r> hdr, @ hdr, over hdr, !
0 ,
unhigh
meta t' fused t' fused-at tvar! target
