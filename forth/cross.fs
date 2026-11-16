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

variable tdp                    \ Data pointer: set where the data starts (data-base, below)

\ Dictionary headers and labels are laid down in a region of their own,
\ above the data, so that they do not push variables up: a variable in the
\ 4,096 cells from var-base is fetched and stored by one instruction.  thp
\ is where the next header goes.  At run time the nucleus does the same:
\ its data carries on from the end of the data here, and its headers from
\ the end of the headers.  Where the region starts depends on the machine
\ (hdr-base, below, once the machine's sizes have been read).
variable thp
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
\   anything  that with the one or the two before it does what one
\             instruction does is replaced by that one.  The result is
\             offered again, so fusions cascade.  Three that are not one but
\             do what two do are replaced by the two, each offered again.
\             Which instruction, or which two, the machine is asked
\             (machine-fuse, machine-two): util/fuse.c has the machine's
\             step and says what leaves the same.
\   n +       after a literal or another n + of the same shift is added to
\             it, if the sum still fits: `1+ 1+` is one instruction.
\   @ and !   after the address of a variable are one instruction with it.
\   a run     that has numbers pushed or added in it, or variables fetched,
\             is given to the machine too (machine-lit), which gives a
\             shorter run that does the same if it finds one.
\
\ Two things stop a fusion:
\   tfence   the most recent address a branch or call can land on.  The
\            instruction there may start a fused group but not sit inside one.
\   tpinned  an instruction that the word called just before it will read
\            and copy (see inline-operand), so it must stay as written.

variable tfence     0 tfence !
variable tpinned    -1 tpinned !
variable peephole   true peephole !
variable plain      false plain !               \ true for a build with PEEPHOLE=off
s" PEEPHOLE" getenv s" off" compare 0= [if] false peephole !  true plain ! [then]   \ PEEPHOLE=off gforth ...

\ A small word is tried in place of a call to it.  Its instructions are
\ compiled where the call would be, each offered to the peephole as any
\ other is; a call hides from the peephole what the word does, and
\ `a b - 0>` as two calls is two calls.  If the peephole joins any of them
\ to what is before, or joins the instruction that comes next to them,
\ they stay.  If it does nothing with them they are taken out again and
\ the word is called: `pend` is such a word waiting for the instruction
\ that comes next, and `settle` takes it out without waiting, for a place
\ a branch lands on.  A push of a number is not what comes next but part
\ of it (`- 100 +`), so the word waits past up to three of those, and they
\ are compiled again after the call if it comes to that.  A word of one instruction or none stays whatever
\ happens, being no longer than the call.  Any other stays only if what
\ is left of it, and of the pushes it waited past, is no longer than the
\ call and they would be; where the word being compiled ends, the call
\ would be a jump and the exit gone, and it is that it must not be longer
\ than.
\
\ That is what is smallest, and a call and its return are not free: where
\ a place is run so often that they cost, `in-place` before the call says
\ that the word is compiled there whatever is joined.  Measure before
\ saying it: how often the place is run, and what the work costs with it
\ and without.
\
\ Every call compiled has a number, and inl-map says which were compiled
\ in place.  A build with the peephole off cannot tell, so it reads the
\ map the build with it on wrote (build/<name>.inline) and does the same,
\ so that the two builds have the same calls and can be compared.

create inl-map 65536 allot  inl-map 65536 erase
variable call#      0 call# !
variable pend       false pend !
variable pend-body
variable pend-tcp0
variable pend-ord
create pend-save 12 cells allot
variable pend-len                               \ the bytes it was compiled as,
create pend-code 8 cells allot                  \ and the instructions
variable pend-xn    0 pend-xn !                 \ pushes compiled since
create pend-x 3 cells allot
variable 'ptcode                                \ compiles an instruction, by way of the peephole
variable cur        false cur !                 \ true while a small word is being compiled in place
variable stays      false stays !               \ true from `in-place` to the call it is said of
variable ending     false ending !              \ true while the exit that ends a word is compiled
variable cur-tcp0
create cur-save 12 cells allot
defer call,     ( body -- )                     \ compile a call to a word
defer tcode,

: snap ( tcp0 buf -- ) \ keep the twelve instructions before tcp0
    12 0 do
        over 24 - i 2* +  dup 0< if drop 0 else t@ then
        over i cells + !
    loop
    2drop
;

: unsnap ( tcp0 buf -- )
    12 0 do
        over 24 - i 2* +  dup 0< if drop else over i cells + @ swap tw! then
    loop
    2drop
;

: (backout) ( -- ) \ take the word waiting out again, and call it
    tcp @ pend-tcp0 @ > if
        tcp @ pend-tcp0 @ do 0 i tw! 2 +loop
    then
    pend-tcp0 @ tcp !
    pend-tcp0 @ pend-save unsnap
    pend-body @ call,
    pend-xn @ 0 ?do pend-x i cells + @ 'ptcode @ execute loop
    0 pend-xn !
;

: unchanged? ( -- f ) \ is the word waiting still as it was compiled?
    tcp @  pend-tcp0 @ pend-len @ +  u< if false exit then
    pend-len @ 2/ 0 ?do
        pend-tcp0 @ i 2* + t@  pend-code i cells + @ <> if false unloop exit then
    loop
    true
;

: settle ( -- ) pend @ if false pend !  (backout) then ;

: label     settle  tcp @ tfence ! ;

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
\ twelve bits.  Its add flag makes it the store.  The literal is taken back
\ and the instruction being compiled is then that one: it is not compiled
\ here, so that it is offered to what comes after as any other is.
\
\ This is tried before the machine is asked about a run (fuse-lit),
\ whether the address is one instruction or a push and an add: what the
\ machine gives for the run before it would keep the address, and `r> blk
\ !` would be three instructions where it is two.  A variable's
\ instruction cannot end a word, where an instruction of the ALU that
\ fetches or stores can have the exit folded into it: so the machine is
\ asked again with the variable's instruction in the run, and at an exit.
variable fv-n
variable fv-a
variable fv-base    -1 fv-base !            \ var-base, once the machine file has been read
: fold-var ( insn -- insn' false )
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
    false
;

\ Which instruction does the work of two or of three (the first is nothing
\ for two), and which two do the work of three, the first of them in the
\ upper sixteen bits: nothing, if none is known to.  The machine is asked,
\ once its sizes are known (below).
defer machine-fuse ( first second third -- one | 0 )
defer machine-two ( first second third -- two | 0 )
\ And what shorter run does the work of a run of instructions that has
\ numbers in it: the run and what is given are instructions of 16 bits one
\ after another, at an address here.  How many are given, or less than
\ nothing.  Before a conditional branch (branch? is 1) what is given leaves
\ the same but for the top, which is nothing when the run's is.
defer machine-lit ( run n branch? out -- m | -1 )

: fuse-pair ( insn -- insn false | true )
    1 fusable? if
        tcp @ 2 - t@ h# e000 and h# 6000 <> if false exit then
        0  tcp @ 2 - t@  2 pick machine-fuse  ?dup if
            nip 1 retract tcode, true exit
        then
    then
    false
;

: fuse-triple ( insn -- insn false | true )
    2 fusable? if
        tcp @ 4 - t@  tcp @ 2 - t@  2 pick machine-fuse  ?dup if
            nip 2 retract tcode, true exit
        then
    then
    false
;

: fuse-three ( insn -- insn false | true ) \ three that two instructions replace
    2 fusable? if
        tcp @ 4 - t@  tcp @ 2 - t@  2 pick machine-two  ?dup if
            nip 2 retract
            dup 16 rshift tcode,  h# ffff and tcode,
            true exit
        then
    then
    false
;

defer fuse-lit ( insn -- insn false | true )

:noname ( insn -- )
    peephole @ if
        fold-exit   if exit then
        fold-var    if exit then                \ (it never is: the instruction goes on, as a variable's)
        fuse-lit    if exit then                \ after a variable's instruction has been made,
        fold-lit+   if exit then                \ and before a number is folded into one before it
        fold-lit-lit if exit then
        dup h# e000 and h# 6000 = if
            fuse-pair   if exit then
            fuse-triple if exit then
            fuse-three  if exit then
        then
    then
    (tcode,)
; constant ptcode,                              \ an instruction, by way of the peephole
ptcode, 'ptcode !

: kernel? ( -- f ) tcp @ 2/ h# 1000 u< ;

:noname ( insn -- )
    pend @ 0= if ptcode, execute exit then
    dup lkind h# 8000 =  kernel? 0= and  cur @ 0= and  pend-xn @ 3 < and if
        dup pend-x pend-xn @ cells + !  1 pend-xn +!        \ a push: wait for what uses it
        false pend !  ptcode, execute  true pend !
        exit
    then
    false pend !
    dup ptcode, execute
    unchanged?                                  \ nothing joined it, or what is left is longer
    tcp @ pend-tcp0 @ -  pend-xn @ 2*  ending @ if 2 else 4 then +  >  or   \ than the call, the pushes and it
    if
        (backout)
        cur @ if tcp @ cur-tcp0 !  tcp @ cur-save snap then
        ptcode, execute
    else
        drop
        0 pend-xn !
        1 inl-map pend-ord @ + c!
    then
; is tcode,
: org       settle tcp ! label ;

\ Code past the kernel is in pages, and the program says where each starts
\ (code-page).  A build with the peephole off is never run: it is made on
\ the host to be compared with the real one, and is about a third bigger.
\ Nothing about the real image is arranged for it.  It takes no notice of
\ where the program starts its pages, and starts one of its own before any
\ definition that has less than a quarter of a page left to go in.  The
\ order of the code is the same in both, which is all the comparison
\ needs; a call that is across pages in one and not in the other is the
\ same call.
variable page-min   \ the first page past page 1, in bytes: code-base, once the machine's sizes are read
: next-page ( -- ) tcp @ $1fff + $2000 negate and  page-min @ max  org ;
: code-page ( n -- ) plain @ if drop else $2000 * org then ;
: ?own-page ( -- ) \ before a definition
    plain @  tcp @ $2000 u< 0= and  tcp @ $1fff and $1800 u> and
    if next-page then
;

wordlist constant target-wordlist
: add-order ( wid -- ) >r get-order r> swap 1+ set-order ;

\ A macro and a word of the program are in the one wordlist, and a name has
\ one case, here as on the machine: a word named line takes the name of the
\ macro LINE from every file after it, which goes on compiling with the
\ word where it wrote the macro.  So the macros are kept in a list, and
\ after each file every one of them is looked up again.  The basewords are
\ not in the list: the nucleus gives each that it needs a body, under the
\ baseword's own name, so that it can be called.
variable macros  0 macros !             \ each is a link, its xt, and its name with its length before it
variable listing  false listing !       \ true while what is defined is put in the list

: macro, ( "name" -- a ) \ a place in the list for the macro that is about to be named
    >in @  parse-name  rot >in !                ( c-addr u )
    align here >r  macros @ ,  0 ,  dup ,  here swap dup allot move
    r> dup macros !
;
: :: ( "name" -- )
    listing @ if macro, else 0 then >r
    get-current >r target-wordlist set-current : r> set-current
    r> ?dup if latestxt swap cell+ ! then
;
: macro? ( xt -- f )
    macros @ begin dup while 2dup cell+ @ = if 2drop true exit then @ repeat nip
;
: macro-name ( a -- c-addr u ) 2 cells + dup cell+ swap @ ;
: ?macros ( c-addr u -- ) \ after the file named: each macro's name still finds a macro
    macros @ begin dup while
        dup macro-name target-wordlist search-wordlist
        0= abort" a macro's name finds nothing"
        macro? 0= if
            cr macro-name type ."  is a macro, and " type ."  has named a word of its own that" abort
        then
        @
    repeat
    drop 2drop
;
: included-whole ( c-addr u -- ) 2dup 2>r included 2r> ?macros ;

next-arg included       \ include the machine.fs
true listing !

( The machine's sizes                                                 )

\ The machine file defines cell-bits (64, 32 or 16: the width of a stack
\ cell, of the ALU, and of @ and !) and addr-bits (memory is that power of
\ two cells).  Everything here that depends on either is
\ derived from them, and the target program can test them: see W64? below.

cell-bits 8 / constant cell-bytes
var-base fv-base !

\ The machine, for the peephole to ask: util/fuse.c, which has vm.c in it,
\ built here for this cell and called through gforth's C interface.  It is
\ built each time, so it is never the machine as it was.  gforth builds it
\ where it keeps such things, so it is told where the file is in full.
: here-is ( -- c-addr u ) \ the directory this is run in
    s" pwd" r/o open-pipe throw >r
    pad 512 r@ read-line throw drop
    r> close-pipe throw drop
    pad swap
;
s" #define CELL_BITS " cell-bits 0 <# #s #> s+ save-c-prefix-line
s" #include " s\" \"" s+  here-is s+  s\" /../util/fuse.c\"" s+ save-c-prefix-line
s" m" add-lib
c-function (machine-fuse) fuse_one n n n -- n
c-function (machine-two) fuse_two n n n -- n
c-function (machine-lit) fuse_lit a n n a -- n
' (machine-fuse) is machine-fuse
' (machine-two) is machine-two
' (machine-lit) is machine-lit
cell-bytes 2 = [if] 1 [else] cell-bytes 4 = [if] 2 [else] 3 [then] [then] constant byte-shift
var-base byte-shift lshift constant data-base       \ the byte of the image the data starts at

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

\ Where things are, after the kernel's page and page 1, which every machine
\ has, and the data, which starts at byte $4000 (var-base):
\
\                             headers          code pages past page 1
\   memory of 64K cells       bytes $6000      4, instructions $4000-$7fff
\   more                      bytes $a000      8, instructions $8000-$ffff
\
\ The data ends where the headers start, and the headers where that code
\ starts.  An execution token is 16 bits in a dictionary header, less the
\ immediate flag when a cell is 16 bits, so with such a cell code cannot be
\ at instruction $8000 or above.  What the system does not use of those
\ pages is for code compiled at run time (code-base, code-size, in
\ instructions).
addr-bits #16 > 0= constant small           \ a machine whose memory is 64K cells
\ The headers start at byte $a000, which with cells of 64 bits is inside
\ the 4,096 cells that one instruction reaches.  With cells of 32 bits
\ those end at byte $8000 and what is bulk comes after them (below): it
\ has 16KB and the headers the 16KB after that.
small [if] $6000 [else] cell-bits #64 < [if] $c000 [else] $a000 [then] [then] constant hdr-base
hdr-base thp !
\ The first cell of the data, the cell at var-base, is every image's: the
\ address of its newest header, stored when the program has been compiled
\ (at the end of this file).  It is where whoever reads an image finds its
\ dictionary (read_header in vm_opcodes.c), and a program that keeps a
\ dictionary keeps its newest word there: `forth`, below.
data-base cell-bytes + tdp !
small [if] $4000 [else] $8000 [then] constant code-base
small [if] $4000 [else] $8000 [then] constant code-size
code-base 2* page-min !

\ One instruction fetches or stores a cell in the 4,096 from var-base, and
\ the data is there so that a variable is such a cell.  Where a cell is
\ less than 64 bits those are fewer bytes than the data is: a font, a
\ table, a buffer of text would push the variables that come after them
\ out.  So what is bulk and not variables says so (`bulk`, and `unbulk`
\ after it), and is laid down in a place of its own after them: past the
\ 4,096 cells, or on the small machine, where they are all the data there
\ is room for, in the last of them, from byte $5400.  The data made at run
\ time carries on after the variables, and must stop where the bulk
\ starts.  On a machine whose 4,096 cells reach the headers there is
\ nothing to do and the two words do nothing.
small [if] $5400 [else] data-base 4096 cell-bytes * + [then] constant bulk-base
bulk-base hdr-base < constant bulky
variable tbp    bulk-base tbp !
variable bsave
variable in-bulk    false in-bulk !
bulky [if] bulk-base [else] hdr-base [then] constant data-limit

\ The small machine has more above its display than below it, and what is
\ only ever reached a cell at a time need not be below: a table of cells,
\ a bitmap.  Such data says so (`high`, and `unhigh` after it) and on that
\ machine is laid down from where the display ends; what is given out at
\ run time (halloc) starts after it.  On any other machine it is bulk.
: display-end ( -- cell ) \ where the last bank's last row ends: an even cell
    disp-bank-step disp-banks 1- *
    disp-h disp-banks + 1- disp-banks / disp-stride * +
    disp-base +  1+ -2 and
;
small disp-w 0<> and constant lofty
variable thi    display-end cell-bytes * thi !
variable hsave

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
\ down, with the bytes of that data that are dictionary headers and labels,
\ and the instruction its code starts at.  The figures go to the .sizes
\ file beside the image, one line a file, a file's line coming after those
\ of the files it includes, whose code and data are part of its own.
variable hdr-bytes  0 hdr-bytes !
variable lbl-bytes  0 lbl-bytes !
variable sizes      0 sizes !

: size-field ( n -- ) s>d <# bl hold #s #> sizes @ write-file throw ;

: size-line ( a u code data headers labels start -- )
    sizes @ 0= if drop 2drop 2drop 2drop exit then
    base @ >r decimal
    >r 2>r 2>r
    sizes @ write-file throw  s"  " sizes @ write-file throw
    2r> swap size-field size-field
    2r> swap size-field size-field
    r> size-field
    s" " sizes @ write-line throw
    r> base !
;

:: include
    parse-name save-mem { a u }
    tcp @ tdp @ thp @ + tbp @ + hdr-bytes @ lbl-bytes @ { c d h l }
    a u included-whole
    a u  tcp @ c -  tdp @ thp @ + tbp @ + d -  hdr-bytes @ h -  lbl-bytes @ l -  c 2/  size-line
;

:: bulk ( -- ) \ what is laid down from here is not variables
    in-bulk @ abort" bulk within bulk"  true in-bulk !
    bulky if tdp @ bsave !  tbp @ tdp ! then
;
\ The bulk is looked at as soon as it is laid down: what runs past it would
\ be laid over the headers, and the next thing to follow them would fail
\ with nothing to say why.
: bulk-fits ( -- )
    bulky if
        tbp @ hdr-base > if
            cr ." the bulk has run into the dictionary headers, by " tbp @ hdr-base - . ." bytes" abort
        then
    then
;
:: unbulk ( -- )
    in-bulk @ 0= abort" unbulk with no bulk"  false in-bulk !
    bulky if tdp @ tbp !  bsave @ tdp ! then
    bulk-fits
;
:: high ( -- ) \ what is laid down from here is reached only a cell at a time
    in-bulk @ abort" high within bulk"  true in-bulk !
    lofty if tdp @ hsave !  thi @ tdp ! else bulky if tdp @ bsave !  tbp @ tdp ! then then
;
:: unhigh ( -- )
    in-bulk @ 0= abort" unhigh with no high"  false in-bulk !
    lofty if tdp @ thi !  hsave @ tdp ! else bulky if tdp @ tbp !  bsave @ tdp ! then then
    bulk-fits
;
:: included     included-whole ;
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

\ ---- the peephole and literals
\
\ What the machine joins by twos and threes is ALU instructions next to
\ each other.  A number between them, or before them, stops that: so a run
\ that has one in it is given to the machine whole (machine-lit).
\ There the code is tokens: an ALU instruction; a push of a number, which
\ is a push instruction, the adds after it, and the invert that makes it
\ negative; an add of a number to the top of the stack, which is add
\ instructions; and the fetch of a variable, which is the one instruction
\ that does it.  A run is up to four tokens and the one being compiled,
\ and it is asked about with four before, then three, and so on, so that
\ it starts where a token does.  What the machine gives is instructions,
\ fewer than the run has, and they are compiled in its place.
\
\ The one being compiled may be a conditional branch.  It asks only whether
\ the top of the stack is zero, so what is before it may be replaced by
\ anything that leaves the same answer: `<> if` is `xor if`.  The branch
\ itself stays.
\
\ A push is first put in the shortest form it has: `16 -` leaves a push of
\ 16, an invert and an add of 1, which is a push of -16, and that is a
\ push of 15 and an invert.

: lval ( insn -- n ) dup h# fff and swap 12 rshift 3 and 12 * lshift ;
: alu-tok? ( insn -- f ) dup h# e000 and h# 6000 =  swap h# 1000 and 0=  and ;

: chunks# ( u -- n ) \ the instructions an add of u is
    0 swap  3 0 do dup h# fff and if swap 1+ swap then 12 rshift loop  drop ;
: litcost ( n -- cost ) dup 0< if invert recurse 1+ exit then  chunks# 1 max ;

: adds ( q -- q' sum ) \ the add instructions that end at q
    0 swap
    begin
        dup 0> if dup 2 - t@ lkind h# c000 = else false then
    while
        2 -  dup t@ lval rot + swap
    repeat
    swap
;

: push? ( q -- f ) dup 0> if 2 - t@ lkind h# 8000 = else drop false then ;

\ The token that ends at p: where it starts, its kind (0 none, 1 an ALU
\ instruction, 2 a push, 3 an add, 4 the fetch of a variable or the store
\ to one) and its instruction or value.
: tok ( p -- p' kind val )
    dup >r adds                                     ( q sum )
    over push? if
        swap 2 - tuck t@ lval +  2 swap  rdrop exit
    then
    over 2 >= if over 2 - t@ h# 6480 = else false then if
        over 2 - adds                               ( q sum q2 sum2 )
        over push? if                               \ a push, an invert, adds: a negative number
            swap 2 - tuck t@ lval +  invert         ( q sum q3 v )
            rot + rot drop  2 swap  rdrop exit
        then
        2drop
    then
    over r@ <> if 3 swap  rdrop exit then           \ adds alone
    drop rdrop
    dup 2 >= if dup 2 - t@ h# b000 and h# b000 = else false then if     \ a literal with a shift field of 3
        2 -  dup t@  4 swap exit
    then
    dup 2 >= if dup 2 - t@ alu-tok? else false then if
        2 -  dup t@  1 swap exit
    then
    0 0
;

create tk-at   4 cells allot
create tk-kind 4 cells allot
create tk-val  4 cells allot
variable tk-n
: toks ( -- ) \ the tokens before tcp, nearest first, up to four
    0 tk-n !  tcp @
    begin
        tk-n @ 4 <
    while
        tok  over 0= if 2drop drop exit then
        tk-n @ cells >r
        r@ tk-val + !  r@ tk-kind + !  dup r> tk-at + !
        1 tk-n +!
    repeat
    drop
;

variable fl-n       \ how many tokens of the run are before the one being compiled
variable fl-trig    \ that one: 0 an instruction of the ALU, 1 an add, 2 a conditional branch,
                    \ 3 an instruction of a variable
variable fl-insn    \ and the instruction

#32 constant fl-room                            \ the most instructions in a run that is asked about
create fl-run fl-room 2* allot                  \ the run, as the machine is given it,
create fl-out fl-room 2* allot                  \ and what the machine gives for it
variable fl-run#

: fl-take ( -- ) \ the fl-n tokens before, and the one being compiled unless it is a branch
    0 fl-run# !
    tcp @  tk-at fl-n @ 1- cells + @  ?do
        i t@  fl-run fl-run# @ 2* + w!  1 fl-run# +!
    2 +loop
    fl-trig @ 2 <> if  fl-insn @  fl-run fl-run# @ 2* + w!  1 fl-run# +!  then
;

\ Instructions of the ALU and nothing else, with one more being compiled,
\ are the machine's to join by twos and threes: they are not asked about
\ here.
: fl-plain? ( -- f )
    fl-trig @ if false exit then
    fl-n @ 0 do  tk-kind i cells + @ 1 <> if false unloop exit then  loop
    true
;

\ Compiling what the machine gives offers each instruction to the peephole,
\ which may come back here: so they are taken out of fl-out first.
: fl-try ( -- f ) \ what the machine gives for the fl-n tokens and the one being compiled, if anything
    fl-plain? if false exit then
    tcp @  tk-at fl-n @ 1- cells + @  -  2/         ( instructions before )
    dup fusable? 0=  over fl-room 2 - > or  if drop false exit then
    fl-take
    fl-run fl-run# @  fl-trig @ 2 = 1 and  fl-out  machine-lit     ( before m )
    dup 0< if 2drop false exit then
    swap retract
    dup 0 ?do  dup 1- i - 2* fl-out + uw@  swap  loop       ( the last of them ... the first m )
    0 ?do tcode, loop
    true
;

:noname ( insn -- insn false | true )
    dup fl-insn !
    dup h# e000 and h# 6000 = if 0 fl-trig ! else   \ of the ALU: an exit too, which no fetch or store
                                                    \ of a variable can have folded into it
    dup h# b000 and h# b000 = if 3 fl-trig ! else   \ the fetch of a variable, or the store to one
    dup lkind h# c000 = if 1 fl-trig ! else
    dup h# e000 and h# 2000 = if 2 fl-trig ! else
    false exit then then then then
    toks
    tk-n @ 0= if false exit then
    tk-kind @ 2 = if                                \ a push that has a shorter form
        tcp @ tk-at @ - 2/  dup tk-val @ litcost >  over fusable? and if
            retract  tk-val @ literal
            tcode,  true exit
        then
        drop
    then
    tk-n @ begin
        dup
    while
        dup fl-n !
        fl-try if
            drop
            dup h# e000 and h# 2000 = if tcode, else drop then  \ a branch is still to be compiled
            true exit
        then
        1-
    repeat
    drop false
; is fuse-lit

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
\     name    its characters, five bits each, as many to a cell as a
\             cell holds; the top bit of its last cell is set
\
\ Every field is a cell at a cell-aligned address, so nothing in the
\ dictionary is fetched unaligned, and a name is compared a cell at a time.
\
\ A name is in lower case, so a character is five bits: nothing, the 26
\ letters, and - ! ? which are the commonest of the rest; then two codes
\ that say the next code is one of 31 others, or one of 8 more.  Those
\ are every character that can be typed in a name.  A cell is 3, 6 or 12
\ codes, the first the lowest, with a bit or more over: the top one says
\ the name ends in this cell, so nothing counts its characters, and a
\ name that is the start of another is not the same cells.
\
\ No code of a name is nothing: which of the others a character is, is
\ counted from one.  So what is left of a cell when the codes of the name
\ have been taken from it is nothing, and whoever reads a name stops
\ there and does not look at the cell's padding.

\
\ Which characters those are is not said here.  It is in vm_opcodes.h
\ (NAME_CODES_REPR), where the C that reads a header has it too, and the
\ machine file brings it: n-direct, the three with codes 27 to 29;
\ n-page1, what follows code 30; n-page2, what follows code 31; and how
\ many each is (n-directs, n-page1s, n-page2s).

: tcell, ( x -- ) there tcell!  cell-bytes tdp +! ;

cell-bits 5 / constant name-per             \ codes to a cell
name-per 5 * constant name-bits             \ and the bits they are

\ What a character is in a name: its code if it has one, or 32 and where it
\ is among the first of the rest, or 64 and where among the others.  What
\ cannot be in a name is ?.
: n-code ( c -- v )
    dup [char] A [char] Z 1+ within if 32 + then
    dup [char] a [char] z 1+ within if [char] a - 1+ exit then
    n-directs 0 do dup n-direct i + c@ = if drop i 27 + unloop exit then loop
    n-page1s  0 do dup n-page1  i + c@ = if drop i 33 + unloop exit then loop
    n-page2s  0 do dup n-page2  i + c@ = if drop i 65 + unloop exit then loop
    drop 29
;

create nk 128 cells allot                   \ a name, packed: its cells,
variable nk#                                \ how many,
variable nksh                               \ and how far along the cell in hand the next code goes

: nk-code ( code -- )
    nksh @ lshift  nk nk# @ cells + +!
    5 nksh +!
    nksh @ name-bits = if  0 nksh !  1 nk# +!  0 nk nk# @ cells + !  then
;

: name-pack ( c-addr u -- ) \ into nk
    63 min
    0 nk# !  0 nksh !  0 nk !
    bounds ?do
        i c@ n-code
        dup 31 > if  dup 5 rshift 29 + nk-code  31 and  then
        nk-code
    loop
    nksh @ 0=  nk# @ 0<> and if -1 nk# +! then     \ the last code filled its cell: that one is the last
    1 cell-bits 1- lshift  nk nk# @ cells + +!
    1 nk# +!
;

: tname, ( c-addr u -- ) name-pack  nk# @ 0 ?do nk i cells + @ tcell, loop ;

\ The two tables the machine has of this, a character each: what each of
\ the 128 characters is in a name (n-code), and what each code is, the
\ plain ones and then the two lots of the rest.
: codes-of-chars, ( -- ) 128 0 do i 33 < i 127 = or if 29 else i n-code then tc, loop ;
: chars-of-codes, ( -- )
    0 tc,  [char] z 1+ [char] a do i tc, loop
    n-directs 0 do n-direct i + c@ tc, loop  0 tc, 0 tc,
    0 tc,  n-page1s 0 do n-page1 i + c@ tc, loop
    0 tc,  n-page2s 0 do n-page2 i + c@ tc, loop  31 n-page2s - 0 do 0 tc, loop
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
true constant labelling                     \ every machine has them: see name-tables

\ Where each header is, by name, so that data can refer to a word by its
\ header (`hdr,`): the name is then in one place, and nothing has to look
\ it up when the program runs.
wordlist constant headers-wordlist
: note-header ( a "name" -- ) \ the name is looked at, and left to be read
    wordstr nextname
    get-current >r  headers-wordlist set-current  create ,  r> set-current
;

: (header) ( immediate? "name" -- )
    ?own-page
    label
    >hdr
    there >r
    tcalign
    there taddr  link @ tcell,  link !
    link @ note-header
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

\ The labels are not in the image.  They are laid down here, as headers
\ are, and go to the file of names for the disk (name-tables): a label is
\ the place of the one before, its value, and its name.  A place is how
\ many cells it is from the start of them all, where the first cell holds
\ the place of the newest; the decompiler makes addresses of them when it
\ has read them in.  The cells are the machine's, as a header's are.
65536 allocate throw constant lflash
lflash 65536 erase
variable lp  cell-bytes lp !                \ where the next goes, in bytes

: lc,    ( c -- ) lflash lp @ + c!  1 lp +! ;
: lcell, ( x -- ) cell-bytes 0 do dup $ff and lc, 8 rshift loop drop ;
: lname, ( c-addr u -- ) name-pack  nk# @ 0 ?do nk i cells + @ lcell, loop ;

: (label) ( x "name" -- ) \ a label for the name that follows
    lp @ byte-shift rshift  llink @ lcell,  llink !
    lcell,
    wordstr lname,
;

:: header       false (header) ;
:: header-imm   true (header) ;

variable wordstart
variable lastdef    \ body of the most recent target colon definition
variable indef      \ true from : to ; (and not from :noname)

\ A word is small (see `pend`, above) if it is no more than four
\ instructions, none a call or a branch, returns only at its end, and
\ leaves the return stack alone below what it pushed itself, so that it
\ does not look at its return address.

4 constant inline-max

variable in-n
variable in-depth
: small? ( -- m+1 | 0 ) \ the word just ended: instructions to compile in place, and 1
    tcp @ wordstart @ - 2/ in-n !
    in-n @ 1 inline-max 2 + within 0= if 0 exit then
    0 in-depth !
    in-n @ 0 do
        wordstart @ i 2* + t@                       ( insn )
        dup h# 8000 and 0= if
            dup h# e000 and h# 6000 <> if drop 0 unloop exit then       \ a call or a branch
            dup h# 1000 and  i in-n @ 1- <>  and if drop 0 unloop exit then  \ returns early
            dup h# 0c00 and h# 0c00 =  in-depth @ 0=  and if drop 0 unloop exit then
            dup h# 1000 and 0= if
                dup 3 and  dup 1 = if 1 in-depth +! then
                dup 3 = if -1 in-depth +! then
                2 = if -2 in-depth +! then
                in-depth @ 0< if drop 0 unloop exit then
            then
        then
        drop
    loop
    in-depth @ if 0 exit then
    wordstart @ in-n @ 1- 2* + t@                   ( the last )
    dup h# 9000 and h# 1000 <> if drop 0 exit then  \ it must be what returns
    h# 7403 = if in-n @ 1- else in-n @ then
    dup inline-max > if drop 0 exit then
    1+
;

: inline, ( body -- )
    dup 2 cells + @ 1-  swap @ 2*                   ( m addr )
    swap 0 ?do
        dup i 2* + t@
        dup h# 9000 and h# 1000 = if h# 1003 invert and then     \ without its return
        tcode,
    loop
    drop
;

: try-inline ( body ord -- )
    >r
    dup 2 cells + @ 1-                          ( body m )
    dup 2 <  over 0= kernel? 0= or  and if      \ no longer than the call (in the kernel a call
        drop inline,  1 inl-map r> + c!         \ before an exit is a jump, which is shorter)
        false stays !  exit
    then
    tcp @ cur-tcp0 !  tcp @ cur-save snap  true cur !
    over inline,
    false cur !
    drop  tcp @ cur-tcp0 @ - 2 > 0=             \ one instruction is left, or none: no more than the call
    stays @ or  false stays !                   \ or it was said to stay
    if
        drop  1 inl-map r> + c!
    else
        pend-body !  cur-tcp0 @ pend-tcp0 !  r> pend-ord !
        cur-save pend-save 12 cells move
        tcp @ pend-tcp0 @ - pend-len !
        pend-len @ 2/ 0 ?do  pend-tcp0 @ i 2* + t@  pend-code i cells + !  loop
        0 pend-xn !
        true pend !
    then
;

: plain-inline, ( body -- ) \ all of the word but its exit, as it is
    dup 3 cells + @ 1-  swap @ 2*               ( n addr )
    2dup swap 2* + t@ h# 7403 <> abort" a word compiled in place does not end with exit"
    swap 0 ?do dup i 2* + t@ tcode, loop
    drop
;

:: :
    hex
    there s>d
    <# bl hold # # # # #>
    lst @ write-file throw
    wordstr lst @ write-line throw

    ?own-page
    wants-label? if tcp @ 2/ (label) then
    tcp @ wordstart !
    label
    create  here lastdef !  codeptr , 0 , 0 , 0 , 0 ,
    true indef !
    does>   dup 4 cells + @ ?dup if             \ a baseword that is a word of the kernel's too
                kernel? 0= if                   \ (callable): anywhere else it is in line,
                    false stays !  nip execute exit     \ whether or not that was said
                then
                drop
            then
            call# @  1 call# +!                 ( body ord )
            plain @ if
                false stays !
                inl-map + c@ if plain-inline, else call, then
                exit
            then
            over 2 cells + @  peephole @ and if try-inline exit then
            stays @ abort" in-place is said of a word that is not small"
            drop call,
;

:: in-place ( -- ) \ the call that comes next is compiled in place whatever is joined
    true stays !
;

:noname ( body -- )
    dup @ dup near? if
        >target scall
    else
        literal
        s" execute" target-wordlist search-wordlist 0= throw
        >body @ scall
    then
    cell+ @ if tcp @ tpinned ! then
; is call,

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
    false indef !
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
\ The key map (keymap.fs): what each of 128 keys is, and what each is with
\ shift held.  It is said a row of keys at a time, by the code of the
\ row's first key, what the keys are and what they are shifted; and a key
\ that is the same either way by its code and what it is.  What is laid
\ down is 256 characters, the keys and then the keys shifted, packed into
\ cells as text is.
create key-plain 128 allot
create key-shifted 128 allot
variable key-at

:: keys[ ( -- ) key-plain 128 erase  key-shifted 128 erase ;

:: keyrow ( code "keys" "shifted" -- )
    key-at !
    parse-name parse-name                           ( keys n shifted n )
    rot over <> abort" a row of keys is as many shifted as not"
    0 ?do                                           ( keys shifted )
        over i + c@  key-plain   key-at @ i + +  c!
        dup  i + c@  key-shifted key-at @ i + +  c!
    loop
    2drop
;

:: keyis ( code what -- ) swap  2dup key-plain + c!  key-shifted + c! ;

:: ]keys ( -- )
    128 0 do key-plain i + c@ tc, loop
    128 0 do key-shifted i + c@ tc, loop
;

:: p,  pixels tc, ;
\ The same eight pixels as a cell of their own: a row of a small bitmap that
\ is fetched a row at a time.
:: pcell, ( byte -- ) tcalign pixels tcell, ;
\ A row of a pattern: eight pixels, repeated across a cell.
:: prow, ( byte -- )
    tcalign
    pixels cell-bytes 0 do dup tc, loop drop
;
\ How many cells n runs of 64 pixels are, laid down as a cell: a bitmap that
\ px, lays down says how long its rows are with this.
:: px-cells, ( n -- )
    #64 cell-bits / *
    tcalign
    there tcell!  cell-bytes tdp +!
;

\ A font is written as a strike: its glyphs side by side in one bitmap, and
\ a table of where each starts.  Drawing does not read that.  It reads the
\ font opened out: four cells for each glyph (where its rows are, how wide
\ it is, the first row with any black and the row after the last) and then
\ a cell for each row of each glyph, with the glyph at its left.  The
\ machine opens a font out the first time it is used (open-font, in
\ display.fs), and has both from then on.  The small machine has not the
\ room for both: so here the font is opened out as it is laid down, above
\ the display, and the strike is not in the image at all.  The font's
\ record says where it is opened out, and the machine finds it so.
\
\ In the source a font's record and table are between font[ and the strike,
\ and ]font is after the strike.
: tcell@ ( a -- x ) 0  cell-bytes 0 do over i + tc@ i 8 * lshift or loop  nip ;
create fstrike 512 cells allot              \ the strike: runs of 64 pixels, a row after a row
variable fstrike#
variable font-rec                           \ where the font's record is
variable in-font    false in-font !
variable fh         variable fn             \ its height, and how many glyphs
variable fruns                              \ runs of 64 pixels in a row of the strike
variable fkc                                \ the cell it is opened out from

: fpixel ( x y -- bit )
    fruns @ * over 6 rshift + cells fstrike + @
    swap 63 and 63 swap - rshift 1 and
;
: frow ( x0 w r -- pixels ) \ a row of a glyph, at the left of a cell as the display has it
    { x0 w r }
    0  w 0 ?do
        x0 i + r fpixel
        disp-left-low if i else cell-bits 1- i - then lshift or
    loop
;
: fcell! ( x cell -- ) byte-shift lshift tcell! ;
: fglyph ( g -- )
    { g }
    font-rec @ 5 g + cell-bytes * + tcell@
    font-rec @ 6 g + cell-bytes * + tcell@ over -
    fkc @ fn @ 4 * + g fh @ * +
    0 0 { x0 w rows first last }
    fh @ 0 ?do
        x0 w i frow
        dup if  last 0= if i to first then  i 1+ to last  then
        rows i + fcell!
    loop
    rows   fkc @ g 4 * +      fcell!
    w      fkc @ g 4 * + 1+   fcell!
    first  fkc @ g 4 * + 2 +  fcell!
    last   fkc @ g 4 * + 3 +  fcell!
;
:: font[ ( -- ) there font-rec !  0 fstrike# !  lofty in-font ! ;
:: ]font ( -- )
    in-font @ 0= if exit then
    false in-font !
    font-rec @ tcell@ fh !
    font-rec @ 2 cell-bytes * + tcell@ fn !
    fstrike# @ fh @ / fruns !
    thi @ byte-shift rshift fkc !
    fkc @ font-rec @ 4 cell-bytes * + tcell!        \ the record says where it is opened out
    fn @ 0 ?do i fglyph loop
    fn @ 4 fh @ + * cell-bytes * thi +!
;

\ 64 pixels of a bitmap, written with the leftmost highest, as the cells
\ the display would hold them in.
: px-lay ( pixels -- )
    in-font @ if fstrike fstrike# @ cells + !  1 fstrike# +!  exit then
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
:: px, ( pixels -- ) px-lay ;

\ A picture, drawn where it is wanted, between picture[ and ]picture.  A
\ row of it is the rest of a line after row|, in which # is a pixel of ink
\ and . is one of none, and the spaces are for whoever reads it.
\
\ A picture wider than a line of source is drawn in pieces, one under
\ another, with `beside` between them: the piece after it goes to the
\ right of what has been drawn, row for row.  So every piece has as many
\ rows as the first, and the rows of a piece are all as long.
\
\ The rows are kept until ]picture, which lays them down 64 pixels at a
\ time, as px, lays them: so a row of the whole picture is a whole number
\ of those, though a piece's need not be.
#64 constant pic-down                       \ the most rows a picture has,
#1024 constant pic-across                   \ and the most pixels a row
create pic-px  pic-down pic-across * allot  \ a character a pixel, 1 for ink: a row after a row
create pic-n   pic-down cells allot         \ how many pixels each row has so far
variable pic-rows                           \ how many rows the first piece had: -1 until it is done
variable pic-row                            \ the row of this piece that is next
variable in-picture     false in-picture !

: pic-piece ( -- ) \ the piece that was being drawn is done
    pic-row @ 0= abort" a piece of a picture has no rows"
    pic-row @ 0 do
        pic-n i cells + @  pic-n @  <> abort" the rows of a piece of a picture are not all as long"
    loop
    pic-rows @ 0< if pic-row @ pic-rows ! then
    pic-row @ pic-rows @ <> abort" a piece of a picture has not as many rows as the first"
;
:: picture[ ( -- )
    in-picture @ abort" picture[ within a picture"
    true in-picture !  -1 pic-rows !  0 pic-row !
    pic-n pic-down cells erase
;
:: beside ( -- )
    in-picture @ 0= abort" beside with no picture"
    pic-piece  0 pic-row !
;
:: row| ( "pixels" -- )
    in-picture @ 0= abort" a row of a picture with no picture[ before it"
    pic-row @ pic-down = abort" a picture has too many rows"
    0 parse bounds ?do
        i c@ bl <> if
            i c@ [char] # <>  i c@ [char] . <>  and abort" a picture is # and ."
            pic-n pic-row @ cells + @               ( across )
            dup pic-across = abort" a row of a picture is too long"
            i c@ [char] # = 1 and
            over  pic-row @ pic-across * +  pic-px + c!
            1+  pic-n pic-row @ cells + !
        then
    loop
    1 pic-row +!
;
:: ]picture ( -- )
    in-picture @ 0= abort" ]picture with no picture"
    pic-piece  false in-picture !
    pic-n @ 63 and abort" a row of a picture is not a whole number of 64 pixels"
    pic-rows @ 0 do
        pic-n @ 0 do                                \ 64 pixels at a time, the leftmost highest
            0  64 0 do  2*  k pic-across * j + i + pic-px + c@ or  loop
            px-lay
        64 +loop
    loop
;
\ A cell for each number of pixels from none to a whole cell: that many
\ pixels at the left of a cell.
:: lefts, ( -- )
    tcalign
    cell-bits 1+ 0 do
        i cell-bits = if -1 else 1 i lshift 1- then
        disp-left-low 0= if cell-bits i - lshift then
        tcell,
    loop
;

\ ," text" lays down a counted string in data space.
\ Tables of the names for the decompiler, made when the build is done and
\ every word is where it will be: the words and the labels by execution
\ token, and those that are a number (a word whose code only pushes one,
\ which a variable or anything made with create is, and a label for data)
\ by the number.  A table is how many entries it has, then their keys, in
\ order so that one is found by halving, and then, in the same order,
\ where the name of each is: the address of the header, or for a label
\ how far it is into the labels, with the top bit of the cell set.  A key
\ and a name are a cell each, so the tables are the same for a cell of
\ any size.  The names are looked for as a search would find them, the
\ dictionary before the labels and the newest first, and the first with a
\ key is kept.
\
\ The tables are not part of the image.  They go to a file, which is put
\ on the disk, and the decompiler reads them in when it has work to do.

create ntk 8192 cells allot                 \ the keys,
create ntn 8192 cells allot                 \ and where each one's name is
variable nt#  0 nt# !

: nt-add ( hdr label? key -- )
    dup cell-mask and over <> if drop 2drop exit then   \ a number a cell does not hold is nobody's key
    nt# @ 0 ?do
        dup ntk i cells + @ = if drop 2drop unloop exit then
    loop
    ntk nt# @ cells + !
    if 1 cell-bits 1- lshift or then
    ntn nt# @ cells + !  1 nt# +!
;
: nt-sort ( -- )
    nt# @ 2 < if exit then
    nt# @ 1 do
        ntk i cells + @  ntn i cells + @  i         ( k n j )
        begin
            dup 0> if 2 pick over 1- cells ntk + @ u< else false then
        while
            dup 1- cells ntk + @  over cells ntk + !
            dup 1- cells ntn + @  over cells ntn + !
            1-
        repeat
        tuck cells ntn + !  cells ntk + !
    loop
;
create ntf 8192 cells allot                 \ the file: its cells
variable ntf#  0 ntf# !
: ntf, ( x -- ) ntf ntf# @ cells + !  1 ntf# +! ;
: nt-lay ( -- ) \ a table into the file: how many, the keys, and where their names are
    nt-sort
    nt# @ ntf,
    nt# @ 0 ?do ntk i cells + @ ntf, loop
    nt# @ 0 ?do ntn i cells + @ ntf, loop
    0 nt# !
;

: hdr-link ( a -- a' ) byte-shift lshift tflash + @ cell-mask and ;
: hdr-val  ( a -- x )  byte-shift lshift cell-bytes + tflash + @ cell-mask and ;
: hdr-xt   ( a -- xt ) hdr-val  1 cell-bits 1- lshift invert and  $ffff and ;   \ less the immediate flag
: lbl-link ( n -- n' ) byte-shift lshift lflash + @ cell-mask and ;    \ a label, by its place
: lbl-val  ( n -- x )  1+ byte-shift lshift lflash + @ cell-mask and ;
: lbl-data? ( x -- f ) 1 cell-bits 1- lshift and 0<> ;                 \ is its value an address of data?

: tlv ( w -- n ) dup $fff and swap 12 rshift 3 and 12 * lshift ;
: lit? ( w kind -- f ) over $c000 and =  swap $3000 and $3000 <> and ;
: thval ( xt -- n true | false ) \ the number, if the code there only pushes one (hval in see.fs)
    2*  dup t@ dup $8000 lit? 0= if 2drop false exit then
    tlv swap                                        ( n a )
    begin
        2 + dup t@ dup $c000 lit?
    while
        tlv rot + swap
    repeat
    nip $7403 = if true else drop false then
;

: xt-table ( -- )
    link @ begin dup while dup false over hdr-xt nt-add  hdr-link repeat drop
    llink @ begin
        dup
    while
        dup lbl-val lbl-data? 0= if dup true over lbl-val $ffff and nt-add then
        lbl-link
    repeat drop
    nt-lay
;
: number-table ( -- )
    link @ begin
        dup
    while
        dup hdr-xt thval if
            over false rot nt-add
        then
        hdr-link
    repeat drop
    llink @ begin
        dup
    while
        dup lbl-val dup lbl-data? if
            1 cell-bits 1- lshift invert and  over true rot nt-add
        else
            drop
        then
        lbl-link
    repeat drop
    nt-lay
;

:: hdr, ( "name" -- ) \ a cell that holds the address of the word's header
    bl parse headers-wordlist search-wordlist 0= abort" hdr, of a word with no header"
    >body @  tcalign tcell,
;

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

: end-word ( -- )
    tcp @ wordstart @ = if
        s" exit" evaluate
    else
        tcp @ 2 - shortcut \ true if shortcut applied
        \ A jump that was written as one is the end too: nothing comes
        \ back to after it.  One that a word before it reads is not.
        tcp @ 2 - t@ h# e000 and 0=  tpinned @ tcp @ 2 - <> and  or
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

\ An exit in the middle of a word is as the one at its end: a call just
\ before it is made a jump, which returns for both.  Not if something
\ lands between the two.  And not if the exit is not an exit at all but
\ the instruction that the word just called reads and copies (`inline:
\ exit`): that one is laid down as it is written.  A small word waiting
\ before it is measured as one before the end of a word is, against the
\ jump.
: exit-here ( -- )
    tpinned @ tcp @ <>  tcp @ wordstart @ <>  and if
        1 fusable? if  tcp @ 2 - shortcut if exit then  then
    then
    h# 7403 tcode,
;
:: exit
    pend @ if
        true ending !  h# 7403 tcode,  false ending !
        tcp @ 2 - t@ h# 7403 = if 1 retract exit-here then
    else
        exit-here
    then
;

:: ;
    \ A small word tried in place at the end: the exit settles it.  If the
    \ exit was joined to it that is the end; if not, the word is called
    \ after all, and the call may be made a jump as any other is.
    stays @ abort" in-place with no call after it"
    pend @ if
        true ending !  h# 7403 tcode,  false ending !
        tcp @ 2 - t@ h# 7403 = if 1 retract end-word then
    else
        end-word
    then
    indef @ if
        small? lastdef @ 2 cells + !
        tcp @ wordstart @ - 2/ lastdef @ 3 cells + !
        false indef !
    then
;

:: ;fallthru ;

\ A baseword as a word of the kernel's as well: `header 0=  callable 0=`.
\ Its body is the baseword's own instructions, so nothing is said again.
\ In the kernel, which has to fit its ROM, a use of it is a small word's:
\ it is tried in place, and called where that is shorter.  Anywhere else
\ it is what it was, its instructions in line.
s" :" target-wordlist search-wordlist 0= throw constant the-colon
s" ;" target-wordlist search-wordlist 0= throw constant the-semicolon
:: callable ( "name" -- )
    >in @
    bl word count target-wordlist search-wordlist 0= abort" callable: there is no such baseword"
    swap >in !                                  ( the baseword )
    >r
    the-colon execute
    r@ execute
    the-semicolon execute
    r> lastdef @ 4 cells + !
;

:: jmp
    ' >body @ >target ubranch
;

:: constant
    create  ,
    does>   @ literal
;

\ A table that is instructions, as the rules of the nucleus's peephole are,
\ is laid down among the code, an instruction to a place, where i@ reads
\ it.  It is not code to be run, and nothing is joined to it or across it.
\
\ It has to be all on one page, since what reads it goes by how far along
\ it is.  table-room is said before a table, with how many instructions it
\ is: if this page has not room for them, the next is started.
: table-room ( n -- )
    settle
    2* tcp @ $1fff and + $1ffe u> if next-page then
;
: (i,) ( w -- )
    settle
    tcp @ $1fff and $1ffe = abort" a table of instructions has run off its page"
    tcp @ tw!  2 tcp +!
    label
;
:: i, ( w -- ) (i,) ;
: is, ( w n -- ) 0 ?do dup (i,) loop drop ;         \ the same in each of n places

\ A header in front of create names the data.  The header points at code,
\ so code is laid down for it that pushes the data's address, as a word
\ made with create at run time has.
:: create
    tcalign                                 \ before the label: it holds where the data will be
    named? dup >r 0= labelling and if
        there taddr 1 cell-bits 1- lshift or (label)
    then
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
:: NAME-PER   name-per literal ;                \ codes of a name in a cell,
:: NAME-BITS  name-bits literal ;               \ and the bits they are,
:: NAME-SPARE cell-bits name-bits - literal ;   \ and the bits of a cell that are not theirs
:: codes-of-chars, codes-of-chars, ;            \ the two tables of a name's codes
:: chars-of-codes, chars-of-codes, ;
:: TOP16      cell-bits #16 - literal ;         \ shift that puts 16 bits at the top
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
:: forth      var-base literal ;                \ which holds the newest header's address
:: HDR-BASE   hdr-base literal ;                \ where headers start
\ A block of source is BLOCK-LINES lines of LINE-CHARS characters;
\ LINE-SHIFT is the shift that multiplies by LINE-CHARS, and LAST-COLUMN
\ and LAST-LINE are the last of each, counted from nothing.
:: LINE-CHARS  disk-line literal ;
:: BLOCK-LINES disk-block disk-line / literal ;
:: LINE-SHIFT  0 begin 1 over lshift disk-line < while 1+ repeat literal ;
:: LAST-COLUMN disk-line 1- literal ;
:: LAST-LINE   disk-block disk-line / 1- literal ;
:: BLOCK-CHARS disk-block literal ;
\ What \ makes of >in in a block, which is two past the \: this added and
\ then that kept is where the line the \ is in ends.
:: LINE-ROUND  disk-line 2 - literal ;
:: LINE-MASK   disk-line negate literal ;
:: PICTURE-BLK disk-picture-block literal ;     \ what is where on the disk: vm_constants.h
:: NAMES-BLK   disk-names-block literal ;
:: CANVAS-BLK  disk-canvas-block literal ;
:: DATA-LIMIT data-limit literal ;              \ where data must stop: there, or where the bulk starts
\ The cell after the last of memory.  Where memory is as many cells as a
\ cell can count that is nothing, and is still right to take from.
:: MEM-END    1 addr-bits lshift cell-mask and literal ;
:: CODE-BASE  code-base literal ;
:: CODE-SIZE  code-size literal ;
:: DISPLAY? disp-w 0<> ;                \ has the machine a display?
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
    settle
    0 0branch
    tcp @ 2 -                                   \ where the branch is, once it is compiled
;

:: DOUBLE
    settle
    tcp @ 2 + 2/ >target scall
;

:: then
    resolve
;

:: else
    settle
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
    settle
    0 0branch
    tcp @ 2 -
;
\ A loop that tests at the top goes round with a jump: test, branch out,
\ body, jump.  If the test is one instruction that says whether two things
\ are the same (xor, or =), the same instruction with the other of the two
\ operations says the opposite, and a conditional branch after the body
\ goes round on that: test, branch out, body, test, branch back.  A turn is
\ then one instruction shorter, and the loop one longer.
: turned ( insn -- insn' true | false )
    dup h# f300 and h# 6000 <> if drop false exit then      \ ALU, to T, no return
    dup h# f0 and dup h# 60 = swap h# 90 = or 0= if drop false exit then
    h# f0 xor true
;
:: repeat ( dest orig -- )
    2dup swap 2 + = if
        over t@ turned if
            tcode,
            dup 2 + 2/ >target 0branch
            nip resolve exit
        then
    then
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
\ The file of names is two cells and then the rest: a stamp, which the
\ image has too, so that names made for another build are not used; how
\ many cells follow; the two tables; and the labels.  forth0 is told the
\ newest word of the dictionary, and stamp the stamp.  The file is beside
\ the image and has its name (build/nuc.tables), and the tools that read an
\ image for its labels look for it there.
create ntb 8 allot                          \ a cell of the file, as its bytes
: cell>file ( x fid -- ) \ a cell of the machine's, its lowest byte first
    swap cell-bytes 0 do dup $ff and ntb i + c!  8 rshift loop drop
    ntb cell-bytes rot write-file throw
;
: name-tables ( forth0 stamp -- )
    link @  tcp @ 16 lshift xor  lp @ 7 lshift xor  cell-mask and   ( forth0 stamp x )
    dup rot tvar!                                       ( forth0 x )
    link @ rot tvar!                                    ( x )
    0 ntf# !
    ntf,  0 ntf,
    xt-table number-table
    llink @  lp @  0 lp !  swap lcell,  lp !            \ the newest label, in the first cell of them
    ntf# @ 2 -  lp @ byte-shift rshift +  ntf cell+ !
    s" tables" out-suffix
    ntf# @ 0 ?do ntf i cells + @ over cell>file loop
    dup lflash lp @ rot write-file throw
    close-file throw
;

:noname
    s" lst" out-suffix lst !
    s" sizes" out-suffix sizes !
; execute

\ The map of calls compiled in place: read for a build with the peephole
\ off, and written at the end of one with it on.
: inline-name ( -- c-addr u ) s" inline" >str prefix. +str s" ../build/" >str +str str@ ;
plain @ [if]
    inline-name r/o open-file 0= [if]
        dup inl-map 65536 rot read-file throw drop  close-file throw
    [else]
        drop
    [then]
[then]


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

s" (all)" tcp @  tdp @ data-base -  thp @ hdr-base - +  tbp @ bulk-base - +  hdr-bytes @ lbl-bytes @ 0 size-line
in-bulk @ [if] cr .( bulk with no unbulk) abort [then]
tdp @ data-limit > [if] cr .( the data has run into what is after it) abort [then]
tbp @ hdr-base >  bulky and [if] cr .( the bulk has run into the dictionary headers) abort [then]
thi @ 131072 >  lofty and [if] cr .( what is laid down above the display has run off the end of memory) abort [then]
thp @ code-base 2* > [if] cr .( the dictionary headers have run into the code) abort [then]
tcp @ code-base code-size + 2* > [if] cr .( the code has run past where code can be) abort [then]
sizes @ close-file throw
link @ var-base tvar!                   \ the newest header, in the first cell of the data
plain @ 0= [if]
    inline-name create-output-file  dup inl-map call# @ rot write-file throw  close-file throw
[then]
dumpall.32
." tdp " tdp @ . ." thp " thp @ .
." tcp " tcp @ .

bye
