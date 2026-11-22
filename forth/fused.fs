\ fused.fs - the image's instructions as words, for see.fs
\ GENERATED from the image by util/fused.c.
\ An entry is the instruction, how many words it is, and each
\ word's header: fields of sixteen bits, one after another in
\ cells.  A field that is nothing ends the table.

high
create fused
$6800 w, $1 w, hdr, @
$6010 w, $1 w, hdr, swap
$6004 w, $1 w, hdr, over
$6404 w, $1 w, hdr, dup
$6c07 w, $1 w, hdr, r>
$650d w, $1 w, hdr, >r
$600c w, $1 w, hdr, drop
$649c w, $1 w, hdr, =
$644c w, $1 w, hdr, and
$6c04 w, $1 w, hdr, r@
$6308 w, $1 w, hdr, !
$643c w, $1 w, hdr, +
$60dc w, $1 w, hdr, lshift
$6480 w, $1 w, hdr, invert

$60cc w, $1 w, hdr, rshift
$6094 w, $1 w, hdr, true
$64ac w, $1 w, hdr, <
$6b38 w, $1 w, hdr, +!
$645c w, $1 w, hdr, or
$6490 w, $2 w, hdr, over hdr, =
$6c17 w, $2 w, hdr, swap hdr, r>
$6d08 w, $1 w, hdr, 2drop
$6464 w, $2 w, hdr, 2dup hdr, xor
$647c w, $1 w, hdr, *
$6494 w, $2 w, hdr, 2dup hdr, =
$6064 w, $1 w, hdr, false
$636c w, $3 w, hdr, false hdr, swap hdr, !
$64bc w, $1 w, hdr, u<
$6804 w, $2 w, hdr, dup hdr, @
$6430 w, $2 w, hdr, over hdr, +

$611d w, $2 w, hdr, swap hdr, >r
$6440 w, $2 w, hdr, over hdr, and
$6414 w, $1 w, hdr, tuck
$60c0 w, $3 w, hdr, over hdr, swap hdr, rshift
$64a0 w, $2 w, hdr, over hdr, >
$640c w, $1 w, hdr, nip
$600f w, $2 w, hdr, r> hdr, 2drop
$646c w, $1 w, hdr, xor
$683c w, $2 w, hdr, @ hdr, +
$6501 w, $2 w, hdr, >r hdr, r@
$6014 w, $2 w, hdr, swap hdr, dup
$6208 w, $1 w, hdr, io!
$6403 w, $1 w, hdr, rdrop
$6539 w, $2 w, hdr, + hdr, >r
$6830 w, $3 w, hdr, @ hdr, over hdr, +
$606c w, $2 w, hdr, 2drop hdr, false

$60d0 w, $3 w, hdr, over hdr, swap hdr, lshift
$6111 w, $3 w, hdr, swap hdr, >r hdr, r@
$68a0 w, $3 w, hdr, @ hdr, over hdr, >
$6060 w, $2 w, hdr, dup hdr, xor
$60e4 w, $2 w, hdr, over hdr, @
$630c w, $3 w, hdr, over hdr, swap hdr, !
$6b58 w, $1 w, hdr, or!
$690d w, $2 w, hdr, @ hdr, >r
$6c34 w, $3 w, hdr, over hdr, r@ hdr, +
$6c0f w, $2 w, hdr, 2drop hdr, r>
$639c w, $3 w, hdr, true hdr, swap hdr, !
$63e8 w, $4 w, hdr, swap hdr, @ hdr, swap hdr, !
$64f0 w, $1 w, hdr, io@
$687c w, $2 w, hdr, @ hdr, *
$6c03 w, $2 w, hdr, r> hdr, nip
$689c w, $2 w, hdr, @ hdr, =

