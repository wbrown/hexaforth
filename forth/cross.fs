( J1 Cross Compiler                          JCB 16:55 05/02/12)

\   Usage gforth cross.fs <machine.fs> <program.fs>
\
\   Where machine.fs defines the target machine
\   and program.fs is the target program
\

variable lst        \ .lst output file handle

: h#
    base @ >r 16 base !
    0. bl parse >number throw 2drop postpone literal
    r> base ! ; immediate

: tcell     2 ;
: tcells    tcell * ;
: tcell+    tcell + ;

131072 allocate throw constant tflash       \ bytes, target flash
131072 cells allocate throw constant _tbranches   \ branch targets, one cell per byte address
tflash      131072 erase
_tbranches  131072 cells erase
: tbranches cells _tbranches + ;

variable tdp    $4000 tdp !     \ Data pointer

\ Dictionary headers and labels are laid down in a region of their own,
\ above the data, so that they do not push variables up: a variable in the
\ 4,096 cells from var-base is fetched and stored by one instruction.  thp
\ is where the next header goes.  At run time the nucleus does the same:
\ its data carries on from the end of the data here, and its headers from
\ the end of the headers.
$a000 constant hdr-base
variable thp    hdr-base thp !
variable dsave
: >hdr ( -- ) tdp @ dsave !  thp @ tdp ! ;      \ lay down in the header region
: hdr> ( -- ) tdp @ thp !  dsave @ tdp ! ;      \ and go back to the data
variable tcp    0 tcp !         \ Code pointer
: there     tdp @ ;
: islegal   ;
: tc!       islegal tflash + c! ;
: tc@       islegal tflash + c@ ;
: tw!       islegal tflash + w! ;
: t!        islegal tflash + l! ;
: t@        islegal tflash + uw@ ;
: twalign   tdp @ 1+ -2 and tdp ! ;
: talign    tdp @ 3 + -4 and tdp ! ;
: tc,       there tc! 1 tdp +! ;
: t,        there t!  4 tdp +! ;
: tw,       there tw! tcell tdp +! ;

( Peephole )

\ Instructions are emitted one at a time through tcode,.  Each one is first
\ offered to the instructions just before it:
\
\   exit      joins an ALU instruction that leaves the return stack alone, by
\             setting RET and r-1 on it.
\   +         after a pushed literal turns the literal into an add: `d# 8 +`
\             is one instruction.
\   anything  that forms a pair or triple listed in peephole.fs is replaced by
\             the one instruction it equals.  The result is offered again, so
\             fusions cascade.  Three instructions that are not one but are
\             listed as equal to two are replaced by the two, each offered
\             again.
\   n +       after a literal or another n + of the same shift is added to
\             it, if the sum still fits: `1+ 1+` is one instruction.
\   @ and !   after the address of a variable are one instruction with it.
\
\ The tables come from util/insn_map.py, which derives them from a symbolic
\ model of vm.c and checks every entry on the VM itself.
\
\ Two things stop a fusion:
\   tfence   the most recent address a branch or call can land on.  The
\            instruction there may start a fused group but not sit inside one.
\   tpinned  an instruction that the word called just before it will read
\            and copy (see inline-operand), so it must stay as written.

variable tfence     0 tfence !
variable tpinned    -1 tpinned !
variable peephole   true peephole !
s" PEEPHOLE" getenv s" off" compare 0= [if] false peephole ! [then]   \ PEEPHOLE=off gforth ...
: label     tcp @ tfence ! ;

s" peephole.fs" included

\ A table is in order of what it matches, so it is searched by halving.
variable fmid
: fuse-lookup { key table n -- value true | false }
    0 n
    begin
        2dup <
    while
        2dup + 2/ fmid !
        table fmid @ 2* cells + @
        dup key = if drop 2drop  table fmid @ 2* cells + cell+ @  true exit then
        key u< if nip fmid @ 1+ swap else drop fmid @ then
    repeat
    2drop false
