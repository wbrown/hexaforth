( J1 Cross Compiler                          JCB 16:55 05/02/12)

\ cross.fs - the cross-compiler, run by gforth:
\     gforth cross.fs basewords.fs nuc.fs
\ basewords.fs is the machine (MODEL.md): its sizes and the
\ fields of an instruction.  nuc.fs is the program.
\
\ The image is the bytes of tflash; tcp, tdp and thp count
\ bytes of it.  An instruction is 16 bits.  A cell is
\ cell-bytes.  A code page is 4,096 instructions ($2000 bytes).
\ Page 0 is the kernel's, and its first krom-bytes may be a ROM.

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

variable tdp                    \ Data pointer
variable thp                    \ where the next header goes
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

\ Every instruction is compiled by tcode, , which first tries to
\ combine it with those before it:
\   exit   sets RET and r-1 in an ALU instruction that leaves
\          the return stack alone
\   +      makes a pushed literal an add
\   n +    is added into a literal of the same shift
\   @ !    after a variable's address are one instruction
\   ALU    with the one or two before it, is the one instruction
\          that does the same; or three are two
\   a run with numbers or variables in it is a shorter run
\ The machine is asked which (util/fuse.h).
\ Instructions named by number: 7403 exit, 643c +, 6800 @,
\ 6308 !, 6480 invert, 6400 does nothing, 8000 a literal pushed,
\ c000 added, b000 f000 a variable fetched and stored (a literal
\ whose shift field is 3: its 12 bits are the cell's offset from
\ var-base), 1003 RET and r-1.

variable tfence     0 tfence !                  \ the last place a branch or call lands: none is combined across it
variable tpinned    -1 tpinned !                \ an instruction that stays as written: the word called before it reads it
variable peephole   true peephole !

\ PEEPHOLE=off is the plain build: nothing is combined.  It is
\ not run: the test `builds` compares it with the image, run for
\ run.  It inlines the calls the image inlined (.inline), so
\ that the two have the same calls.
variable plain      false plain !               \ true for the plain build
s" PEEPHOLE" getenv s" off" compare 0= [if] false peephole !  true plain ! [then]

\ A small word is inlined where that is no longer than a call.
\ It is inlined and waits (pend): outside the kernel up to three
\ pushes are let past it, and the instruction after is given to
\ the peephole.  If none combined, or the result is longer than
\ the call and the pushes, its instructions are taken out and it
\ is called (backout).  settle does that at once where a branch
\ lands.  At the end of a definition the call would be a jump
\ with no exit after it, which is what the word is held to.

create inl-map 65536 allot  inl-map 65536 erase \ a byte for each call, by its number: 1 if it was inlined (.inline)
variable call#      0 call# !                   \ the number of the next use of a word, or `repeat`
variable pend       false pend !                \ true while an inlined word is waiting
variable pend-body                              \ the word that is waiting
variable pend-tcp0                              \ where its instructions start
variable pend-ord                               \ its call's number
create pend-save 12 cells allot                 \ the twelve instructions before it, to put back
variable pend-len                               \ the bytes it was compiled as,
create pend-code 8 cells allot                  \ and the instructions
variable pend-xn    0 pend-xn !                 \ how many pushes were compiled after it,
create pend-x 3 cells allot                     \ and the pushes
variable 'ptcode                                \ compiles an instruction through the peephole
variable cur        false cur !                 \ true while a small word's instructions are being inlined
variable stays      false stays !               \ true from `in-place` to the call after it
variable ending     false ending !              \ true while the exit that ends a definition is compiled
variable cur-tcp0                               \ where the word being inlined starts,
create cur-save 12 cells allot                  \ and the twelve instructions before it
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

: (backout) ( -- ) \ remove the waiting word's instructions, call it, and compile its pushes again
    tcp @ pend-tcp0 @ > if
        tcp @ pend-tcp0 @ do 0 i tw! 2 +loop
    then
    pend-tcp0 @ tcp !
    pend-tcp0 @ pend-save unsnap
    pend-body @ call,
    pend-xn @ 0 ?do pend-x i cells + @ 'ptcode @ execute loop
    0 pend-xn !
;

: unchanged? ( -- f ) \ are the waiting word's instructions still as they were compiled?
    tcp @  pend-tcp0 @ pend-len @ +  u< if false exit then
    pend-len @ 2/ 0 ?do
        pend-tcp0 @ i 2* + t@  pend-code i cells + @ <> if false unloop exit then
    loop
    true
;

: settle ( -- ) pend @ if false pend !  (backout) then ;

: label     settle  tcp @ tfence ! ;    \ a branch or a call lands here

: (tcode,)
    tcp @ $1fff and $1ffe = abort" code page full"
    tcp @ tw! tcell tcp +! ;
: retract ( n -- ) \ take back the last n instructions
    0 ?do  tcp @ 2 - tcp !  0 tcp @ tw!  loop ;

\ May the n instructions before tcp be replaced?  Not if a branch
\ lands among them, or one of them is pinned.
: fusable? ( n -- f )
    2* tcp @ over u< if drop false exit then
    tcp @ swap -                            ( first )
    dup tfence @ u< if drop false exit then
    tpinned @ swap tcp @ within 0=
;

: foldable? ( insn -- f ) \ ALU, no RET, r+0, result not to R or IO
    dup h# f003 and h# 6000 =
    swap h# 0300 and dup h# 0100 <> swap h# 0200 <> and and
;

: fold-exit ( insn -- insn false | true ) \ an exit, folded into the instruction before
    dup h# 7403 = if
        1 fusable? if
            tcp @ 2 - t@ foldable? if
                drop tcp @ 2 - t@ h# 1003 or
                1 retract tcode,
                true exit
            then
        then
    then
    false
;

: lkind ( insn -- kind ) \ h# 8000 a push, h# c000 an add, 0 not a literal
    dup h# 3000 and h# 3000 = if drop 0 exit then   \ shift field 3: a variable's instruction
    h# c000 and dup h# 8000 and 0= if drop 0 then
;

: fold-lit+ ( insn -- insn false | true ) \ `+` after a literal makes its push an add
    dup h# 643c <> if false exit then
    0 begin                                 ( insn n )
        1+
        dup 2* tcp @ u> if drop false exit then
        tcp @ over 2* - t@ lkind            ( insn n kind )
        dup h# c000 =
    while
        drop
    repeat
    h# 8000 <> if drop false exit then
    dup fusable? 0= if drop false exit then
    nip
    tcp @ swap 2* - dup t@ h# 4000 or swap tw!
    true
;

\ an add after a literal of the same shift, if the sum fits
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

