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
131072 allocate throw constant _tbranches   \ branch targets, cells
tflash      131072 erase
_tbranches  131072 erase
: tbranches cells _tbranches + ;

variable tdp    $4000 tdp !     \ Data pointer
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
: tcode,    tcp @ tw! tcell tcp +! ;
: org       tcp ! ;

wordlist constant target-wordlist
: add-order ( wid -- ) >r get-order r> swap 1+ set-order ;
: :: get-current >r target-wordlist set-current : r> set-current ;

next-arg included       \ include the machine.fs

( Language basics for target                 JCB 19:08 05/02/12)

warnings off
:: ( postpone ( ;
:: \ postpone \ ;

:: org          org ;
:: include      include ;
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
\            BYTE    lit_shifts: 2;  // shifts:      {imm<<12,imm<<24,imm<<36}
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

\ Variables for tracking literal encoding state
variable literal-magnitude   0 ,      \ Tracks how many 12-bit shifts needed (0-3)
variable initializer-lit?    true ,   \ True if this is the first literal instruction

: incr-lit-magnitude
  literal-magnitude @ 1 + literal-magnitude !
;

: decr-lit-magnitude
  literal-magnitude @ 1 - literal-magnitude !
;

: add-imm+-flag
  initializer-lit? @ if        \ if we're the first literal written out,
    false initializer-lit? !   \ set the flag to false
  ELSE
    imm+                       \ non-initializers accumulate into the last lit
  THEN
;

: set-lit-magnitude
  \ Convert magnitude counter (0-3) to shift encoding bits:
  \ 0 -> 0x0000 (no shift)
  \ 1 -> 0x1000 (shift 12, bits 12-13 = 01)
  \ 2 -> 0x2000 (shift 24, bits 12-13 = 10)
  \ 3 -> 0x3000 (shift 36, bits 12-13 = 11)
  literal-magnitude @ dup if
    12 lshift or               \ move magnitude to bits 12-13
  else
    drop
  then
;

: literal-write? ( value -- value flag )
  \ Decide whether to write this chunk
  \ Write if: value is non-zero OR (mag=0 AND init=true)
  dup 0<>                      \ value non-zero?
  literal-magnitude @ 0=       \ mag=0?
  initializer-lit? @ and       \ mag=0 AND init?
  or                           \ write if non-zero OR (mag=0 AND init)
;

: should-write-chunk? ( chunk init mag -- chunk flag )
    \ Check if chunk should be written (non-zero OR (mag=0 AND init))
    rot dup >r          \ chunk init mag | R: chunk
    rot                 \ mag chunk init | R: chunk  
    swap 0<>            \ mag init chunk<>0 | R: chunk
    rot 0=              \ init chunk<>0 mag=0 | R: chunk
    rot and             \ chunk<>0 mag=0&init | R: chunk
    or                  \ flag | R: chunk
    r> swap             \ chunk flag
;

: write-chunk ( chunk mag init -- init' )
    \ Write a chunk with given magnitude, return updated init flag
    rot swap                     \ chunk init mag
    over $fff and                \ chunk init mag chunk&fff
    swap dup if                  \ chunk init chunk&fff mag
        12 lshift or             \ chunk init encoded
    else
        drop                     \ chunk init chunk&fff
    then
    nip                          \ init encoded
    
    \ Add imm+ flag if not first chunk
    over 0= if
        imm+                     \ add accumulate flag
    then
    
    h# 8000 or tcode,            \ emit instruction
    drop false                   \ return false (not init anymore)
;

: _literal ( n -- )
    \ Recursively encode a literal value into 12-bit chunks
    \ Processes from MSB to LSB, writing instructions in reverse order
    dup 0 < if
        invert recurse            \ Handle negative: convert to positive
        ~T alu                    \ Then add invert instruction at end
    else
        dup $fffffffffffff000 and if      \ Check if value needs > 12 bits (64-bit mask)
          dup 12 rshift           \ Get upper bits (shift right by 12)
          incr-lit-magnitude      \ Track that we need a bigger shift
          recurse                 \ Process upper bits first (MSB to LSB)
          $fff and                \ Mask current value to lower 12 bits
          recurse                 \ Process lower 12 bits
        else
            \ Base case: value fits in 12 bits
            literal-write? if     \ Check if should write
              $fff and            \ Mask to 12 bits
              ." Writing chunk " dup hex . ." at mag " literal-magnitude @ . decimal
              set-lit-magnitude   \ Set correct magnitude bits
              ." after set-lit: " dup hex . decimal
              add-imm+-flag       \ Add imm+ flag if not first chunk
              ." after add-imm+: " dup hex . cr decimal
              h# 8000 or          \ Set literal flag
              ." final instruction: " dup hex . cr decimal  
              tcode,              \ Write to code memory
            else
              drop                \ Skip zero chunks in middle of literal
            then
            decr-lit-magnitude
         then
     then
;

: literal ( n -- )
  \ Main entry point for encoding literals up to 48 bits
  \ NOTE: Values > 48 bits not supported (would need special handling)
  0    literal-magnitude !        \ Reset shift counter
  true initializer-lit?  !        \ Mark as first literal instruction
  dup ." DEBUG: literal " hex . decimal cr
  _literal
  ." Final mag: " literal-magnitude @ . cr
;

( Defining words for target                  JCB 19:04 05/02/12)

: codeptr   tcp @ 2/ ;  \ target code pointer as a word address

: wordstr ( "name" -- c-addr u )
    >in @ >r bl word count r> >in !
;

variable link 0 link !

:: header
    twalign there
    \ cr ." link is " link @ .
    link @ tw,
    link !
    bl parse
    \ cr ." at " there . 2dup type tcp @ .
    dup tc,
    bounds do
        i c@ tc,
    loop
    twalign
    tcp @ tw,
;

:: header-imm
    twalign there
    link @ 1+ tw,
    link !
    bl parse
    dup tc,
    bounds do
        i c@ tc,
    loop
    twalign
    tcp @ tw,
;

variable wordstart

:: :
    hex
    there s>d
    <# bl hold # # # # #>
    lst @ write-file throw
    wordstr lst @ write-line throw

    there wordstart !
    0    literal-magnitude !        \ Reset shift counter
    true initializer-lit?  !        \ Mark as first literal instruction
    create  codeptr ,
    does>   @ scall

;

:: :noname
;

:: ,
    talign
    t,
;

:: c,
    tc,
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
    ' >body @ ubranch
;

:: constant
    create  ,
    does>   @ literal
;

:: create
    talign
    create there ,
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
            drop dup literal 32 rshift literal
        else
            -1 abort" bad number"
        then
    then ;
warnings on

:: d# bl parse 10 base>number ;
:: h# bl parse 16 base>number ;
:: ['] ' >body @ 2* literal ;
:: [char] char literal ;

:: asm-0branch
    ' >body @
    0branch
;

( Conditionals                               JCB 13:12 09/03/10)

: resolve ( orig -- )
    tcp @ over tbranches ! \ forward reference from orig to this loc
    dup t@ tcp @ 2/ or swap tw!
;

:: if
    tcp @
    0 0branch
;

:: DOUBLE
    tcp @ 2 + 2/ scall
;

:: then
    resolve
;

:: else
    tcp @
    0 ubranch 
    swap resolve
;

:: begin tcp @ ;

:: again ( dest -- )
    2/ ubranch
;
:: until
    2/ 0branch
;
:: while
    tcp @
    0 0branch
;
:: repeat
    swap 2/ ubranch
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
    8192 0 do
        tflash i 4 * + @
        s>d <# # # # # # # # # #> file write-line throw
    loop
    file close-file
;

dumpall.32
." tdp " tdp @ .
." tcp " tcp @ .

bye
