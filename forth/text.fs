\ text.fs - text in cells, a line at a time: put in, found and
\ drawn

\ Text is kept as it is drawn: characters packed into cells, the
\ lowest eight bits of a cell first (char@, display.fs).  A text
\ is lines, and each starts where a cell does and ends with EOL.
\ What is after that in its cell is not text, and nothing is
\ kept for the columns a line does not reach.  Two other codes
\ below a space are in a line: INDENT, which is three columns of
\ nothing, and a face, FACE and three bits, which is how what
\ comes after it on the line looks.  A line starts plain.

\ Text is put in a cell at a time: each cell of what goes in is
\ shifted to where it goes, and only the first is merged with
\ what is there.  A code is found a cell at a time too: a cell
\ that has one is told by its bits all at once (ctl?), and only
\ then is it looked into.
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

: ctl? ( x -- f ) \ is a character of the cell below a space?
    dup BLANKS - swap invert and HIGHS and ;

: >cb ( a i -- a' s ) \ character i of the cells from a: its
    \ cell, and how many bits into it
    dup BYTESHIFT rshift rot +
    swap OFFSET-MASK and d# 3 lshift
;

\ a cell of what starts s bits into the cell at a, s less than a
\ cell
: bits@ ( a s -- x )
    ?dup if
        >r  dup @ r@ rshift  swap 1+ @  CELLBITS r> - lshift
        or
        exit
    then
    @
;

\ x put at character i of the cells from a: what is before it
\ is kept, and the rest of its cell is x's
: put-cell ( x a i -- )
    >cb  rot over lshift  swap d# 1 swap lshift 1-
    rot dup >r @ and or  r> !
;

\ the cells from b up to e, from what starts s bits into a
: bits-move ( a s b e -- )
    begin 2dupxor while
        >r >r  2dup bits@ r@ !  swap 1+ swap  r> 1+ r>
    repeat
    2drop 2drop
;

\ u characters from character i of the cells from a, put at
\ character j of the cells from b: the first cell they go in,
\ and the rest
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

\ A line's end is found from the codes in it: the first below a
\ space after character i, and the first of those that is EOL.
\ What is before i is made spaces, so that it is not found.
: code-at ( a i -- k )
    over >r  >cb  d# 1 swap lshift 1- BLANKS and  over @ or
    begin dup ctl? 0= while drop 1+ dup @ repeat
    swap r> - BYTESHIFT lshift swap
    begin dup h# ff and d# 32 u< 0= while
        d# 8 rshift swap 1+ swap
    repeat
    drop
;

: line-end ( a -- k ) \ where the line that starts at a ends
    d# 0
    begin over swap code-at  2dup d# 3 lshift char@ EOL <> while
        1+
    repeat
    nip
;
: line-next ( a -- a' ) dup line-end BYTESHIFT rshift + 1+ ;

\ ---- a text

\ A text is a record of six cells, and the cells it has from the
\ first of them.  When it is full, a text that rolls loses its
\ first lines to make room, and one that does not takes no more.
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

: text-drop ( tb -- )
    \ its first line out, and the rest moved up
    >r  r@ @ line-next r@ @ -                       ( d )
    r@ text-after  over r@ @ +  tuck -  r@ @ swap cells-move
    negate r@ tb.last +!  d# -1 r> tb.lines +!
;

\ n cells free after its last line's, if they are or can be made
: text-room? ( n tb -- f )
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

\ u characters from character i of the cells from a, onto the
\ end of its last line
header text-add
: text-add ( a i u tb -- )
    >r
    dup r@ more r@ text-room? 0= if drop 2drop rdrop exit then
    r@ tb.len @  over r@ tb.len +!  r@ tb.last @ swap text-put
    r> text-eol
;

header text-code
: text-code ( c tb -- ) \ a code onto the end of its last line
    >r  d# 1 r@ more r@ text-room? 0= if drop rdrop exit then
    r@ tb.last @ r@ tb.len @ put-cell  d# 1 r@ tb.len +!
    r> text-eol
;

header text-nl
: text-nl ( tb -- )
    \ its last line ended, and an empty one after
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

\ A line is drawn a run at a time: the characters up to the next
\ code, and then what that code does.  Where the next run goes
\ is the port's origin, which is moved along the line as it is
\ drawn and put back after.
create blank meta spaces-cell target ,  \ a space, three of
                                        \ which are an indent

: text-run ( a i k f -- )
    \ the characters from i to k, in face f
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

: text-step ( a i f -- a i' f' more? )
    \ a run, and the code after
    >r  2dup code-at  3dup r@ text-run  nip
    2dup d# 3 lshift char@  swap 1+ swap  r> swap text-do
;

header draw-line
: draw-line ( a x y -- ) \ the line that starts at a
    ox @ >r  oy @ >r  oy +!  ox +!
    d# 0 d# 0  begin text-step 0= until  2drop drop
    d# 0 bold !  d# 0 italic !  r> oy !  r> ox !
;

\ A window that shows part of a text says so: [PGUP] at the
\ right of its first line when more is above, and [PGDN] at the
\ right of its last when more is below, white on black, each
\ ending at x.
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

\ n lines from the one at a, each a line of the font under the
\ one before
header draw-lines
: draw-lines ( a n x y -- )
    ox @ >r  oy @ >r  oy +!  ox +!
    d# 0 max negate
    begin dup while
        over d# 0 d# 0 draw-line  font @ @ oy +!
        swap line-next swap  1+
    repeat
    2drop  r> oy !  r> ox !
;
