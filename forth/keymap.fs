\ keymap.fs - what each key is

\ A keyboard says which key by a number, the Linux key code, and
\ this is what each of the 128 is as a character, and what each
\ is with shift held: 256 characters packed into cells, the keys
\ and then the keys shifted, a key found with char@ (wm.fs).  A
\ row of keys is the code of its first, what the keys are, and
\ what they are shifted (cross.fs).  What is 128 and up is not a
\ character: the cursor keys and the like, which whoever has the
\ keys knows by those numbers.

bulk
header keymap create keymap

keys[
    #2  keyrow 1234567890-=    !@#$%^&*()_+
    #16 keyrow qwertyuiop[]    QWERTYUIOP{}
    #30 keyrow asdfghjkl;'`    ASDFGHJKL:"~
    #43 keyrow \zxcvbnm,./     |ZXCVBNM<>?
    #71 keyrow 789-456+1230.   789-456+1230.        \ the keypad
    #1   #27  keyis     \ escape
    #14  #8   keyis     \ backspace
    #15  #9   keyis     \ tab
    #28  #13  keyis     \ enter
    #57  #32  keyis     \ space
    #55  #42  keyis     \ the keypad's *
    #96  #13  keyis     \ and its enter
    #98  #47  keyis     \ and its /
    #103 #128 keyis     \ up
    #108 #129 keyis     \ down

    #105 #130 keyis     \ left
    #106 #131 keyis     \ right
    #104 #132 keyis     \ page up
    #109 #133 keyis     \ page down
    #102 #134 keyis     \ home
    #107 #135 keyis     \ end
    #111 #127 keyis     \ delete
]keys

unbulk