;

: (tcode,)
    tcp @ $1fff and $1ffe = abort" code page full"
    tcp @ tw! tcell tcp +! ;
: retract ( n -- ) \ take back the last n instructions
    0 ?do  tcp @ 2 - tcp !  0 tcp @ tw!  loop ;

\ May the n instructions before tcp be replaced, along with one emitted at tcp?
: fusable? ( n -- f )
    2* tcp @ over u< if drop false exit then
    tcp @ swap -                            ( first )
    dup tfence @ u< if drop false exit then \ a label inside the group
    tpinned @ swap tcp @ within 0=          \ a pinned instruction in the group
;

: foldable? ( insn -- f ) \ ALU, no RET, r+0, result not to R or IO
    dup h# f003 and h# 6000 =
    swap h# 0300 and dup h# 0100 <> swap h# 0200 <> and and
;

defer tcode,

: fold-exit ( insn -- insn false | true )
    dup h# 7403 = if
        1 fusable? if
            tcp @ 2 - t@ foldable? if
                drop tcp @ 2 - t@ h# 1003 or        \ the instruction with the return in it
                1 retract tcode,                    \ is offered to those before it
                true exit
            then
        then
    then
    false
;

\ `+` after a literal.  The literal is a push followed by any number of adds;
\ making the push an add turns the whole of it into T += n.
\ What kind of literal an instruction is: h# 8000 if it pushes, h# c000 if
\ it adds, and 0 if it is not one.  A shift field of 3 makes it a fetch or
\ a store of a variable, which is not a literal.
: lkind ( insn -- kind )
    dup h# 3000 and h# 3000 = if drop 0 exit then
    h# c000 and dup h# 8000 and 0= if drop 0 then
;

: fold-lit+ ( insn -- insn false | true )
    dup h# 643c <> if false exit then
    0 begin                                 ( insn n )
        1+
        dup 2* tcp @ u> if drop false exit then
        tcp @ over 2* - t@ lkind            ( insn n kind )
        dup h# c000 =
    while
        drop
    repeat
    h# 8000 <> if drop false exit then      \ not a literal at all
    dup fusable? 0= if drop false exit then
    nip
    tcp @ swap 2* - dup t@ h# 4000 or swap tw!
    true
;

\ An add of a constant after a literal of the same shift, or after another
\ such add: the two are one, if the sum fits in twelve bits.
: fold-lit-lit ( insn -- insn false | true )
    dup lkind h# c000 <> if false exit then
    1 fusable? 0= if false exit then
    tcp @ 2 - t@                                    ( insn before )
    dup lkind 0= if drop false exit then
    2dup xor h# 3000 and if drop false exit then
    2dup h# fff and swap h# fff and +               ( insn before sum )
    dup h# fff > if drop drop false exit then
    swap h# f000 and or  tcp @ 2 - tw!
    drop true
;

\ A fetch or a store straight after a literal that is the address of one of
\ the 4,096 cells from var-base: the machine has one instruction for each,
\ a literal instruction with a shift field of 3 and the cell's place in its
\ twelve bits.  Its add flag makes it the store.
variable fv-n
variable fv-a
variable fv-base    -1 fv-base !            \ var-base, once the machine file has been read
: fold-var ( insn -- insn false | true )
    dup h# 6800 <> over h# 6308 <> and if false exit then
    0 fv-a !  0 fv-n !
    begin                                   \ back over the adds to the push
        1 fv-n +!
        fv-n @ 2* tcp @ u> if false exit then
        tcp @ fv-n @ 2* - t@
        dup lkind 0= if drop false exit then
        dup h# fff and over 12 rshift 3 and 12 * lshift fv-a +!
        lkind h# 8000 =
    until
    fv-n @ fusable? 0= if false exit then
    fv-a @ fv-base @ -
    dup #4096 u< 0= if drop false exit then
    swap h# 6308 = if h# f000 else h# b000 then or
    fv-n @ retract
    (tcode,)
    true