\ @ or ! after a literal that is the address of one of the 4,096
\ cells from var-base: the literal is removed, and the @ or !
\ becomes the variable's one instruction.  It returns false, so
\ the caller goes on to compile that instruction.
variable fv-n                               \ how many instructions the literal is
variable fv-a                               \ the address it pushes
variable fv-base    -1 fv-base !            \ var-base
: fold-var ( insn -- insn' false )
    dup h# 6800 <> over h# 6308 <> and if false exit then   \ neither @ nor !
    0 fv-a !  0 fv-n !
    begin
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

\ What the machine is asked (util/fuse.h, bound further down):
\ the one instruction that does the work of two or three (first
\ is 0 for two); the two that do the work of three, the first in
\ the upper sixteen bits; and a shorter run for a run that has
\ numbers in it.  run and out are addresses here of 16-bit
\ instructions, n and m how many.  branch? is 1 if a conditional
\ branch follows the run: then only whether the top is 0 need be
\ the same.
defer machine-fuse ( first second third -- one | 0 )
defer machine-two ( first second third -- two | 0 )
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

: fuse-three ( insn -- insn false | true )
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
        fold-var    if exit then
        fuse-lit    if exit then
        fold-lit+   if exit then
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

\ The kernel is page 0 without what `nearby` puts there.  In it
\ a `callable` baseword is the kernel's word, and no push is let
\ past a waiting word.
variable laid-near  false laid-near !           \ true from nearby to nearby-end
: kernel? ( -- f ) tcp @ 2/ h# 1000 u<  laid-near @ 0= and ;

\ tcode, : an instruction, compiled while a small word may be waiting
:noname ( insn -- )
    pend @ 0= if ptcode, execute exit then
    dup lkind h# 8000 =  kernel? 0= and  cur @ 0= and  pend-xn @ 3 < and if
        dup pend-x pend-xn @ cells + !  1 pend-xn +!        \ a push: keep waiting, for what uses it
        false pend !  ptcode, execute  true pend !
        exit
    then
    false pend !
    dup ptcode, execute
    unchanged?                                  \ nothing combined with it, or it is longer than the call:
    tcp @ pend-tcp0 @ -  pend-xn @ 2*  ending @ if 2 else 4 then +  >  or   \ call it after all
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

\ Page 0 is 8KB and the ROM is its first krom-bytes.  The rest
\ of the page is ordinary memory that a call reaches from any
\ page in one instruction.  Code between upper-half and
\ lower-half is compiled there.
variable upper                                  \ where the next code compiled there goes
variable lower                                  \ where code was being compiled before upper-half
: upper-half ( -- ) plain @ if exit then  tcp @ lower !  upper @ org ;
: lower-half ( -- )
    plain @ if exit then
    tcp @ $2000 > abort" the upper half of the kernel's page is full"
    tcp @ upper !  lower @ org
;

\ n code-page continues code on page n, from where that page's
\ code stopped, so the program can come back to a page.
\ The plain build ignores this: its code is longer and would not
\ fit the same pages.  It starts a new page before any
\ definition that begins in the last quarter of a page
\ (?own-page).
variable page-min   \ the byte where the first page after page 1 starts (code-base, as bytes)
16 constant code-pages
create page-at code-pages cells allot   page-at code-pages cells erase   \ for each page, the byte its code has reached
: page-kept ( -- ) tcp @  tcp @ $2000 / cells page-at +  ! ;
: code-top ( -- byte ) page-kept  0  code-pages 0 do page-at i cells + @ max loop ;    \ the end of the last page's code
: next-page ( -- ) code-top $1fff + $2000 negate and  page-min @ max  org ;
: code-page ( n -- )
    plain @ if drop exit then
    page-kept  dup cells page-at + @ ?dup if nip else $2000 * then  org
;
: ?own-page ( -- ) \ before a definition, in the plain build
    plain @  tcp @ $2000 u< 0= and  tcp @ $1fff and $1800 u> and
    if next-page then
;

wordlist constant target-wordlist
: add-order ( wid -- ) >r get-order r> swap 1+ set-order ;

\ ---- who uses what: every definition is noted with its file
\ (note-def), and every use of one by its name (name-used)
variable file-now   0 file-now !            \ the file being read.  0 is this one, and the machine's
variable files#     2 files# !
256 constant files-room
create file-names files-room 2* cells allot \ the name of each,
create file-open files-room cells allot     \ and whether it is being read
file-open files-room cells erase
s" (cross.fs)" save-mem file-names 2!
s" (the program)" save-mem file-names 2 cells + 2!      \ 1 is the file that is compiled
1 file-open !

: file-id ( a u -- n )
    files# @ 0 ?do
        2dup i 2* cells file-names + 2@ compare 0= if 2drop i unloop exit then
    loop
    files# @ files-room = abort" more files than there is room to note"
    files# @ 2* cells file-names + 2!
    files# @  1 files# +!
;

wordlist constant said-wordlist             \ the names the source gave a header

2048 constant saids-room
create saids saids-room 3 * cells allot     \ of each: the file that gave it its header, and the name
variable saids#     0 saids# !
: said-at ( i -- a ) 3 * cells saids + ;
: note-said ( -- ) \ the name that create has just made in said-wordlist
    saids# @ saids-room = abort" more headers than there is room to note"
    saids# @ said-at >r
    file-now @ r@ !  latest name>string r> cell+ 2!
    1 saids# +!
;

4096 constant defs-room
create defs defs-room 5 * cells allot       \ of each: its body, its file, whether a header named it, its name
variable defs#      0 defs# !
variable said-header                        \ whether a header named the word being defined
: def-at ( i -- a ) 5 * cells defs + ;

: note-def ( -- ) \ the word that create has just made: its body is here
    defs# @ defs-room = abort" more words than there is room to note"
    defs# @ def-at >r
    here r@ !  file-now @ r@ cell+ !  said-header @ r@ 2 cells + !
    latest name>string r> 3 cells + 2!
    1 defs# +!
;

: def-of { body -- a | 0 } \ what was noted of the word with that body
    0 defs# @ 0 { lo hi mid }
    begin
        lo hi <
    while
        lo hi + 2/ to mid
        mid def-at @ body = if mid def-at exit then
        mid def-at @ body u< if mid 1+ to lo else mid to hi then
    repeat
    0
;

1024 constant uses-room
create uses uses-room 4 * cells allot       \ the word, the file that uses it, how often, and
variable uses#      0 uses# !               \ whether the word's own file was still being read
: use-at ( i -- a ) 4 * cells uses + ;

create file-needs files-room dup * allot    \ which file uses a word of which (.needs)
file-needs files-room dup * erase
: needs-at ( user of -- a ) swap files-room * + file-needs + ;

: name-used ( body -- ) \ a word is used by name, in the file being read
    def-of dup 0= if drop exit then                     ( d )
    dup cell+ @ file-now @ = if drop exit then
    1  file-now @  2 pick cell+ @  needs-at c!
    dup 2 cells + @ if drop exit then                   \ it has a header
    dup 3 cells + 2@ said-wordlist search-wordlist if 2drop exit then
    uses# @ 0 ?do
        dup i use-at @ =  i use-at cell+ @ file-now @ =  and if
            drop  1 i use-at 2 cells + +!  unloop exit
        then
    loop
    uses# @ uses-room = abort" more uses of words with no header than there is room to note"
    uses# @ use-at >r
    dup r@ !  file-now @ r@ cell+ !  1 r@ 2 cells + !
    cell+ @ cells file-open + @ r> 3 cells + !
    1 uses# +!
;

\ A macro (::) defined by the program is two words: its
\ definition, kept in macro-bodies, and a word of its name that
\ records the use and then runs the definition.
wordlist constant macro-bodies
: macro-word ( "name" -- a ) \ a is the cell for the definition's xt
    create  false said-header !  note-def  here  0 ,
    does> dup name-used @ execute
;

\ Macros and the program's words are in one wordlist, and a
\ name has one case: a word `line` would hide the macro LINE
\ from every later file.  After each file every macro's name is
\ looked up again (?macros), and no macro takes a word's name
\ (macro,).
variable macros  0 macros !             \ each entry: link, xt, checked after each file?, length and name
variable listing  false listing !       \ true once the basewords are read: macros defined now are checked

: macro? ( xt -- f )
    macros @ begin dup while 2dup cell+ @ = if 2drop true exit then @ repeat nip
;
: macro, ( "name" -- a ) \ add the macro about to be defined to the list
    >in @  parse-name  rot >in !                ( c-addr u )
    2dup target-wordlist search-wordlist if
        macro? 0= if cr type ."  is a word of the program, and a macro may not take its name" abort then
    then
    align here >r  macros @ ,  0 ,  listing @ ,  dup ,  here swap dup allot move
    r> dup macros !
;
: :: ( "name" -- )
    macro, >r
    file-now @ if                               \ the program's: a word that runs it, and it under its name
        >in @ >r
        get-current >r  target-wordlist set-current  macro-word  r> set-current    ( a )
        latestxt  r> >in !  r> cell+ !
        >r
        get-current >r  macro-bodies set-current  :  r> set-current
        latestxt r> !
    else
        get-current >r target-wordlist set-current : r> set-current
        latestxt r> cell+ !
    then
;
\ a macro whose body is the lines of source up to ;source.  The
\ lines are compiled where the macro is used, not here.
: source: ( "name" -- )
    ::
    begin
        refill 0= abort" source: with no ;source"
        source -trailing bl skip                ( c-addr u )
        source nip >in !
        2dup s" ;source" compare
    while
        postpone sliteral  postpone evaluate
    repeat
    2drop  postpone ;
;
: macro-name ( a -- c-addr u ) 3 cells + dup cell+ swap @ ;
: ?macros ( c-addr u -- ) \ after the file named: does each macro's name still find the macro?
    macros @ begin dup while
        dup 2 cells + @ if
            dup macro-name target-wordlist search-wordlist
            0= abort" a macro's name is not found"
            macro? 0= if
                cr macro-name type ."  is a macro, and " type ."  has defined a word of that name" abort
            then
        then
        @
    repeat
    drop 2drop
;
: included-whole ( c-addr u -- ) 2dup 2>r included 2r> ?macros ;

next-arg included       \ include the machine.fs
true listing !

( The machine's sizes                                                 )

\ The machine file defines cell-bits and addr-bits.

cell-bits 8 / constant cell-bytes
var-base fv-base !
krom-bytes upper !

: rom-ends ( -- ) \ used where the kernel's code ends: it must fit the ROM
    plain @ if exit then
    tcp @ krom-bytes > if
        cr ." the kernel is " tcp @ krom-bytes - . ." bytes longer than its ROM" abort
    then
;

\ the machine, for the peephole to ask: util/fuse.h, built here
: here-is ( -- c-addr u ) \ the directory this is run in
    s" pwd" r/o open-pipe throw >r
    pad 512 r@ read-line throw drop
    r> close-pipe throw drop
    pad swap
;
s" #define CELL_BITS " cell-bits 0 <# #s #> s+ save-c-prefix-line
s" #include " s\" \"" s+  here-is s+  s\" /../util/fuse.h\"" s+ save-c-prefix-line
s" m" add-lib
c-function (machine-fuse) fuse_one n n n -- n
c-function (machine-two) fuse_two n n n -- n
c-function (machine-lit) fuse_lit a n n a -- n
' (machine-fuse) is machine-fuse
' (machine-two) is machine-two
' (machine-lit) is machine-lit
cell-bytes 2 = [if] 1 [else] cell-bytes 4 = [if] 2 [else] 3 [then] [then] constant byte-shift
var-base byte-shift lshift constant data-base       \ the byte of the image the data starts at

: taddr ( byte -- a ) byte-shift rshift ;   \ the number of a cell

: one-literal? ( a -- f ) \ a 12-bit value shifted left by 12 or 24?
    dup 0= if exit then
    false swap
    #25 #12 do
        dup 1 i lshift 1- and 0=  over i rshift #4096 u< and  rot or swap
    #12 +loop
    drop
;
: display-ok ( -- ) \ the display suits the cell (MODEL.md)
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

\ The image, from byte 0: code pages 0 and 1; the data, from the
\ cell var-base (the variables first, which one instruction
\ fetches or stores); the headers, from hdr-base; and the code
\ pages after page 1, from code-base.  hdr-base, bulk-base and
\ data-base are bytes.  code-base and code-size are instructions.
\ A header holds its word's xt in 16 bits, and on a machine with
\ cells of 16 bits the top bit of those is the immediate flag.
addr-bits #16 > 0= constant small           \ a machine whose memory is 64K cells
small [if] $6000 [else] cell-bits #64 < [if] $c000 [else] $a800 [then] [then] constant hdr-base
hdr-base thp !
data-base cell-bytes + tdp !                \ the first cell of the data is `forth`
small [if] $4000 [else] $8000 [then] constant code-base
small [if] $4000 [else] $8000 [then] constant code-size
code-base 2* page-min !
: sys-page ( n -- ) code-base $1000 / + code-page ;    \ the nth page past page 1
: low-page ( -- ) small if upper-half then ;            \ the kernel's upper half, where pages are few
: low-page-end ( -- ) small if lower-half then ;

\ bulk ... unbulk: data that is not variables, laid past the
\ cells one instruction reaches, where the machine has such a place
small [if] $5400 [else] data-base 4096 cell-bytes * + [then] constant bulk-base
bulk-base hdr-base < constant bulky
variable tbp    bulk-base tbp !
variable bsave
variable in-bulk    false in-bulk !
bulky [if] bulk-base [else] hdr-base [then] constant data-limit

\ high ... unhigh: data reached only a cell at a time, laid above
\ the display on the small machine, and bulk on any other
: display-end ( -- cell ) \ where the last bank's last row ends: an even cell
    disp-bank-step disp-banks 1- *
    disp-h disp-banks + 1- disp-banks / disp-stride * +
    disp-base +  1+ -2 and
;
small disp-w 0<> and constant lofty
variable thi    display-end cell-bytes * thi !
variable hsave

: tcalign   tdp @ cell-bytes 1- + cell-bytes negate and tdp ! ;

: tcell! ( x a -- ) \ a is in bytes
    cell-bytes 0 do
        over i 8 * rshift over i + tc!
    loop
    2drop ;

: tvar! ( x a -- ) \ a is the number of a cell
    byte-shift lshift tcell! ;

( Language basics for target                 JCB 19:08 05/02/12)

warnings off
:: ( postpone ( ;
:: \ postpone \ ;

:: org          org ;
\ .sizes has a line for each file included: its name, the bytes
\ of its code, the bytes of its data, how many of those are
\ headers and how many labels, the instruction its code starts
\ at, and how many includes deep the file is (1 for the program).
variable hdr-bytes  0 hdr-bytes !
variable lbl-bytes  0 lbl-bytes !
variable sizes      0 sizes !
variable included-deep  0 included-deep !
variable included#  0 included# !

: size-field ( n -- ) s>d <# bl hold #s #> sizes @ write-file throw ;

: size-line ( a u code data headers labels start deep -- )
    sizes @ 0= if 2drop 2drop 2drop 2drop exit then
    base @ >r decimal
    >r >r 2>r 2>r
    sizes @ write-file throw  s"  " sizes @ write-file throw
    2r> swap size-field size-field
    2r> swap size-field size-field
    r> size-field  r> size-field
    s" " sizes @ write-line throw
    r> base !
;

: one-run ( from -- bytes ) \ the code from there, if it is one run of one page
    tcp @ over $2000 / over $2000 / =  2 pick 2 pick u> 0=  and if swap - else 2drop 0 then ;

: include-measured ( a u -- ) \ include a file, and write its line of the .sizes
    { a u }
    tcp @ tdp @ thp @ + tbp @ + hdr-bytes @ lbl-bytes @ { c d h l }
    file-now @ { was }
    a u file-id file-now !  1 file-now @ cells file-open + +!
    1 included-deep +!  1 included# +!
    a u included-whole
    -1 file-now @ cells file-open + +!  was file-now !
    a u  c one-run  tdp @ thp @ + tbp @ + d -  hdr-bytes @ h -  lbl-bytes @ l -  c 2/  included-deep @  size-line
    -1 included-deep +!
;
:: include  parse-name save-mem include-measured ;

\ nearby ... nearby-end: the code between them is laid in the
\ upper half of the kernel's page.  Each stretch gets a line of
\ its own in the .sizes, under its file's name, with depth 0.
variable near-from                              \ where the stretch starts
variable near-deep  0 near-deep !               \ how many nearby wait for their nearby-end
variable near-files                             \ how many files had been included when it started
:: nearby ( -- )
    1 near-deep +!  near-deep @ 1 > if exit then
    upper-half  true laid-near !  tcp @ near-from !  included# @ near-files ! ;
:: nearby-end ( -- )
    near-deep @ 0= abort" nearby-end with no nearby"
    -1 near-deep +!  near-deep @ if exit then
    settle
    plain @ 0=  included# @ near-files @ =  and if
        file-now @ 2* cells file-names + 2@  tcp @ near-from @ -  0 0 0  near-from @ 2/  0  size-line
    then
    false laid-near !  lower-half ;

\ files named in the environment: SYSTEM, the system's list;
\ ROM, the driver of the disk the machine starts from; FUSED,
\ the decompiler's table
:: include-system  s" SYSTEM" getenv dup 0= if 2drop s" system.fs" then save-mem include-measured ;
:: include-rom  s" ROM" getenv dup 0= if 2drop s" port-disk.fs" then save-mem include-measured ;
:: include-fused  s" FUSED" getenv dup 0= if 2drop s" fused.fs" then save-mem include-measured ;

:: lacks? ( "name" -- f ) parse-name target-wordlist search-wordlist if drop false else true then ;

variable top-kept  0 top-kept !     \ the cells at the top of memory that the ROM's driver keeps

:: bulk ( -- )
    in-bulk @ abort" bulk within bulk"  true in-bulk !
    bulky if tdp @ bsave !  tbp @ tdp ! then
;
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
:: high ( -- )
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

\ A literal is twelve bits shifted left by 0, 12 or 24, pushed or
\ added.  A wider number is its high part shifted left 36 and the
\ rest added; a negative one is its complement and an invert.

: chunks ( u36 push? -- ) \ the chunks that are not zero, highest first
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
    if h# 8000 tcode, then                  \ every chunk was zero: push 0
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
\ A run with a number in it is given to the machine whole
\ (machine-lit), as tokens: an ALU instruction, a push of a
\ number, an add of one, or a variable's instruction.  A run is
\ up to four tokens and the one being compiled, which may be a
\ conditional branch: that stays.

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

\ the token that ends at p: where it starts, its kind (0 none,
\ 1 ALU, 2 a push, 3 an add, 4 a variable's) and what it is.
\ A push is a number: the push and the adds after it, or, for a
\ negative one, a push, adds, invert and adds.
: tok ( p -- p' kind val )
    dup >r adds                                     ( q sum )
    over push? if
        swap 2 - tuck t@ lval +  2 swap  rdrop exit
    then
    over 2 >= if over 2 - t@ h# 6480 = else false then if
        over 2 - adds                               ( q sum q2 sum2 )
        over push? if                               \ a negative number
            swap 2 - tuck t@ lval +  invert         ( q sum q3 v )
            rot + rot drop  2 swap  rdrop exit
        then
        2drop
    then
    over r@ <> if 3 swap  rdrop exit then           \ adds alone
    drop rdrop
    dup 2 >= if dup 2 - t@ h# b000 and h# b000 = else false then if
        2 -  dup t@  4 swap exit
    then
    dup 2 >= if dup 2 - t@ alu-tok? else false then if
        2 -  dup t@  1 swap exit
    then
    0 0
;

create tk-at   4 cells allot    \ of each token found: where it starts,
create tk-kind 4 cells allot    \ its kind,
create tk-val  4 cells allot    \ and what it is
variable tk-n                   \ how many were found
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
variable fl-trig    \ that one: 0 ALU, 1 an add, 2 a conditional branch, 3 a variable's
variable fl-insn    \ and the instruction

#32 constant fl-room
create fl-run fl-room 2* allot                  \ the run, as the machine is given it,
create fl-out fl-room 2* allot                  \ and what the machine gives for it
variable fl-run#

: fl-take ( -- ) \ the run, into fl-run
    0 fl-run# !
    tcp @  tk-at fl-n @ 1- cells + @  ?do
        i t@  fl-run fl-run# @ 2* + w!  1 fl-run# +!
    2 +loop
    fl-trig @ 2 <> if  fl-insn @  fl-run fl-run# @ 2* + w!  1 fl-run# +!  then
;

: fl-plain? ( -- f ) \ is the run only ALU instructions?
    fl-trig @ if false exit then
    fl-n @ 0 do  tk-kind i cells + @ 1 <> if false unloop exit then  loop
    true
;

\ The run is the fl-n tokens before tcp and the instruction
\ being compiled.  If the machine gives a shorter run for it, the
\ tokens are taken back and that run is compiled: true.
: fl-try ( -- f )
    fl-plain? if false exit then
    tcp @  tk-at fl-n @ 1- cells + @  -  2/         ( instructions before )
    dup fusable? 0=  over fl-room 2 - > or  if drop false exit then
    fl-take
    fl-run fl-run# @  fl-trig @ 2 = 1 and  fl-out  machine-lit     ( before m )
    dup 0< if 2drop false exit then
    swap retract
    dup 0 ?do  dup 1- i - 2* fl-out + uw@  swap  loop       ( the last ... the first m )
    0 ?do tcode, loop       \ which may come back here
    true
;

\ fuse-lit: insn is an ALU instruction, an add, a variable's or
\ a conditional branch.  A push before it that has a shorter form
\ is made that.  If not, the longest run of tokens before it
\ that the machine shortens is replaced (fl-try).
:noname ( insn -- insn false | true )
    dup fl-insn !
    dup h# e000 and h# 6000 = if 0 fl-trig ! else
    dup h# b000 and h# b000 = if 3 fl-trig ! else
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
            dup h# e000 and h# 2000 = if tcode, else drop then
            true exit
        then
        1-
    repeat
    drop false
; is fuse-lit

( Defining words for target                  JCB 19:04 05/02/12)

: codeptr   tcp @ 2/ ;  \ target code pointer as a word address

( Code pages                                                          )

\ A branch or a call has 13 bits for where it goes.  Bit 12
\ clear: the instruction of that number in page 0.  Bit 12 set:
\ the low 12 bits are an instruction of the branch's own page.

: tpage ( word-addr -- page ) 12 rshift ;
: >target ( word-addr -- field ) dup tpage if $fff and $1000 or then ;
: here-target ( -- field ) tcp @ 2/ >target ;
: near? ( word-addr -- f ) \ kernel, or the page being compiled?
    tpage dup 0= swap tcp @ 2/ tpage = or ;

: wordstr ( "name" -- c-addr u )
    >in @ >r bl word count r> >in !
;

variable link 0 link !

\ A dictionary header is cells:
\     link    address of the previous header, or 0
\     xt      of the code; the top bit is set if immediate
\     name    codes of five bits, packed from a cell's low bits
\             up; the top bit of the name's last cell is set
\ A code: 1-26 is a letter (case is not kept); 27-29 is one of
\ - ! ? (n-direct); 30 means the next code is a character of
\ n-page1, and 31 that it is one of n-page2 (basewords.fs).  A
\ character that has no code is packed as 29.

: tcell, ( x -- ) there tcell!  cell-bytes tdp +! ;

cell-bits 5 / constant name-per             \ codes to a cell
name-per 5 * constant name-bits             \ and the bits they are

\ a character's code; or 32 plus its code in n-page1, or 64 plus
\ its code in n-page2
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
variable nksh                               \ and the bit of the last cell the next code goes at

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
    nksh @ 0=  nk# @ 0<> and if -1 nk# +! then     \ the last code filled its cell
    1 cell-bits 1- lshift  nk nk# @ cells + +!
    1 nk# +!
;

: tname, ( c-addr u -- ) name-pack  nk# @ 0 ?do nk i cells + @ tcell, loop ;

\ the same codes as two tables for the machine, a byte an entry:
\ the code of each character, and the character of each code
: codes-of-chars, ( -- ) 128 0 do i 33 < i 127 = or if 29 else i n-code then tc, loop ;
: chars-of-codes, ( -- )
    0 tc,  [char] z 1+ [char] a do i tc, loop
    n-directs 0 do n-direct i + c@ tc, loop  0 tc, 0 tc,
    0 tc,  n-page1s 0 do n-page1 i + c@ tc, loop
    0 tc,  n-page2s 0 do n-page2 i + c@ tc, loop  31 n-page2s - 0 do 0 tc, loop
;

\ A definition with no header gets a label: its name, and its xt
\ or, for data, its address with the top bit set.  Labels are
\ not in the image.  They go to the .tables file, where the
\ decompiler looks a name up.

variable llink      0 llink !
variable named      false named !           \ true if a header names the next definition,
variable named-cp                           \ which is here
true constant labelling

wordlist constant headers-wordlist          \ where each header is, by name (hdr,)
: note-header ( a "name" -- )
    wordstr nextname
    get-current >r  headers-wordlist set-current  create ,  r> set-current
;

: (header) ( immediate? "name" -- ) \ a header for the definition that starts here
    ?own-page
    label
    >hdr
    there >r
    tcalign
    there taddr  link @ tcell,  link !
    link @ note-header
    wordstr nextname
    get-current  said-wordlist set-current  create  set-current
    note-said
    tcp @ 2/ swap if 1 cell-bits 1- lshift or then tcell,
    bl parse tname,
    there r> - hdr-bytes +!
    hdr>
    true named !  tcp @ named-cp !
;

: named? ( -- f ) \ did a header name the definition that starts here?
    named @ tcp @ named-cp @ = and
    false named !
;

: wants-label? ( -- f ) named? 0= labelling and ;

\ The labels, kept in lflash for name-tables.  A label is: the
\ cell of lflash the label before starts at, or 0; its value;
\ and its name, packed as a header's.  Cell 0 is not used.
65536 allocate throw constant lflash
lflash 65536 erase
variable lp  cell-bytes lp !                \ where the next goes, in bytes

: lc,    ( c -- ) lflash lp @ + c!  1 lp +! ;
: lcell, ( x -- ) cell-bytes 0 do dup $ff and lc, 8 rshift loop drop ;
: lname, ( c-addr u -- ) name-pack  nk# @ 0 ?do nk i cells + @ lcell, loop ;

: (label) ( x "name" -- )
    lp @ byte-shift rshift  llink @ lcell,  llink !
    lcell,
    wordstr lname,
;

:: header       false (header) ;
:: header-imm   true (header) ;

variable wordstart
variable lastdef    \ body of the most recent target colon definition
variable indef      \ true from : to ; (and not from :noname)

\ A small word: no more than four instructions, none a call or a
\ branch, that returns only at its end and leaves the return
\ stack alone below what it pushed.  m is how many instructions
\ it is inline: an exit that is an instruction of its own is not
\ counted.

4 constant inline-max

variable in-n                               \ how many instructions the word is
variable in-depth                           \ how many cells it has pushed on the return stack
: small? ( -- m+1 | 0 ) \ of the word just ended; 0 if it is not small
    tcp @ wordstart @ - 2/ in-n !
    in-n @ 1 inline-max 2 + within 0= if 0 exit then
    0 in-depth !
    in-n @ 0 do
        wordstart @ i 2* + t@                       ( insn )
        dup h# 8000 and 0= if                       \ not a literal:
            dup h# e000 and h# 6000 <> if drop 0 unloop exit then              \ a call or a branch
            dup h# 1000 and  i in-n @ 1- <>  and if drop 0 unloop exit then    \ a return before the end
            dup h# 0c00 and h# 0c00 =  in-depth @ 0=  and if drop 0 unloop exit then   \ reads R, having pushed none
            dup h# 1000 and 0= if
                dup 3 and  dup 1 = if 1 in-depth +! then    \ r+1
                dup 3 = if -1 in-depth +! then              \ r-1
                2 = if -2 in-depth +! then                  \ r-2
                in-depth @ 0< if drop 0 unloop exit then    \ pops what it did not push
            then
        then
        drop
    loop
    in-depth @ if 0 exit then                       \ leaves something on the return stack
    wordstart @ in-n @ 1- 2* + t@                   ( the last )
    dup h# 9000 and h# 1000 <> if drop 0 exit then  \ which does not return
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

\ a use of a small word, numbered ord: inlined at once if that
\ cannot be longer than a call, and if not, as a trial (pend)
: try-inline ( body ord -- )
    >r
    dup 2 cells + @ 1-                          ( body m )
    dup 2 <  over 0= kernel? 0= or  and if      \ no longer than the call
        drop inline,  1 inl-map r> + c!
        false stays !  exit
    then
    tcp @ cur-tcp0 !  tcp @ cur-save snap  true cur !
    over inline,
    false cur !
    drop  tcp @ cur-tcp0 @ - 2 > 0=
    stays @ or  false stays !
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

\ A word of the program is a word of gforth whose body is five
\ cells: its xt; true if it reads the instruction after a call
\ of it (inline-operand); m+1 if it is small, or 0 (small?); how
\ many instructions it is; and the baseword, if `callable` made
\ it.  Run, it compiles a call of the word, or the word inline.
\ Each use of a word has a number (call#), by which inl-map
\ records whether that use was inlined.
:: :
    hex
    there s>d
    <# bl hold # # # # #>
    lst @ write-file throw
    wordstr lst @ write-line throw

    ?own-page
    named? dup said-header !
    0= labelling and if tcp @ 2/ (label) then
    tcp @ wordstart !
    label
    create  here lastdef !  note-def  codeptr , 0 , 0 , 0 , 0 ,
    true indef !
    does>   dup name-used
            dup 4 cells + @ ?dup if             \ callable: in line outside the kernel
                kernel? 0= if
                    false stays !  nip execute exit
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
            stays @ abort" in-place is before a call of a word that is not small"
            drop call,
;

\ in-place: the small word called next is inlined, even where
\ that is longer than the call
:: in-place ( -- )
    true stays !
;

\ a call: one instruction if the word is on page 0 or this page;
\ if not, its xt pushed and a call of execute.  After a call of
\ an inline-operand word the next instruction is pinned.
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

\ the word just defined reads the instruction after a call of it
:: inline-operand
    true lastdef @ cell+ !
;

:: peephole-off
    false peephole !
;

:: :noname
    false indef !
;

:: ,
    tcalign
    there tcell!  cell-bytes tdp +!
;

:: c,
    tc,
;

\ Pixels are written with the leftmost in the highest bit.
: pixels ( byte -- byte' ) \ in the machine's order
    disp-left-low if
        0 swap 8 0 do dup 1 and rot 2* or swap 2/ loop drop
    then ;

\ the key map: 128 keys and then the keys shifted, a character each
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
:: pcell, ( byte -- ) tcalign pixels tcell, ;   \ eight pixels as a cell
:: prow, ( byte -- ) \ eight pixels, repeated across a cell
    tcalign
    pixels cell-bytes 0 do dup tc, loop drop
;
:: px-cells, ( n -- ) \ the cells that n runs of 64 pixels are

    #64 cell-bits / *
    tcalign
    there tcell!  cell-bytes tdp +!
;

\ A font is written as a strike (mono.fs).  ]font lays it in
\ another form, and replaces the last two of the strike's five
\ cells.  A lane is a field of 8 or 16 bits of a cell.  The form:
\   five cells: the height; the first character; how many
\     glyphs; in the low eight bits the cells a glyph's rows
\     take, and above them 3 if a row is a lane of 8 bits or 4
\     if of 16; and the address of the rest
\   the rest: a cell with 1 in the lowest bit of each row lane;
\     for each glyph a lane of sixteen bits (its width in bits
\     0-4, its first row that has black in bits 5-9, the row
\     after its last in bits 10-15); and each glyph's rows, a
\     lane a row, the glyph at the left of the lane and the
\     first row in the lowest lane of a cell
: tcell@ ( a -- x ) 0  cell-bytes 0 do over i + tc@ i 8 * lshift or loop  nip ;
cell-bits #16 / constant lanes              \ lanes of sixteen bits in a cell
create fstrike 512 cells allot              \ the strike: runs of 64 pixels, a row after a row
variable fstrike#
create fstarts 257 cells allot              \ where each glyph starts in it, and where the last ends
variable font-rec                           \ where the font's record is
variable in-font    false in-font !
variable fh         variable fn             \ its height, and how many glyphs
variable fruns                              \ runs of 64 pixels in a row of the strike
variable flb                                \ the bits of a lane that is a row of it

: fpixel ( x y -- bit )
    fruns @ * over 6 rshift + cells fstrike + @
    swap 63 and 63 swap - rshift 1 and
;
: fstart ( g -- x0 ) cells fstarts + @ ;
: fwide ( g -- w ) dup 1+ fstart swap fstart - ;
: frow ( g r -- pixels ) \ a row of a glyph, at the left of a lane
    { g r }
    0  g fwide 0 ?do
        g fstart i + r fpixel
        disp-left-low if i else flb @ 1- i - then lshift or
    loop
;
: finfo ( g -- x ) \ a glyph's lane of sixteen bits
    { g }
    0 0 { first last }
    fh @ 0 ?do
        g i frow if  last 0= if i to first then  i 1+ to last  then
    loop
    g fwide #16 > abort" a glyph of a font is wider than a lane, sixteen pixels"
    g fwide  first 5 lshift or  last #10 lshift or
;
: fcell! ( x cell -- ) byte-shift lshift tcell! ;
: per-lane ( n bits -- cells ) cell-bits swap / tuck + 1- swap / ;
: flanes ( xt n cell bits -- cell' ) \ n lanes, each what xt ( i -- x ) gives, laid as cells
    { xt n cell bits }
    cell-bits bits / { per }
    n bits per-lane 0 ?do
        0  per 0 do
            j per * i +  dup n < if xt execute i bits * lshift or else drop then
        loop
        cell i + fcell!
    loop
    cell n bits per-lane +
;
variable fg                                 \ the glyph whose rows are being laid
: fg-row ( r -- pixels ) fg @ swap frow ;
: fwidest ( -- w ) 0  fn @ 0 ?do i fwide max loop ;
: lane-ones ( bits -- x ) 0  cell-bits 0 do  1 i lshift or  over +loop  nip ;
:: font[ ( -- ) there font-rec !  0 fstrike# !  true in-font ! ;
:: ]font ( -- )
    false in-font !
    font-rec @ tcell@ fh !
    font-rec @ 2 cell-bytes * + tcell@ fn !
    fn @ #256 > abort" a font has more glyphs than there is room to lay"
    fstrike# @ fh @ / fruns !
    fn @ 1+ 0 do  font-rec @ 5 i + cell-bytes * + tcell@  fstarts i cells + !  loop
    fwidest 8 > 0=  lanes 1 > and if 8 else #16 then flb !
    lofty if thi @ else font-rec @ 5 cell-bytes * + then byte-shift rshift { data }
    fh @ flb @ per-lane  flb @ 8 = if 3 else 4 then 8 lshift or
    font-rec @ 3 cell-bytes * + tcell!
    data  font-rec @ 4 cell-bytes * + tcell!
    flb @ lane-ones data fcell!
    ['] finfo fn @ data 1+ #16 flanes               ( the rows go from here )
    fn @ 0 ?do
        i fg !  ['] fg-row fh @ rot flb @ flanes
    loop
    lofty if
        1+ -2 and cell-bytes * thi !                \ an even cell
        font-rec @ 5 cell-bytes * + tdp !
    else
        cell-bytes * tdp !
    then
;

: px-lay ( pixels -- ) \ 64 pixels, as the display's cells
    in-font @ if fstrike fstrike# @ cells + !  1 fstrike# +!  exit then
    tcalign
    8 cell-bytes / 0 do
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

\ picture[ ... ]picture: a row is the line after row|, # ink and
\ . none.  The piece after `beside` goes to the right of what is
\ drawn, row for row.  A row is a whole number of 64 pixels.
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
        pic-n @ 0 do
            0  64 0 do  2*  k pic-across * j + i + pic-px + c@ or  loop
            px-lay
        64 +loop
    loop
;
:: lefts, ( -- ) \ a cell for each count: that many pixels at its left
    tcalign
    cell-bits 1+ 0 do
        i cell-bits = if -1 else 1 i lshift 1- then
        disp-left-low 0= if cell-bits i - lshift then
        tcell,
    loop
;

\ ---- the decompiler's tables of names (.tables), for the disk:
\ words and labels by xt, and those that are a number by the
\ number.  A table is records in the order of their keys, none
\ across two blocks.  A record is a header's address, or a
\ label's place with the top bit set; the key; and a label's name.

8192 constant nt-room
create ntk nt-room cells allot              \ the keys,
create ntn nt-room cells allot              \ and where each one's name is
variable nt#  0 nt# !
1 cell-bits 1- lshift constant top-bit

: nt-add ( hdr label? key -- )
    dup cell-mask and over <> if drop 2drop exit then
    nt# @ nt-room = abort" there are more names than the tables have room for"
    ntk nt# @ cells + !
    if top-bit or then
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
: hdr-link ( a -- a' ) byte-shift lshift tflash + @ cell-mask and ;
: hdr-val  ( a -- x )  byte-shift lshift cell-bytes + tflash + @ cell-mask and ;
: hdr-xt   ( a -- xt ) hdr-val  1 cell-bits 1- lshift invert and  $ffff and ;
: lbl-cell ( n -- x )  byte-shift lshift lflash + @ cell-mask and ;
: lbl-link ( n -- n' ) lbl-cell ;
: lbl-val  ( n -- x )  1+ lbl-cell ;
: lbl-data? ( x -- f ) 1 cell-bits 1- lshift and 0<> ;

\ The file: a block that says where the rest is (name-tables),
\ then blocks that are how many records each has and the records.
disk-block cell-bytes / constant nt-blk     \ cells in a block of the disk
create ntf nt-room cells allot
variable ntf#
: ntf, ( x -- )
    ntf# @ nt-room = abort" the file of names is longer than there is room for"
    ntf ntf# @ cells + !  1 ntf# +!
;
create nti nt-blk cells allot               \ the first key of each block of the runs
variable nti#
variable nt-at                              \ the cell the block being filled starts at

: nt-key  ( i -- key ) cells ntk + @ ;
: nt-name ( i -- x )   cells ntn + @ ;
: lbl-name ( x -- n ) top-bit invert and 2 + ;
: name-cells ( n -- m ) \ of the name from cell n of the labels
    0 begin 2dup + lbl-cell top-bit and 0= while 1+ repeat 1+ nip
;
: rec-cells ( i -- n ) \ of the i'th entry's record
    nt-name dup top-bit and if lbl-name name-cells 2 + else drop 2 then
;
: key-cells ( i -- n ) \ of the records from the i'th that have its key
    dup nt-key >r  0 swap
    begin dup nt# @ < if dup nt-key r@ = else false then while
        tuck rec-cells + swap 1+
    repeat
    drop rdrop
;
: nt-block ( key -- ) \ the next block is begun, by a record of this key
    nti# @ 3 + nt-blk = abort" the first keys of the blocks of names do not fit a block"
    nti nti# @ cells + !  1 nti# +!
    ntf# @ nt-blk + 1- nt-blk / nt-blk * ntf# !
    ntf# @ nt-at !  0 ntf,
;
: rec, ( i -- )
    dup nt-name dup ntf,  swap nt-key ntf,
    dup top-bit and if
        lbl-name begin dup lbl-cell dup ntf, top-bit and 0= while 1+ repeat
    then
    drop
    1 nt-at @ cells ntf + +!
;
: nt-lay ( -- ) \ a table into the file; a key's records in one block
    nt-sort
    nt# @ 0 ?do
        i if i nt-key i 1- nt-key <> else true then if
            i 0=  ntf# @ nt-at @ -  i key-cells +  nt-blk >  or if
                i nt-key nt-block
                i key-cells 1+ nt-blk > abort" the records of a key are more than a block"
            then
        then
        i rec,
    loop
    0 nt# !
;

: tlv ( w -- n ) dup $fff and swap 12 rshift 3 and 12 * lshift ;
: lit? ( w kind -- f ) over $c000 and =  swap $3000 and $3000 <> and ;
: thval ( xt -- n true | false ) \ the number, if the code only pushes one
    2*  dup t@ dup $8000 lit? 0= if 2drop false exit then
    tlv swap                                        ( n a )
    begin
        2 + dup t@ dup $c000 lit?
    while
        tlv rot + swap
    repeat
    nip $7403 = if true else drop false then
;

: xt-table ( -- ) \ words with a header, and labels of code, by xt
    link @ begin dup while dup false over hdr-xt nt-add  hdr-link repeat drop
    llink @ begin
        dup
    while
        dup lbl-val lbl-data? 0= if dup true over lbl-val $ffff and nt-add then
        lbl-link
    repeat drop
    nt-lay
;
\ words that only push a number, by the number; and labels of
\ data, by the address
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

:: ," \ a counted string
    [char] " parse dup tc,
    bounds ?do i c@ tc, loop
;

:: allot
    0 ?do
        0 tc,
    loop
;

: shortcut ( orig -- f ) \ a call before ; becomes a jump
    dup t@ h# e000 and h# 4000 = if
        dup t@ h# 1fff and over tw!
        true
    else
        false
    then
    nip
;

\ the end of a definition: an exit, unless the last instruction
\ is a jump, or a call, which is made a jump, and no branch
\ lands after it
: end-word ( -- )
    tcp @ wordstart @ = if
        s" exit" evaluate
    else
        tcp @ 2 - shortcut
        \ a jump written as one is the end too, unless it is pinned
        tcp @ 2 - t@ h# e000 and 0=  tpinned @ tcp @ 2 - <> and  or
        tcp @ 0 do
            i tbranches @ tcp @ = if
                i tbranches @ shortcut and
            then
        loop
        0= if
            s" exit" evaluate
        then
    then
;

\ a call before an exit is made a jump, unless something lands
\ between the two or the exit is pinned
: exit-here ( -- )
    tpinned @ tcp @ <>  tcp @ wordstart @ <>  and if
        1 fusable? if  tcp @ 2 - shortcut if exit then  then
    then
    h# 7403 tcode,
;
\ With a word waiting, the exit goes to the peephole first, which
\ settles the word.  If the exit is left an instruction of its
\ own, it is taken back and exit-here lays it.  The same for ;
:: exit
    pend @ if
        true ending !  h# 7403 tcode,  false ending !
        tcp @ 2 - t@ h# 7403 = if 1 retract exit-here then
    else
        exit-here
    then
;

:: ;
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

\ ;fallthru ends a definition with no exit: it runs on into the
\ definition after it.
:: ;fallthru ;

\ header X callable X: the baseword X as a word of the kernel as
\ well.  In the kernel a use of X is that word, called or
\ inlined as any small word is.  On other pages X is the
\ baseword, inline.
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
    ' >body dup name-used @ >target ubranch
;

:: constant
    create  false said-header !  note-def  ,
    does>   dup name-used @ literal
;

\ A table of instructions is laid among the code, where i@ reads
\ it, all on one page.
variable tabled  -1 tabled !            \ the page the last table is on
: table-room ( n -- ) \ before a table of n instructions
    settle
    2* tcp @ $1fff and + $1ffe u> if next-page then
    tcp @ $2000 / tabled !
;
: (i,) ( w -- )
    settle
    tcp @ $1fff and $1ffe = abort" a table of instructions has run off its page"
    tcp @ tw!  2 tcp +!
    label
;
:: i, ( w -- ) (i,) ;
: is, ( w n -- ) 0 ?do dup (i,) loop drop ;

\ With a header before it, code is laid that pushes the address.
:: create
    tcalign
    named? dup said-header !  dup >r 0= labelling and if
        there taddr 1 cell-bits 1- lshift or (label)
    then
    r> if
        there taddr literal  s" exit" evaluate  label
    then
    create  note-def  there taddr ,
    does>   dup name-used @ literal
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

:: c# base @ >r decimal 0. bl parse >number 2drop drop r> base ! literal ;
:: d# bl parse 10 base>number ;
:: h# bl parse 16 base>number ;
:: ['] ' >body dup name-used @ literal ;

\ n at: data name  is the address n cells past data, in line
:: at: ( n "data" "name" -- )
    ' >body dup name-used @ +  create  false said-header !  note-def  ,
    does> dup name-used @ literal ;
:: [char] char literal ;

( The machine's sizes, for the target program )

:: CELL       1 literal ;                       \ from one cell's address to the next
:: CELL2      2 literal ;                       \ offset of a header's name
:: CELLB      cell-bytes literal ;              \ bytes in a cell
:: BYTESHIFT  byte-shift literal ;              \ and the shift that multiplies by that
:: ALIGN-MASK cell-bytes negate literal ;       \ of a byte address: the start of its cell
:: OFFSET-MASK cell-bytes 1- literal ;          \ of a byte address: where in its cell
:: (>c)       byte-shift literal s" lshift" evaluate ;  \ a cell's byte address
:: (c>)       byte-shift literal s" rshift" evaluate ;  \ the cell a byte is in
:: TOPBIT     cell-bits 1- literal ;            \ index of the sign bit
:: HALFBITS   cell-bits 2/ literal ;
:: NAME-PER   name-per literal ;                \ codes of a name in a cell,
:: NAME-BITS  name-bits literal ;               \ and the bits they are,
:: NAME-SPARE cell-bits name-bits - literal ;   \ and the bits of a cell that are not theirs
:: codes-of-chars, codes-of-chars, ;            \ the two tables of a name's codes
:: chars-of-codes, chars-of-codes, ;
:: TOP16      cell-bits #16 - literal ;         \ shift that puts 16 bits at the top
:: (i@)  cell-bits #16 = if s" @" else s" 2* uw@" then evaluate ;
:: (i!)  cell-bits #16 = if s" !" else s" 2* w!" then evaluate ;

( The display, for display.fs )

:: RAW>SRC        disp-ink-black 0= if s" invert" evaluate then ;   \ a set bit is black
:: PXCELL         cell-bits literal ;           \ pixels in a cell
:: CELLBITS       cell-bits literal ;
:: LANE1          0 cell-bits 16 / 0 do 16 lshift 1 or loop literal ;   \ 1 in each lane of 16 bits
:: LANES          lanes literal ;               \ how many lanes that is,
:: LANESHIFT                                    \ and the shift that divides by it
    lanes 4 = if 2 else lanes 2 = if 1 else 0 then then literal ;
:: PXMASK         cell-bits 1- literal ;        \ and: how far into its cell a pixel is
:: PXSHIFT        byte-shift 3 + literal ;      \ rshift: which cell of its row
:: DISP-BASE      disp-base literal ;
:: DISP-STRIDE    disp-stride literal ;
:: DISP-BANKS     disp-banks literal ;
:: DISP-BANK-STEP disp-bank-step literal ;
:: BANKED?    disp-banks 1 > ;
:: INK-BLACK? disp-ink-black 0<> ;
:: LEFT-LOW?  disp-left-low 0<> ;
:: PIXR   disp-left-low if s" lshift" else s" rshift" then evaluate ;  \ pixels to the right
:: PIXL   disp-left-low if s" rshift" else s" lshift" then evaluate ;
:: EDGE   disp-left-low 0= if cell-bits 8 - literal s" lshift" evaluate then ; \ a byte to a cell's left
:: LEFT8  disp-left-low if $ff literal s" and" evaluate
          else cell-bits 8 - literal s" rshift" evaluate then ;
:: LEFT1  disp-left-low if 1 else 1 cell-bits 1- lshift then literal ;
:: M8     disp-left-low if $ff else $ff cell-bits 8 - lshift then literal ;
:: VARS       var-base literal ;                \ the first cell one instruction can fetch
:: forth      var-base literal ;                \ which holds the newest header's address
:: HDR-BASE   hdr-base literal ;

( The disk )

:: LINE-CHARS  disk-line literal ;
:: BLOCK-LINES disk-block disk-line / literal ;
:: LINE-SHIFT  0 begin 1 over lshift disk-line < while 1+ repeat literal ;
:: LAST-COLUMN disk-line 1- literal ;
:: LAST-LINE   disk-block disk-line / 1- literal ;
:: BLOCK-CHARS disk-block literal ;
:: BLKCELLS    disk-block cell-bytes / literal ;
:: LINE-ROUND  disk-line 2 - literal ;          \ added to >in, which is two past a \,
:: LINE-MASK   disk-line negate literal ;       \ and this kept: where its line ends
:: NAMES-BLK   disk-names-block literal ;       \ what is where on the disk: vm_constants.h
:: CANVAS-BLK  disk-canvas-block literal ;
:: MASK64-BLK  disk-masks-block literal ;       \ the threshold masks: blue noise of 64 by 64,
:: MASK32-BLK  disk-mask32-block literal ;      \ of 32 by 32,
:: BAYER-BLK   disk-bayer-block literal ;       \ and Bayer's of 8 by 8
:: VIEW-BLK    disk-view-block literal ;        \ the viewer's bitmap,
:: VIEW-BLKS   disk-view-blocks literal ;
:: GRAY-BLK    disk-gray-block literal ;        \ the viewer's picture kept as gray,
:: GRAY-BLKS   disk-gray-blocks literal ;
:: OWN-BLKS    disk-own-blocks literal ;        \ for the names of words a boot compiles
:: SOURCE-BLK  disk-source-block literal ;      \ what a machine started from its ROM loads
:: DISK-SHIFT  disk-block-bits literal ;        \ a block's number, rshift: its disk
:: DRIVES      disk-drives literal ;
\ the index, block 0 (doc/disk.md): a number of an entry is two bytes
:: ENTRY-BYTES  disk-entry literal ;
:: ENTRIES      disk-block disk-entry / literal ;
:: ENTRY-TYPE   disk-entry-type literal ;
:: ENTRY-FIRST  disk-entry-first literal ;
:: ENTRY-BLOCKS disk-entry-blocks literal ;
:: ENTRY-WHERE  disk-entry-where literal ;
:: TYPE-BINARY  disk-binary literal ;
:: TYPE-SOURCE  disk-source literal ;
:: TYPE-INDEX   disk-index literal ;
\ a row of a mask on the disk: its planes, the lowest first
:: MASK-WIDTH  disk-mask-width literal ;        \ pixels across,
:: MASK-CELLS  disk-mask-width cell-bits / literal ;    \ which is the cells of a plane's row;
:: MASK-ROW    disk-mask-width cell-bits / disk-mask-planes * literal ; \ the cells of a row
:: MASK-ROWS-SHIFT                              \ 1, lshift: the rows in a block
    disk-block 8 * disk-mask-width / disk-mask-planes /
    0 begin 2dup 1 swap lshift > while 1+ repeat nip literal ;
:: DATA-LIMIT data-limit literal ;
\ the cell after the last that is anyone's to take
:: MEM-END    1 addr-bits lshift top-kept @ - cell-mask and literal ;
:: CODE-BASE  code-base literal ;
:: CODE-SIZE  code-size literal ;
:: DISPLAY? disp-w 0<> ;
:: W64?   cell-bits #64 = ;
:: W32?   cell-bits #32 = ;
:: W32+?  cell-bits #32 >= ;

s" BOOT" getenv s" on" compare 0= constant booting  \ BOOT=on: the image without apps.fs
:: BOOTING? booting ;

\ a baseword as a word with a header of its name, if there is none
:: bw-header ( "name" -- )
    bl word count  2dup headers-wordlist search-wordlist if drop 2drop exit then
    s" header " pad place  2dup pad +place  s"  : bw-" pad +place  2dup pad +place
    s"  " pad +place  pad +place  s"  ;" pad +place  pad count evaluate
;
\ and one of the machine's sizes
:: bw-size ( "name" -- )
    bl word count  2dup headers-wordlist search-wordlist if drop 2drop exit then
    s" header " pad place  2dup pad +place  s"  : bw-" pad +place  2dup pad +place
    s"  HOST " pad +place  pad +place  s"  ;" pad +place  pad count evaluate
;

:: HOST ( "name" -- ) \ what that word of this file's is, as a number
    bl word count forth-wordlist search-wordlist 0= abort" HOST: there is no such word here"
    execute literal ;

:: HDR-LIMIT code-base 2* literal ;     \ where headers made at run time stop: a byte

:: asm-0branch
    ' >body @
    0branch
;

( Conditionals                               JCB 13:12 09/03/10)

: resolve ( orig -- ) \ the branch at orig lands here
    label
    tcp @ over tbranches !
    dup t@ here-target or swap tw!
;

:: if
    settle
    0 0branch
    tcp @ 2 -
;

\ DOUBLE is a call of the instruction after it: the code from
\ there to its exit is run twice, once called and once more
\ when that returns.
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
\ repeat rotates a loop whose test is the one instruction xor or
\ =.  Instead of a jump back to the test, the test is compiled
\ again after the body, inverted, with a conditional branch back
\ to the body.  Each pass of the loop runs one instruction
\ fewer, and the loop is one instruction longer.  inl-map
\ records which loops were rotated, for the plain build.
: turned ( insn -- insn' true | false ) \ xor made =, or = made xor
    dup h# f300 and h# 6000 <> if drop false exit then
    dup h# f0 and dup h# 60 = swap h# 90 = or 0= if drop false exit then
    h# f0 xor true
;
: turned-again ( dest orig -- ) \ the test again, its last instruction inverted
    2 - { dest last }
    last dest ?do i t@ tcode, 2 +loop
    last t@ turned 0= abort" a loop is turned with the peephole on, and with it off its test does not end in xor or ="
    tcode,
;
:: repeat ( dest orig -- )
    call# @  1 call# +!  { dest orig ord }
    plain @ if
        inl-map ord + c@ if
            dest orig turned-again
            orig 2 + 2/ >target 0branch
            orig resolve exit
        then
    else
        dest 2 + orig = if
            dest t@ turned if
                tcode,
                orig 2 + 2/ >target 0branch
                orig resolve
                1 inl-map ord + c!  exit
            then
        then
    then
    dest 2/ >target ubranch
    orig resolve
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
\ what is made goes to ../build/, or the directory BUILD names
: build-dir ( -- c-addr u ) s" BUILD" getenv dup 0= if 2drop s" ../build/" then ;
: create-output-file w/o create-file throw ;
: out-name ( s -- c-addr u ) \ the file with suffix s
    >str
    prefix. +str
    build-dir >str +str str@
;
: out-suffix ( s -- h ) \ Create an output file h with suffix s
    out-name create-output-file
;
\ .tables' first block: a stamp, which the image has too; how
\ many blocks the table by xt is, and the one by number; and the
\ first key of each of those blocks.
create ntb 8 allot
: cell>file ( x fid -- ) \ a cell of the machine's, its lowest byte first
    swap cell-bytes 0 do dup $ff and ntb i + c!  8 rshift loop drop
    ntb cell-bytes rot write-file throw
;
\ forth0 and stamp are two variables of the image: the first is
\ set to the newest header, the second to the stamp
: name-tables ( forth0 stamp -- )
    link @  tcp @ 16 lshift xor  lp @ 7 lshift xor  cell-mask and   ( forth0 stamp x )
    dup rot tvar!                                       ( forth0 x )
    link @ rot tvar!                                    ( x )
    ntf nt-room cells erase  nt-blk ntf# !  0 nti# !
    xt-table  nti# @  number-table                      ( x blocks )
    ntf# @ >r  0 ntf# !
    swap ntf,  dup ntf,  nti# @ swap - ntf,
    nti# @ 0 ?do nti i cells + @ ntf, loop
    r> ntf# !
    s" tables" out-suffix
    ntf# @ 0 ?do ntf i cells + @ over cell>file loop
    close-file throw
;

:noname
    s" lst" out-suffix lst !
    s" sizes" out-suffix sizes !
; execute

\ .inline: inl-map, read by a plain build
: inline-name ( -- c-addr u ) s" inline" out-name ;
plain @ [if]
    inline-name r/o open-file 0= [if]
        dup inl-map 65536 rot read-file throw drop  close-file throw
    [else]
        drop
    [then]
[then]


1 file-now !  1 file-open cell+ !
target included                         \ include the program.fs

[ tcp @ 0 org ]
\ the image's first two instructions: calls of main and of quit
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
: dumpall.32 \ the image as the .hex: 32 bits a line, in hex
    s" hex" out-suffix to file

    hex
    32768 0 do
        tflash i 4 * + ul@
        s>d <# # # # # # # # # #> file write-line throw
    loop
    file close-file
;

s" (all)" code-top  tdp @ data-base -  thp @ hdr-base - +  tbp @ bulk-base - +  hdr-bytes @ lbl-bytes @ 0 0 size-line
in-bulk @ [if] cr .( bulk with no unbulk) abort [then]
near-deep @ [if] cr .( nearby with no nearby-end) abort [then]
tdp @ data-limit > [if] cr .( the data has run into what is after it) abort [then]
tbp @ hdr-base >  bulky and [if] cr .( the bulk has run into the dictionary headers) abort [then]
thi @ 131072 >  lofty and [if] cr .( what is laid down above the display has run off the end of memory) abort [then]
thp @ code-base 2* > [if] cr .( the dictionary headers have run into the code) abort [then]
code-top code-base code-size + 2* > [if] cr .( the code has run past where code can be) abort [then]
sizes @ close-file throw

\ .public: the names with a header, by file; then those with none
\ that another file uses
: file-name ( n -- a u ) 2* cells file-names + 2@ ;
: headers-said ( file f -- ) \ eight to a line
    { file f }
    0
    saids# @ 0 ?do
        i said-at @ file = if
            dup 0= if s"  " f write-file throw then
            s"  " f write-file throw  i said-at cell+ 2@ f write-file throw
            1+ dup 8 = if drop 0  s" " f write-line throw then
        then
    loop
    if s" " f write-line throw then
;
: public-written ( -- )
    s" public" out-suffix { f }
    decimal
    s" The names that have a header, by the file that defines them." f write-line throw
    files# @ 0 ?do
        0  saids# @ 0 ?do i said-at @ j = if 1+ then loop
        ?dup if
            i file-name f write-file throw  s" : " f write-file throw
            s>d <# #s #> f write-line throw
            i f headers-said
        then
    loop
    s" " f write-line throw
    s" The names that have none and that another file uses: the name, its file, the file that" f write-line throw
    s" uses it, how often, and whether its own file was still being read (inside) or was done (after)." f write-line throw
    uses# @ 0 ?do
        i use-at @ 3 cells + 2@ f write-file throw       s"  " f write-file throw
        i use-at @ cell+ @ file-name f write-file throw   s"  " f write-file throw
        i use-at cell+ @ file-name f write-file throw     s"  " f write-file throw
        i use-at 2 cells + @ s>d <# #s #> f write-file throw
        i use-at 3 cells + @ if s"  inside" else s"  after" then f write-line throw
    loop
    f close-file throw
    0 defs# @ 0 ?do i def-at 2 cells + @ 0= if 1+ then loop       ( with no header )
    0 uses# @ 0 ?do
        true  i 0 ?do  j use-at @ i use-at @ = if drop false then  loop
        if 1+ then
    loop
    cr ." words, tables, constants and macros with no header: " swap .
    ." ; used by another file: " .
;
public-written

\ .needs: for each file, the files whose words it uses; and then
\ those and every file they need in turn
: needed-said ( user f -- )
    { user f }
    files# @ 0 ?do
        user i needs-at c@  user i <> and if  s"  " f write-file throw  i file-name f write-file throw  then
    loop
    s" " f write-line throw
;
: needs-written ( -- )
    s" needs" out-suffix { f }
    files# @ 0 ?do
        i file-name f write-file throw  s"  needs" f write-file throw  i f needed-said
    loop
    files# @ 0 ?do  files# @ 0 ?do  files# @ 0 ?do
        j k needs-at c@  k i needs-at c@  and if 1 j i needs-at c! then
    loop loop loop
    files# @ 0 ?do
        i file-name f write-file throw  s"  needs in all" f write-file throw  i f needed-said
    loop
    f close-file throw
;
needs-written

link @ var-base tvar!
plain @ 0= [if]
    inline-name create-output-file  dup inl-map call# @ rot write-file throw  close-file throw
[then]
dumpall.32
." tdp " tdp @ . ." thp " thp @ .
." tcp " tcp @ .

bye
