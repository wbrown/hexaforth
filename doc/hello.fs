\ hello.fs - a window, typed at the prompt (DESKTOP.md, the target
\ `hello`).  It shows how many keys it has been given; q prints that
\ and how many windows there are, and halts the machine.

create t-hello  34 parse Hello" s,
variable presses

: hello-draw ( win -- )
   drop  false 8 8 120 40 frame-rect  presses @ '0' + 16 20 draw-char ;

: hello-key ( c win -- )
   swap 'q' = if drop  presses @ .  nwin @ .  8000 execute then
   >r  1 presses +!  0 0 136 56 r> win-inval ;

display-init  0 nwin !
40 60 200 120 t-hello ' hello-draw ' hello-key 0 window drop
event-loop
abcq