$600b w, $3 w, hdr, r> hdr, 2drop hdr, nip
$686c w, $2 w, hdr, @ hdr, xor
$6cd3 w, $3 w, hdr, r> hdr, swap hdr, lshift
$6f0f w, $3 w, hdr, r> hdr, swap hdr, !
$6090 w, $2 w, hdr, dup hdr, =
$6101 w, $2 w, hdr, over hdr, >r
$632c w, $2 w, hdr, tuck hdr, !
$6434 w, $2 w, hdr, 2dup hdr, +
$680c w, $2 w, hdr, nip hdr, @
$68ac w, $2 w, hdr, @ hdr, <
$6cc3 w, $3 w, hdr, r> hdr, swap hdr, rshift
$6008 w, $2 w, hdr, drop hdr, nip
$6559 w, $2 w, hdr, or hdr, >r
$6c13 w, $2 w, hdr, nip hdr, r>
$6068 w, $3 w, hdr, + hdr, 2drop hdr, false
$6084 w, $2 w, hdr, over hdr, invert

$6410 w, $2 w, hdr, nip hdr, dup
$64b0 w, $2 w, hdr, over hdr, u>
$6718 w, $2 w, hdr, swap hdr, !
$6880 w, $2 w, hdr, @ hdr, invert
$6890 w, $3 w, hdr, @ hdr, over hdr, =
$6c14 w, $2 w, hdr, swap hdr, r@
$6c37 w, $3 w, hdr, over hdr, r> hdr, +
$609c w, $2 w, hdr, 2drop hdr, true
$6105 w, $3 w, hdr, over hdr, >r hdr, dup
$6191 w, $2 w, hdr, true hdr, >r
$6408 w, $2 w, hdr, nip hdr, nip
$640f w, $2 w, hdr, nip hdr, rdrop
$6840 w, $3 w, hdr, @ hdr, over hdr, and
$68bc w, $2 w, hdr, @ hdr, u<
$6b48 w, $1 w, hdr, and!
$6b68 w, $1 w, hdr, xor!

$6c87 w, $2 w, hdr, r> hdr, invert
$6ce4 w, $2 w, hdr, r@ hdr, @
$61d9 w, $2 w, hdr, lshift hdr, >r
$6438 w, $2 w, hdr, + hdr, nip
$6549 w, $2 w, hdr, and hdr, >r
$684c w, $2 w, hdr, @ hdr, and
$68b0 w, $3 w, hdr, @ hdr, over hdr, u>
$68e0 w, $2 w, hdr, @ hdr, @
$6941 w, $4 w, hdr, 2dup hdr, @ hdr, and hdr, >r
$6c24 w, $3 w, hdr, nip hdr, dup hdr, r@
$6f03 w, $3 w, hdr, r> hdr, over hdr, !
$606f w, $3 w, hdr, 2drop hdr, rdrop hdr, false
$6121 w, $3 w, hdr, swap hdr, >r hdr, dup
$6165 w, $3 w, hdr, dup hdr, false hdr, >r
$6368 w, $4 w, hdr, nip hdr, false hdr, swap hdr, !
$6448 w, $2 w, hdr, and hdr, nip

$6460 w, $2 w, hdr, over hdr, xor
$6484 w, $2 w, hdr, dup hdr, invert
$64a4 w, $2 w, hdr, 2dup hdr, <
$6860 w, $3 w, hdr, @ hdr, over hdr, xor
$6884 w, $3 w, hdr, dup hdr, @ hdr, invert
$6000 w, $2 w, hdr, drop hdr, dup
$609f w, $3 w, hdr, 2drop hdr, rdrop hdr, true
$611c w, $3 w, hdr, swap hdr, rdrop hdr, >r
$61c9 w, $2 w, hdr, rshift hdr, >r
$61cd w, $4 w, hdr, over hdr, swap hdr, rshift hdr, >r
$64a8 w, $2 w, hdr, < hdr, nip
$6894 w, $3 w, hdr, 2dup hdr, @ hdr, =
$6b3c w, $3 w, hdr, over hdr, swap hdr, +!
$6b8c w, $1 w, hdr, not!
$6c60 w, $4 w, hdr, drop hdr, dup hdr, r@ hdr, xor
$6ce7 w, $2 w, hdr, r> hdr, @

