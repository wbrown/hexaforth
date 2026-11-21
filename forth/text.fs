\ text.fs - text in cells, a line at a time: put in, found and
\ drawn

\ A line starts where a cell does and ends with EOL.  INDENT is
\ three columns; FACE and three bits is how what comes after it
\ on the line looks.
meta
:: EOL         #10 literal ;
:: INDENT      #9 literal ;
:: INDENT-COLS 3 literal ;
:: FACE        $10 literal ;
:: F-BOLD      1 literal ;
:: F-LEAN      2 literal ;
:: F-UNDER     4 literal ;

\ a cell of spaces, and of the top bit of each character
$2020202020202020 cell-mask and constant spaces-cell
:: BLANKS spaces-cell literal ;
:: HIGHS  $8080808080808080 cell-mask and literal ;
target

header INDENT      : code-indent      INDENT ;
header INDENT-COLS : code-indent-cols INDENT-COLS ;
header FACE        : code-face        FACE ;
header F-BOLD      : code-bold        F-BOLD ;
header F-LEAN      : code-lean        F-LEAN ;
header F-UNDER     : code-under       F-UNDER ;

: ctl? ( x -- f ) \ is a character of the cell below a space?
    dup BLANKS - swap invert and HIGHS and ;

: >cb ( a i -- a' s ) \ character i's cell, and bits into it
    dup BYTESHIFT rshift rot +
    swap OFFSET-MASK and d# 3 lshift
;

: bits@ ( a s -- x ) \ a cell of what starts s bits into a's
    ?dup if
        >r  dup @ r@ rshift  swap 1+ @  CELLBITS r> - lshift
        or
        exit
    then
    @
;

\ x at character i of the cells from a, to the end of its cell
: put-cell ( x a i -- )
    >cb  rot over lshift  swap d# 1 swap lshift 1-
    rot dup >r @ and or  r> !
;

\ the cells from b up to e, from what starts s bits into a
: bits-move ( a s b e -- )
    begin 2dup xor while
        >r >r  2dup bits@ r@ !  swap 1+ swap  r> 1+ r>
    repeat
    2drop 2drop
;

\ u characters from character i of a's cells, to j of b's
: text-first ( a i b j -- ) >r >r >cb bits@ r> r> put-cell ;
: dst-cells ( b j u -- b1 e ) \ the cells after the first
    over + 1- BYTESHIFT rshift  swap BYTESHIFT rshift
    rot tuck + 1+ -rot + 1+ ;
: rest-of ( i b j u -- i' b1 e )
    >r  rot over OFFSET-MASK and - CELLB +  -rot
    r> dst-cells ;
: text-put ( a i u b j -- )
    rot ?dup 0= if 2drop 2drop exit then
    >r  2over 2over text-first  r> rest-of
    >r >r >cb r> r> bits-move
;

: code-at ( a i -- k ) \ the first code after character i
    over >r  >cb  d# 1 swap lshift 1- BLANKS and  over @ or
    begin dup ctl? 0= while drop 1+ dup @ repeat
    swap r> - BYTESHIFT lshift swap
    dup h# ff and d# 32 u< 0= if
        begin
            >r 1+ r> d# 8 rshift  dup h# ff and d# 32 u<
        until
    then
    drop
;

: line-end ( a -- k ) \ where the line that starts at a ends
    d# 0
    begin
        over swap code-at  2dup d# 3 lshift char@ EOL <>
    while
        1+
    repeat
    nip
;
: line-next ( a -- a' ) dup line-end BYTESHIFT rshift + 1+ ;

\ ---- a text

\ a text's record: its first cell,
: tb.lim  ( tb -- a ) 1+ ;       \ the cell after its last,
: tb.last ( tb -- a ) CELL2 + ;  \ where its last line starts,
header tb.len
: tb.len  ( tb -- a ) c# 3 + ;   \ the characters in that,
header tb.lines
: tb.lines ( tb -- a ) c# 4 + ;  \ how many lines it has,
: tb.roll ( tb -- a ) c# 5 + ;   \ and whether it rolls

: text-eol ( tb -- ) \ EOL after its last line's characters
    >r  EOL r@ tb.last @ r> tb.len @ put-cell ;

: text-after ( tb -- a ) \ the cell after its last line's EOL
    dup tb.last @ swap tb.len @ BYTESHIFT rshift + 1+ ;

header text-clear
: text-clear ( tb -- ) \ nothing in it but an empty line
    dup @ over tb.last !  d# 0 over tb.len !
    d# 1 over tb.lines !  text-eol
;

header text-new
: text-new ( a n roll? tb -- ) \ a text in the n cells from a
    >r  r@ tb.roll !  over + r@ tb.lim !  r@ !  r> text-clear ;

header text-line
: text-line ( n tb -- a ) \ where its nth line starts
    @ swap negate begin dup while swap line-next swap 1+ repeat
    drop
;

: text-free ( tb -- n ) dup tb.lim @ swap text-after - ;

: text-drop ( tb -- ) \ its first line out
    >r  r@ @ line-next r@ @ -                       ( d )
    r@ text-after  over r@ @ +  tuck -  r@ @ swap cells-move
    negate r@ tb.last +!  d# -1 r> tb.lines +!
;

: text-room? ( n tb -- f ) \ n cells free after its last line
    begin 2dup text-free > while
        dup tb.roll @  over tb.lines @ d# 1 >  and 0= if
            2drop false exit
        then
        dup text-drop
    repeat
    2drop true
;
: more ( k tb -- n ) \ the cells k more characters take
    tb.len @ tuck + BYTESHIFT rshift swap BYTESHIFT rshift - ;

header text-add
: text-add ( a i u tb -- ) \ onto the end of its last line
    >r
    dup r@ more r@ text-room? 0= if drop 2drop rdrop exit then
    r@ tb.len @  over r@ tb.len +!  r@ tb.last @ swap text-put
    r> text-eol
;

header text-code
: text-code ( c tb -- ) \ a code, the same
    >r  d# 1 r@ more r@ text-room? 0= if drop rdrop exit then
    r@ tb.last @ r@ tb.len @ put-cell  d# 1 r@ tb.len +!
    r> text-eol
;

header text-nl
: text-nl ( tb -- ) \ an empty line after its last
    >r  d# 1 r@ text-room? 0= if rdrop exit then
    r@ text-after r@ tb.last !  d# 0 r@ tb.len !
    d# 1 r@ tb.lines +!  r> text-eol
;

header text-back
: text-back ( tb -- ) \ the last character of its last line out
    dup tb.len @ 0= if drop exit then
    d# -1 over tb.len +!  text-eol
;

\ ---- drawn

create blank meta spaces-cell target ,

: text-run ( a i k f -- ) \ from i to k, in face f
    >r  over -  ?dup 0= if 2drop rdrop exit then
    3dup d# 0 d# 0 draw-chars  chars-width
    r> F-UNDER and if
        dup >r false d# 0 font @ @ 1- r> hline
    then
    ox +!
;

: text-do ( f c -- f' more? ) \ what a code does
    dup EOL = if drop false exit then
    dup INDENT = if
        drop  blank d# 0 d# 1 chars-width INDENT-COLS *  ox +!
        true exit
    then
    nip FACE -
    dup F-BOLD and 0<> bold !  dup F-LEAN and 0<> italic !
    true
;

: text-step ( a i f -- a i' f' more? ) \ a run, and its code
    >r  2dup code-at  3dup r@ text-run  nip
    2dup d# 3 lshift char@  swap 1+ swap  r> swap text-do
;

header draw-line
: draw-line ( a x y -- ) \ the line that starts at a
    ox @ >r  oy @ >r  oy +!  ox +!
    d# 0 d# 0  begin text-step 0= until  2drop drop
    d# 0 bold !  d# 0 italic !  r> oy !  r> ox !
;

\ [PGUP] and [PGDN], white on black, each ending at x
create k-pgup ," [PGUP]"
create k-pgdn ," [PGDN]"

: mark ( x y cstr -- )
    ox @ >r  oy @ >r  -rot oy +! ox +!
    ctext  3dup chars-width negate ox +!
    true inverse !  d# 0 d# 0 draw-chars  d# 0 inverse !
    r> oy !  r> ox !
;
header mark-up   : mark-up   ( x y -- ) k-pgup mark ;
header mark-down : mark-down ( x y -- ) k-pgdn mark ;

header draw-lines
: draw-lines ( a n x y -- ) \ n lines from the one at a
    ox @ >r  oy @ >r  oy +!  ox +!
    d# 0 max negate
    begin dup while
        over d# 0 d# 0 draw-line  font @ @ oy +!
        swap line-next swap  1+
    repeat
    2drop  r> oy !  r> ox !
;
