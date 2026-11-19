\ hello.fs - a window, typed at the prompt
\
\ It shows how many keys it has been given, in a frame.  The key q prints
\ that and how many windows there are, and halts the machine.  The target
\ `hello` types this at the reference VM and makes a picture of its
\ display (DESKTOP.md).

create t-hello  34 parse Hello" s,
variable presses

: hello-draw ( win -- )
   drop  false 8 8 120 40 frame-rect  presses @ '0' + 16 20 draw-char ;

: hello-key ( c win -- )
   swap 'q' = if drop  presses @ .  nwin @ .  8000 execute then
   >r  1 presses +!  0 0 136 56 r> win-inval ;

display-init  no-windows
40 60 200 120 t-hello ' hello-draw ' hello-key 0 window drop
event-loop
abcq
