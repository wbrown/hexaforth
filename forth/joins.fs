\ joins.fs - the rules of the nucleus's peephole (join.fs)
\
\ These are 59 rules.  55 are of the 360 that were once made from a built
\ image: those that save at least their own eight bytes in the system's
\ code, and those that save at least a ten-thousandth of the instructions
\ that the desktop's test or a benchmark runs (the target `peephole`).  The
\ other four are for what is typed and the system's own source happens
\ not to say: the two that make `2dup xor` one instruction, which is how a
\ loop over memory is written in standard words, and the two that make
\ `r> @ over !` one, the store that moves a cell.  The rest of the 360
\ were taken out.
\
\ A rule is four instructions, and they are in order: nothing or the
\ instruction two before, the one before, the one being compiled, and
\ the one instruction they are together.  join-rules, in the kernel,
\ is told where they are and how many.  They are all on one page.

meta $ec table-room  tcp @ 2/ t' join-rules tvar! target
$0000 i, $6004 i, $6430 i, $6434 i,   \ over , over +
$0000 i, $6004 i, $643c i, $6430 i,   \ over , +
$0000 i, $6004 i, $6440 i, $6444 i,   \ over , over and
$0000 i, $6004 i, $644c i, $6440 i,   \ over , and
$0000 i, $6004 i, $6460 i, $6464 i,   \ over , over xor
$0000 i, $6004 i, $646c i, $6460 i,   \ over , xor
$0000 i, $6004 i, $6480 i, $6084 i,   \ over , invert
$0000 i, $6004 i, $6490 i, $6494 i,   \ over , over =
$0000 i, $6004 i, $649c i, $6490 i,   \ over , =
$0000 i, $6004 i, $6718 i, $630c i,   \ over , swap !
$0000 i, $6004 i, $6800 i, $60e4 i,   \ over , @
$0000 i, $600c i, $6403 i, $600f i,   \ drop , rdrop
$0000 i, $600c i, $6404 i, $6000 i,   \ drop , dup
$0000 i, $600c i, $6c07 i, $6c03 i,   \ drop , r>
$0000 i, $6014 i, $650d i, $6111 i,   \ swap dup , >r
$0000 i, $6020 i, $6004 i, $6414 i,   \ swap , over
$0000 i, $6020 i, $6308 i, $6718 i,   \ swap , !
$0000 i, $6020 i, $6404 i, $6014 i,   \ swap , dup
$0000 i, $6020 i, $650d i, $612d i,   \ swap , >r
$0000 i, $6020 i, $6c04 i, $6c14 i,   \ swap , r@
$0000 i, $6020 i, $6c07 i, $6c17 i,   \ swap , r>
$0000 i, $60c0 i, $650d i, $61cd i,   \ over swap rshift , >r
$0000 i, $6111 i, $6404 i, $6115 i,   \ swap >r r@ , dup
$0000 i, $6308 i, $6403 i, $630b i,   \ ! , rdrop
$0000 i, $6403 i, $650d i, $650c i,   \ rdrop , >r
$0000 i, $6404 i, $6480 i, $6484 i,   \ dup , invert
$0000 i, $6404 i, $650d i, $6501 i,   \ dup , >r
$0000 i, $6404 i, $6800 i, $6804 i,   \ dup , @
$0000 i, $6414 i, $6020 i, $6024 i,   \ tuck , swap
$0000 i, $642c i, $6c07 i, $6c13 i,   \ nip , r>
$0000 i, $643c i, $650d i, $6539 i,   \ + , >r
$0000 i, $6440 i, $650d i, $654d i,   \ over and , >r
$0000 i, $645c i, $650d i, $6559 i,   \ or , >r
$0000 i, $64a0 i, $650d i, $65ad i,   \ over > , >r
$0000 i, $650d i, $6c04 i, $6501 i,   \ >r , r@
$0000 i, $6800 i, $6430 i, $6830 i,   \ @ , over +
$0000 i, $6800 i, $643c i, $683c i,   \ @ , +
$0000 i, $6800 i, $6440 i, $6840 i,   \ @ , over and
$0000 i, $6800 i, $644c i, $684c i,   \ @ , and
$0000 i, $6800 i, $646c i, $686c i,   \ @ , xor
$0000 i, $6800 i, $647c i, $687c i,   \ @ , *
$0000 i, $6800 i, $6480 i, $6880 i,   \ @ , invert
$0000 i, $6800 i, $64ac i, $68ac i,   \ @ , <
$0000 i, $6800 i, $64bc i, $68bc i,   \ @ , u<
$0000 i, $6800 i, $650d i, $65ed i,   \ @ , >r
$0000 i, $6c04 i, $6800 i, $6ce4 i,   \ r@ , @
$0000 i, $6c07 i, $6480 i, $6c87 i,   \ r> , invert
$0000 i, $6c07 i, $6800 i, $6ce7 i,   \ r> , @
$0000 i, $6c17 i, $650d i, $6020 i,   \ swapr> , >r
$0000 i, $6d08 i, $6c07 i, $6c0f i,   \ 2drop , r>
$6004 i, $6004 i, $645c i, $6454 i,   \ over , over , or
$6004 i, $6004 i, $64ac i, $64a4 i,   \ over , over , <
$6004 i, $6020 i, $64ac i, $64a0 i,   \ over , swap , <
$6004 i, $60e4 i, $649c i, $6894 i,   \ over , over@ , =
$6004 i, $6c04 i, $643c i, $6c34 i,   \ over , r@ , +
$6004 i, $6c04 i, $64ac i, $6ca4 i,   \ over , r@ , <
$6004 i, $6c07 i, $643c i, $6c37 i,   \ over , r> , +
$6c04 i, $6004 i, $6308 i, $6f00 i,   \ r@ , over , !
$6ce7 i, $6004 i, $6308 i, $6fe3 i,   \ r> @ , over , !
meta $3b t' join-rules 1+ tvar! target