$6003 w, $3 w, hdr, r> hdr, 2drop hdr, dup
$6024 w, $2 w, hdr, tuck hdr, swap
$6470 w, $2 w, hdr, over hdr, *
$648c w, $2 w, hdr, invert hdr, nip
$64d0 w, $2 w, hdr, dup hdr, lshift
$6511 w, $2 w, hdr, tuck hdr, >r
$6515 w, $3 w, hdr, tuck hdr, >r hdr, dup
$654d w, $3 w, hdr, over hdr, and hdr, >r
$6569 w, $2 w, hdr, xor hdr, >r
$6901 w, $3 w, hdr, dup hdr, @ hdr, >r
$6c30 w, $4 w, hdr, drop hdr, dup hdr, r@ hdr, +
$6ca7 w, $3 w, hdr, over hdr, r> hdr, <
$6f00 w, $3 w, hdr, r@ hdr, over hdr, !
$6f13 w, $4 w, hdr, swap hdr, r> hdr, over hdr, !
$600e w, $3 w, hdr, rdrop hdr, r> hdr, 2drop
$60d8 w, $2 w, hdr, lshift hdr, nip

$6161 w, $2 w, hdr, false hdr, >r
$630b w, $2 w, hdr, rdrop hdr, !
$6360 w, $3 w, hdr, false hdr, over hdr, !
$6402 w, $2 w, hdr, rdrop hdr, rdrop
$6454 w, $2 w, hdr, 2dup hdr, or
$6505 w, $3 w, hdr, >r hdr, r@ hdr, r@
$6531 w, $3 w, hdr, 2dup hdr, + hdr, >r
$6561 w, $3 w, hdr, 2dup hdr, xor hdr, >r
$658d w, $2 w, hdr, invert hdr, >r
$6814 w, $2 w, hdr, tuck hdr, @
$6838 w, $3 w, hdr, @ hdr, + hdr, nip
$6870 w, $3 w, hdr, @ hdr, over hdr, *
$6911 w, $3 w, hdr, tuck hdr, @ hdr, >r
$6b18 w, $3 w, hdr, @ hdr, swap hdr, !
$6b30 w, $2 w, hdr, 2dup hdr, +!
$6b40 w, $2 w, hdr, 2dup hdr, and!

$6b50 w, $2 w, hdr, 2dup hdr, or!
$6b80 w, $2 w, hdr, dup hdr, not!
$6c3f w, $3 w, hdr, r> hdr, nip hdr, +
$6c47 w, $3 w, hdr, over hdr, r> hdr, and
$6ca4 w, $3 w, hdr, over hdr, r@ hdr, <
$6cc0 w, $3 w, hdr, r@ hdr, swap hdr, rshift
$6ccf w, $4 w, hdr, nip hdr, r> hdr, swap hdr, rshift
$6dd1 w, $4 w, hdr, r@ hdr, over hdr, lshift hdr, >r
$6de1 w, $3 w, hdr, r@ hdr, @ hdr, >r
$6f0b w, $4 w, hdr, nip hdr, r> hdr, swap hdr, !
$600a w, $4 w, hdr, drop hdr, nip hdr, rdrop hdr, rdrop
$6013 w, $2 w, hdr, swap hdr, rdrop
$6096 w, $3 w, hdr, rdrop hdr, rdrop hdr, true
$6098 w, $3 w, hdr, 2drop hdr, dup hdr, =
$60e7 w, $3 w, hdr, rdrop hdr, over hdr, @
$6181 w, $3 w, hdr, over hdr, invert hdr, >r

$618d w, $4 w, hdr, drop hdr, dup hdr, invert hdr, >r
$61c1 w, $3 w, hdr, 2dup hdr, rshift hdr, >r
$61d1 w, $3 w, hdr, 2dup hdr, lshift hdr, >r
$6300 w, $2 w, hdr, 2dup hdr, !
$6324 w, $4 w, hdr, tuck hdr, tuck hdr, tuck hdr, !
$632f w, $3 w, hdr, tuck hdr, rdrop hdr, !
$6398 w, $4 w, hdr, nip hdr, true hdr, swap hdr, !
$63e0 w, $4 w, hdr, over hdr, @ hdr, over hdr, !
$63ec w, $4 w, hdr, over hdr, @ hdr, swap hdr, !
$6417 w, $2 w, hdr, tuck hdr, rdrop
$643f w, $2 w, hdr, + hdr, rdrop
$6444 w, $2 w, hdr, 2dup hdr, and
$6450 w, $2 w, hdr, over hdr, or
$6458 w, $2 w, hdr, or hdr, nip
$645b w, $3 w, hdr, or hdr, nip hdr, rdrop
$645f w, $2 w, hdr, or hdr, rdrop