;

: fuse-pair ( insn -- insn false | true )
    1 fusable? if
        tcp @ 2 - t@ h# e000 and h# 6000 <> if false exit then
        tcp @ 2 - t@ 16 lshift over or fuse2 #fuse2 fuse-lookup if
            nip 1 retract tcode, true exit
        then
    then
    false
;

: fuse-triple ( insn -- insn false | true )
    2 fusable? if
        tcp @ 4 - t@ 32 lshift  tcp @ 2 - t@ 16 lshift or  over or
        fuse3 #fuse3 fuse-lookup if
            nip 2 retract tcode, true exit
        then
    then
    false
;

: fuse-three ( insn -- insn false | true ) \ three that two instructions replace
    2 fusable? if
        tcp @ 4 - t@ 32 lshift  tcp @ 2 - t@ 16 lshift or  over or
        fuse32 #fuse32 fuse-lookup if
            nip 2 retract
            dup 16 rshift tcode,  h# ffff and tcode,
            true exit
        then
    then
    false
;

:noname ( insn -- )
    peephole @ if
        fold-exit   if exit then
        fold-lit+   if exit then
        fold-lit-lit if exit then
        fold-var    if exit then
        dup h# e000 and h# 6000 = if
            fuse-pair   if exit then
            fuse-triple if exit then
            fuse-three  if exit then
        then
    then
    (tcode,)
; is tcode,
: org       tcp ! label ;

wordlist constant target-wordlist
: add-order ( wid -- ) >r get-order r> swap 1+ set-order ;
: :: get-current >r target-wordlist set-current : r> set-current ;

next-arg included       \ include the machine.fs

( The machine's sizes                                                 )

\ The machine file defines cell-bits (64, 32 or 16: the width of a stack
\ cell, of the ALU, and of @ and !) and addr-bits (memory is that power of
\ two cells).  Everything here that depends on either is
\ derived from them, and the target program can test them: see W64? below.

cell-bits 8 / constant cell-bytes
var-base fv-base !
cell-bytes 2 = [if] 1 [else] cell-bytes 4 = [if] 2 [else] 3 [then] [then] constant byte-shift

\ An address on the target is the number of a cell.  The data pointer here
\ counts bytes, since bytes are laid down one at a time; taddr turns it into
\ an address for the target.
: taddr ( byte -- a ) byte-shift rshift ;

\ The display, if the machine has one, must suit the cell: see MODEL.md.
\ vm_constants.h makes the same checks when the C is built.
: one-literal? ( a -- f ) \ a 12-bit value shifted left by 12 or 24?
    dup 0= if exit then
    false swap
    #25 #12 do
        dup 1 i lshift 1- and 0=  over i rshift #4096 u< and  rot or swap
    #12 +loop
    drop
;
: display-ok ( -- )
    disp-w 0= if exit then
    disp-w cell-bits mod abort" display: the width is not a whole number of cells"
    disp-stride cell-bits * disp-w < abort" display: a row is longer than the stride"
    disp-banks 1 < abort" display: there must be at least one bank"
    disp-banks 1 > if
        disp-bank-step  disp-h disp-banks + 1- disp-banks / disp-stride *  <
        abort" display: the banks overlap"
    then
    disp-base one-literal? 0= abort" display: the base is not a single shifted literal"
;
display-ok
cell-bits #64 = [if] -1 [else] 1 cell-bits lshift 1- [then] constant cell-mask

\ Where code compiled at run time goes, in instructions: two pages from
\ $6000 on a small machine, or the eight pages from $8000 when there is
\ room.  An execution token is 16 bits in a dictionary header, less the
\ immediate flag when a cell is 16 bits, so code cannot go higher.
addr-bits #16 > [if] $8000 [else] $6000 [then] constant code-base
addr-bits #16 > [if] $8000 [else] $2000 [then] constant code-size

: tcalign   tdp @ cell-bytes 1- + cell-bytes negate and tdp ! ;

: tcell! ( x a -- ) \ store one target cell at a place given in bytes
    cell-bytes 0 do
        over i 8 * rshift over i + tc!
    loop
    2drop ;

: tvar! ( x a -- ) \ store into a target variable, given its address
    byte-shift lshift tcell! ;

( Language basics for target                 JCB 19:08 05/02/12)

warnings off
:: ( postpone ( ;
:: \ postpone \ ;

:: org          org ;
\ Each file included is measured: the code it compiled and the data it laid
\ down, with the bytes of that data that are dictionary headers and labels.
\ The figures go to the .sizes file beside the image, one line a file, a
\ file's line coming after those of the files it includes.
variable hdr-bytes  0 hdr-bytes !
variable lbl-bytes  0 lbl-bytes !
variable sizes      0 sizes !

: size-field ( n -- ) s>d <# bl hold #s #> sizes @ write-file throw ;

: size-line ( a u code data headers labels -- )
    sizes @ 0= if 2drop 2drop 2drop exit then
    base @ >r decimal
    2>r 2>r
    sizes @ write-file throw  s"  " sizes @ write-file throw
    2r> swap size-field size-field
    2r> swap size-field size-field
    s" " sizes @ write-line throw
    r> base !
;

:: include
    parse-name save-mem { a u }
    tcp @ tdp @ thp @ + hdr-bytes @ lbl-bytes @ { c d h l }
    a u included
    a u  tcp @ c -  tdp @ thp @ + d -  hdr-bytes @ h -  lbl-bytes @ l -  size-line
;
:: included     included ;
:: marker       marker ;
:: [if]         postpone [if] ;
:: [else]       postpone [else] ;
:: [then]       postpone [then] ;

\ : literal
\    \ dup $f rshift over $e rshift xor 1 and throw
\    dup h# 8000 and if
\        h# ffff xor recurse
\        ~T alu
\    else
\        h# 8000 or tcode,
\    then
\ ;

\ ===========================================================================
\ literal encoding for hexaforth
\
\       struct {
\            WORD    lit_v: 12;      //              unsigned 12-bit integer
\            BYTE    lit_shifts: 2;  // shifts:      {imm<<12,imm<<24}; 3: a variable
\            bool    lit_add: 1;     // add to T?:   {true, false}
\            bool    lit_f: 1;       // literal?:    {true, false}
\        } lit;
\
\ 16-bit instruction with 12-bit immediate values, with 2 bits for the 12-bit
\ magnitude of the value, ranging from 12 bits to 48 bits.
\
\ lit_add flag accumulates the immediate value into the prior immediate value.
\ this is an addition operation rather than a OR, as it means that we have
\ the ability to increment a value on the stack; in other words, it's a way
\ to encode +1 to +4095 in a single instruction.
\ ===========================================================================

\ A literal instruction is twelve bits, shifted left by 0, 12 or 24, that
\ are pushed or added to the top of the stack: so one push and two adds
\ make 36 bits.  A wider number is its high part, shifted left 36, with the
\ low 36 bits added.  A negative number is the number it inverts to, and an
\ invert.  (A shift field of 3 is not a literal: see fold-var.)

: chunks ( u36 push? -- ) \ lay down the chunks that are not zero, highest first
    swap
    3 0 do
        dup 2 i - 12 * rshift $fff and ?dup if
            2 i - 12 lshift or  h# 8000 or
            2 pick 0= if h# 4000 or then
            tcode,
            nip false swap
        then
    loop
    drop
    if h# 8000 tcode, then                  \ nothing was: push zero
;

: literal ( n -- )
    dup 0< if
        invert recurse
        ~T alu
        exit
    then
    dup 36 rshift ?dup if
        recurse  36 recurse  h# 60dc tcode,     \ lshift
        $fffffffff and false chunks
        exit
    then
    true chunks
;

( Defining words for target                  JCB 19:04 05/02/12)

: codeptr   tcp @ 2/ ;  \ target code pointer as a word address

( Code pages                                                          )

\ A page is 4096 words.  A branch target field with bit 12 clear is an
\ absolute kernel address; with bit 12 set it is an offset in the branch's
\ own page.  Code compiled past the kernel reaches other pages by execution
\ token, exactly as the run-time compiler does.

: tpage ( word-addr -- page ) 12 rshift ;
: >target ( word-addr -- field ) \ for a branch in the page being compiled
    dup tpage if $fff and $1000 or then ;
: here-target ( -- field ) tcp @ 2/ >target ;
: near? ( word-addr -- f ) \ kernel, or the page being compiled?
    tpage dup 0= swap tcp @ 2/ tpage = or ;


: wordstr ( "name" -- c-addr u )
    >in @ >r bl word count r> >in !
;

variable link 0 link !

\ A dictionary header is made of cells:
\
\     link    address of the previous header, or 0
\     xt      word address of the code; the top bit is set if immediate
\     name    a count byte, then the name in lower case, padded with
\             zeros to a whole number of cells
\
\ Every field is a cell at a cell-aligned address, so nothing in the
\ dictionary is fetched unaligned, and a name is compared a cell at a time.

: tcell, ( x -- ) there tcell!  cell-bytes tdp +! ;

: tname, ( c-addr u -- )
    dup tc,
    bounds ?do
        i c@ dup [char] A [char] Z 1+ within if 32 + then tc,
    loop
    tcalign
;

\ Labels.  A definition with no header still has a name in the source, and
\ on a 64-bit machine that name is kept in a second chain, laid out as the
\ dictionary is, which the prompt does not search but the decompiler does.
\ A label for code holds its execution token.  A label for something made
\ with create holds the address of its data, with the top bit set.
\
\ `named` is set by a header and is still true, with the code pointer where
\ the header left it, if the next definition is the one the header names.

variable llink      0 llink !
variable named      false named !
variable named-cp
cell-bits 64 = constant labelling

: (header) ( immediate? "name" -- )
    label
    >hdr
    there >r
    tcalign
    there taddr  link @ tcell,  link !
    tcp @ 2/ swap if 1 cell-bits 1- lshift or then tcell,
    bl parse tname,
    there r> - hdr-bytes +!
    hdr>
    true named !  tcp @ named-cp !
;

: named? ( -- f ) \ and the next definition will have to be asked again
    named @ tcp @ named-cp @ = and
    false named !
;

: wants-label? ( -- f ) named? 0= labelling and ;

: (label) ( x "name" -- a ) \ a label for the name that follows; a is where x went
    >hdr
    there >r
    tcalign
    there taddr  llink @ tcell,  llink !
    there swap tcell,
    wordstr tname,
    there r> - lbl-bytes +!
    hdr>
;

:: header       false (header) ;
:: header-imm   true (header) ;

variable wordstart
variable lastdef    \ body of the most recent target colon definition

:: :
    hex
    there s>d
    <# bl hold # # # # #>
    lst @ write-file throw
    wordstr lst @ write-line throw

    wants-label? if tcp @ 2/ (label) drop then
    tcp @ wordstart !
    label
    create  here lastdef !  codeptr , 0 ,
    does>   dup @ dup near? if
                >target scall
            else
                literal
                s" execute" target-wordlist search-wordlist 0= throw
                >body @ scall
            then
            cell+ @ if tcp @ tpinned ! then
;

\ Mark the word just defined as one that reads the instruction after each
\ call to it (through its return address).  That instruction is then left
\ exactly as written.
:: inline-operand
    true lastdef @ cell+ !
;

\ For programs that are compared instruction-for-instruction (test cases).
:: peephole-off
    false peephole !
;

:: :noname
;

\ A target data cell is cell-bytes wide: that is what `@` and `!` move.
:: ,
    tcalign
    there tcell!  cell-bytes tdp +!
;

:: c,
    tc,
;

\ Pixels are written here with the leftmost in the highest bit.  A machine
\ whose leftmost pixel is the lowest bit of a cell has them the other way
\ round in each byte.  p, lays down a byte of eight pixels and glyph, a
\ cell of eight such bytes, in the machine's order.
: pixels ( byte -- byte' )
    disp-left-low if
        0 swap 8 0 do dup 1 and rot 2* or swap 2/ loop drop
    then ;
:: p,  pixels tc, ;
\ A row of a pattern: eight pixels, repeated across a cell.
:: prow, ( byte -- )
    tcalign
    pixels cell-bytes 0 do dup tc, loop drop
;
\ 64 pixels of a bitmap, written with the leftmost highest, as the cells
\ the display would hold them in.
:: px, ( pixels -- )
    tcalign
    8 cell-bytes / 0 do                             \ a cell at a time, from the left
        cell-bytes 0 do
            dup
            disp-left-low if
                56 j cell-bytes * i + 8 * - rshift $ff and pixels
            else
                56 j cell-bytes * cell-bytes 1- i - + 8 * - rshift $ff and
            then
            tc,
        loop
    loop drop
;

\ ," text" lays down a counted string in data space.
:: ,"
    [char] " parse dup tc,
    bounds ?do i c@ tc, loop
;

:: allot
    0 ?do
        0 tc,
    loop
;

: shortcut ( orig -- f ) \ insn @orig precedes ;. Shortcut it.
    \ call becomes jump
    dup t@ h# e000 and h# 4000 = if
        dup t@ h# 1fff and over tw!
        true
    else
        \ dup t@ h# e00c and h# 6000 = if
        \    dup t@ h# 0080 or r-1 over tw!
        \    true
        \ else
            false
        \ then
    then
    nip
;

:: ;
    tcp @ wordstart @ = if
        s" exit" evaluate
    else
        tcp @ 2 - shortcut \ true if shortcut applied
        tcp @ 0 do
            i tbranches @ tcp @ = if
                i tbranches @ shortcut and
            then
        loop
        0= if   \ not all shortcuts worked
            \ tcp @ 2 - dup t@ 
            \ RET r-1 swap tw!
            s" exit" evaluate
        then
    then
;

:: ;fallthru ;

:: jmp
    ' >body @ >target ubranch
;

:: constant
    create  ,
    does>   @ literal
;

\ A header in front of create names the data.  The header points at code,
\ so code is laid down for it that pushes the data's address, as a word
\ made with create at run time has.
:: create
    named? dup >r 0= labelling and if
        0 (label)
        there taddr 1 cell-bits 1- lshift or swap tcell!
    then
    tcalign
    r> if
        there taddr literal  s" exit" evaluate  label
    then
    create there taddr ,
    does>   @ literal
;

( Switching between target and meta          JCB 19:08 05/02/12)

: target    only target-wordlist add-order definitions ;
: ]         target ;
:: meta     forth definitions ;
:: [        forth definitions ;

: t'        bl parse target-wordlist search-wordlist 0= throw >body @ ;

( eforth's way of handling constants         JCB 13:12 09/03/10)

: sign>number   ( c-addr1 u1 -- ud2 c-addr2 u2 )
    0. 2swap
    over c@ [char] - = if
        1 /string
        >number
        2swap dnegate 2swap
    else
        >number
    then
;

: base>number   ( caddr u base -- )
    base @ >r base !
    sign>number
    r> base !
    dup 0= if
        2drop drop literal
    else
        1 = swap c@ [char] . = and if
            cell-bits #64 = if
                swap literal literal        \ double: lo hi, one cell each
            else
                drop dup cell-mask and literal
                cell-bits rshift cell-mask and literal
            then
        else
            -1 abort" bad number"
        then
    then ;
warnings on

\ c# n is the distance from a cell's address to that of the nth after it.
:: c# base @ >r decimal 0. bl parse >number 2drop drop r> base ! literal ;
:: d# bl parse 10 base>number ;
:: h# bl parse 16 base>number ;
:: ['] ' >body @ literal ;  \ xt = word address

\ n at: data name  makes name the address n cells past where data is, as a
\ number compiled in line.
:: at: ( n "data" "name" -- ) ' >body @ + create , does> @ literal ;
:: [char] char literal ;

\ The machine's sizes, for the target program.  The first group compile a
\ literal; the second leave a flag for [if].
:: CELL       1 literal ;                       \ from one cell's address to the next
:: CELL2      2 literal ;                       \ offset of a header's name
:: CELLB      cell-bytes literal ;              \ bytes in a cell
:: BYTESHIFT  byte-shift literal ;              \ and the shift that multiplies by that
:: ALIGN-MASK cell-bytes negate literal ;       \ of a byte address: the start of its cell
:: OFFSET-MASK cell-bytes 1- literal ;          \ of a byte address: where in its cell

\ Text and code are reached a byte at a time, by the address of a byte:
\ the cell's address times the bytes in a cell, plus the byte's place.
\ >c makes the byte address of a cell, and c> the address of the cell a
\ byte is in.
\ These two compile it in line; the nucleus makes words of them, >c and c>.
:: (>c)       byte-shift literal s" lshift" evaluate ;
:: (c>)       byte-shift literal s" rshift" evaluate ;
:: TOPBIT     cell-bits 1- literal ;            \ index of the sign bit
:: HALFBITS   cell-bits 2/ literal ;
:: TOP16      cell-bits #16 - literal ;         \ shift that puts 16 bits at the top
\ or! ( bits a -- ) sets bits of a cell of memory, in one instruction as +!
\ is: memory, through the ALU, back to memory.
:: or!   $6b58 tcode, ;                         \ [T]->IN IN|N ->[T] d-2 alu
\ An instruction is a cell when cells are 16 bits, and otherwise a field.
:: (i@)  cell-bits #16 = if s" @" else s" 2* uw@" then evaluate ;
:: (i!)  cell-bits #16 = if s" !" else s" 2* w!" then evaluate ;
\ The display, for display.fs.  PIXR and PIXL shift pixels right and left
\ on the screen; EDGE takes a byte of eight pixels to the left end of a
\ cell, LEFT8 takes the leftmost eight of a cell as a byte, LEFT1 is the
\ leftmost pixel of a cell and M8 its leftmost eight.
\ A cell is PXCELL pixels across: pixel x of a row is PXMASK and x pixels
\ into the cell PXSHIFT rshift x along it.
\ RAW>SRC turns pixels as the bitmap has them into a source for a mode,
\ in which a set bit is black.
:: RAW>SRC        disp-ink-black 0= if s" invert" evaluate then ;
:: PXCELL         cell-bits literal ;
:: CELLBITS       cell-bits literal ;           \ bits in a cell
\ A lane is sixteen bits of a cell, and a cell is as many lanes as fit: four,
\ two or one.  LANE1 is a cell with 1 in each of its lanes.
:: LANE1          0 cell-bits 16 / 0 do 16 lshift 1 or loop literal ;
:: PXMASK         cell-bits 1- literal ;
:: PXSHIFT        byte-shift 3 + literal ;
:: DISP-BASE      disp-base literal ;
:: DISP-STRIDE    disp-stride literal ;
:: DISP-BANKS     disp-banks literal ;
:: DISP-BANK-STEP disp-bank-step literal ;
:: BANKED?    disp-banks 1 > ;
:: INK-BLACK? disp-ink-black 0<> ;
:: LEFT-LOW?  disp-left-low 0<> ;
:: PIXR   disp-left-low if s" lshift" else s" rshift" then evaluate ;
:: PIXL   disp-left-low if s" rshift" else s" lshift" then evaluate ;
:: EDGE   disp-left-low 0= if cell-bits 8 - literal s" lshift" evaluate then ;
:: LEFT8  disp-left-low if $ff literal s" and" evaluate
          else cell-bits 8 - literal s" rshift" evaluate then ;
:: LEFT1  disp-left-low if 1 else 1 cell-bits 1- lshift then literal ;
:: M8     disp-left-low if $ff else $ff cell-bits 8 - lshift then literal ;
:: VARS       var-base literal ;                \ the first cell one instruction can fetch
:: HDR-BASE   hdr-base literal ;                \ where headers start, and data must stop
:: CODE-BASE  code-base literal ;
:: CODE-SIZE  code-size literal ;
:: W64?   cell-bits #64 = ;
:: W32?   cell-bits #32 = ;
:: W32+?  cell-bits #32 >= ;

:: asm-0branch
    ' >body @
    0branch
;

( Conditionals                               JCB 13:12 09/03/10)

: resolve ( orig -- )
    label
    tcp @ over tbranches ! \ forward reference from orig to this loc
    dup t@ here-target or swap tw!
;

:: if
    tcp @
    0 0branch
;

:: DOUBLE
    tcp @ 2 + 2/ >target scall
;

:: then
    resolve
;

:: else
    tcp @
    0 ubranch 
    swap resolve
;

:: begin label tcp @ ;

:: again ( dest -- )
    2/ >target ubranch
;
:: until
    2/ >target 0branch
;
:: while
    tcp @
    0 0branch
;
:: repeat
    swap 2/ >target ubranch
    resolve
;

4 org
: .trim ( a-addr u ) \ shorten string until it ends with '.'
    begin
        2dup + 1- c@ [char] . <>
    while
        1-
    repeat
;

( Strings                                    JCB 11:57 05/18/12)

: >str ( c-addr u -- str ) \ a new u char string from c-addr
    dup cell+ allocate throw dup >r
    2dup ! cell+    \ write size into first cell
                    ( c-addr u saddr )
    swap cmove r>
;
: str@  dup cell+ swap @ ;
: str! ( str c-addr -- c-addr' ) \ copy str to c-addr
    >r str@ r>
    2dup + >r swap
    cmove r>
;
: +str ( str2 str1 -- str3 )
    over @ over @ + cell+ allocate throw >r
    over @ over @ + r@ !
    r@ cell+ str! str! drop r>
;

: example
    s"  sailor" >str
    s" hello" >str
    +str str@ type
;

next-arg 2dup .trim >str constant prefix.
: .suffix  ( c-addr u -- c-addr u ) \ e.g. "bar" -> "foo.bar"
    >str prefix. +str str@
;
: create-output-file w/o create-file throw ;
: out-suffix ( s -- h ) \ Create an output file h with suffix s
    >str
    prefix. +str
    s" ../build/" >str +str str@
    create-output-file
;
:noname
    s" lst" out-suffix lst !
    s" sizes" out-suffix sizes !
; execute


target included                         \ include the program.fs

[ tcp @ 0 org ]
\ bootloader 
main        \ address 0
quit        \ address 2
[ org ]
meta

decimal
0 value file
: dumpall.16
    s" hex" out-suffix to file

    hex
    1024 0 do
        tflash i 2* + w@
        s>d <# # # # # #> file write-line throw
    loop
    file close-file
;
: dumpall.32
    s" hex" out-suffix to file

    hex
    32768 0 do
        tflash i 4 * + ul@
        s>d <# # # # # # # # # #> file write-line throw
    loop
    file close-file
;

s" (all)" tcp @  tdp @ $4000 -  thp @ hdr-base - +  hdr-bytes @ lbl-bytes @ size-line
tdp @ hdr-base > [if] cr .( the data has run into the dictionary headers) abort [then]
sizes @ close-file throw
dumpall.32
." tdp " tdp @ . ." thp " thp @ .
." tcp " tcp @ .

bye