$6498 w, $2 w, hdr, = hdr, nip
$64b4 w, $2 w, hdr, 2dup hdr, u<
$64b8 w, $2 w, hdr, u< hdr, nip
$64d3 w, $3 w, hdr, dup hdr, rdrop hdr, lshift
$64d8 w, $4 w, hdr, dup hdr, lshift hdr, nip hdr, nip
$6541 w, $3 w, hdr, 2dup hdr, and hdr, >r
$6579 w, $2 w, hdr, * hdr, >r
$6581 w, $3 w, hdr, dup hdr, invert hdr, >r
$6599 w, $2 w, hdr, = hdr, >r
$65a1 w, $3 w, hdr, 2dup hdr, < hdr, >r
$65a9 w, $2 w, hdr, < hdr, >r
$65ad w, $3 w, hdr, over hdr, > hdr, >r
$65b9 w, $2 w, hdr, u< hdr, >r
$670c w, $2 w, hdr, dup hdr, !
$671c w, $3 w, hdr, tuck hdr, swap hdr, !
$6807 w, $3 w, hdr, dup hdr, rdrop hdr, @

$6810 w, $3 w, hdr, nip hdr, dup hdr, @
$6834 w, $3 w, hdr, 2dup hdr, @ hdr, +
$6864 w, $3 w, hdr, 2dup hdr, @ hdr, xor
$6878 w, $3 w, hdr, @ hdr, * hdr, nip
$6898 w, $3 w, hdr, @ hdr, = hdr, nip
$68a4 w, $3 w, hdr, 2dup hdr, @ hdr, <
$68e8 w, $4 w, hdr, nip hdr, nip hdr, @ hdr, @
$6908 w, $4 w, hdr, nip hdr, rdrop hdr, @ hdr, >r
$6959 w, $3 w, hdr, @ hdr, or hdr, >r
$6999 w, $3 w, hdr, @ hdr, = hdr, >r
$699d w, $4 w, hdr, @ hdr, over hdr, = hdr, >r
$6b6c w, $3 w, hdr, over hdr, swap hdr, xor!
$6b78 w, $1 w, hdr, *!
$6c00 w, $2 w, hdr, r@ hdr, nip
$6c0b w, $3 w, hdr, + hdr, 2drop hdr, r>
$6c10 w, $2 w, hdr, nip hdr, r@

$6c27 w, $3 w, hdr, nip hdr, dup hdr, r>
$6c53 w, $4 w, hdr, drop hdr, dup hdr, r> hdr, or
$6c54 w, $3 w, hdr, over hdr, r@ hdr, or
$6c84 w, $2 w, hdr, r@ hdr, invert
$6c97 w, $3 w, hdr, over hdr, r> hdr, =
$6ca0 w, $4 w, hdr, drop hdr, dup hdr, r@ hdr, <
$6cb4 w, $3 w, hdr, over hdr, r@ hdr, u<
$6cb7 w, $3 w, hdr, over hdr, r> hdr, u<
$6cd0 w, $3 w, hdr, r@ hdr, swap hdr, lshift
$6cdf w, $4 w, hdr, nip hdr, r> hdr, swap hdr, lshift
$6cec w, $3 w, hdr, 2drop hdr, r@ hdr, @
$6d30 w, $4 w, hdr, over hdr, r> hdr, + hdr, >r
$6dd0 w, $4 w, hdr, r> hdr, over hdr, lshift hdr, >r
$6f0c w, $3 w, hdr, r@ hdr, swap hdr, !
$6f0e w, $4 w, hdr, r> hdr, swap hdr, rdrop hdr, !
$6fe3 w, $4 w, hdr, r> hdr, @ hdr, over hdr, !

$6fef w, $4 w, hdr, r> hdr, @ hdr, swap hdr, !
0 w,
meta tcalign target
unhigh
meta t' fused t' fused-at tvar! target
