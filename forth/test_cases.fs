\ Test cases generated from test/tests.h

meta
    4 org
target

\ 8-bit literals
header test_0 : test_0 d# 1  d# 2  d# 3  d# 4 ;

\ Context cleared after tests
header test_1 : test_1 d# 6  d# 7  d# 8  d# 9  d# 10 ;

\ halt ( -- )
header test_2 : test_2 d# 1  halt ;

\ noop ( -- )
header test_3 : test_3 d# 1  d# 2  noop  d# 3  noop ;

\ invert ( a -- ~a )
header test_4 : test_4 d# 1  d# 2  d# 1024  invert ;

\ negative literals
header test_5 : test_5 d# -1  d# -2  d# -3  d# -4  d# -5 ;

\ lit16
header test_6 : test_6 d# 65535  d# -65535  d# 49151  d# -49151 ;

\ lit32 max/min
header test_7 : test_7 d# 4294967295  d# -4294967295  ;

\ lit48 max/min
header test_8 : test_8 d# 140737488355328  d# -140737488355328 ;

\ dup ( a -- a a )
header test_9 : test_9 d# 1  d# 2  d# 3  dup  d# 4  dup ;

\ swap ( a b -- b a )
header test_10 : test_10 d# 5  d# 4  d# 2  d# 1  swap ;

\ over ( a b -- a b a )
header test_11 : test_11 d# 5  d# 4  d# 3  d# 2  over ;

\ nip ( a b -- b )
header test_12 : test_12 d# 5  d# 4  d# 3  d# 2  nip ;

\ tuck ( a b -- b a b )
header test_13 : test_13 d# 1024  d# 2048  tuck ;

\ rot ( a b c -- b c a )
header test_14 : test_14 d# 1024  d# 2048  d# 4096  rot ;

\ -rot ( a b c -- c a b )
header test_15 : test_15 d# 1024  d# 2048  d# 4096  -rot ;

\ drop ( a b -- a )
header test_16 : test_16 d# 5  d# 4  d# 3  d# 2  drop ;

\ 2drop ( a b -- )
header test_17 : test_17 d# 5  d# 4  d# 3  d# 2  2drop ;

\ rdrop ( R: a -- R: )
header test_18 : test_18 d# 1024  >r  d# 2048  >r  rdrop ;

\ + ( a b -- a+b )
header test_19 : test_19 d# 10  d# 15  + ;

\ * ( a b -- a*b )
header test_20 : test_20 d# 10  d# 15  *  d# -520  d# 10  * ;

\ 1+ ( a -- a+1 )
header test_21 : test_21 d# 1  d# 2  d# 3  1+ ;

\ 2+ ( a -- a+2 )
header test_22 : test_22 d# 1  d# 2  d# 3  2+ ;

\ xor ( a b -- a^b )
header test_23 : test_23 d# 65535  d# 32768  xor  d# 65535  d# 65535  xor  d# -1  d# -1  xor ;

\ and ( a b -- a&b )
header test_24 : test_24 d# 65535  d# 32768  and  d# -1  d# 65535  and ;

\ or ( a b -- a|b )
header test_25 : test_25 d# 65535  d# 32768  or  d# -1  d# -1  or ;

\ lshift ( a b -- a << b )
header test_26 : test_26 d# 2  d# 1  lshift ;

\ 48-bit literal test
header test_27 : test_27 d# 20015998343868 ;

\ edge case: 4096
header test_28 : test_28 d# 4096 ;

\ lshift64 ( a b -- a << b )
header test_29 : test_29 d# 140737488355328  d# 16  lshift ;

\ rshift ( a b -- a >> b )
header test_30 : test_30 d# 2  d# 1  rshift ;

\ rshift ( a b -- a >> b )
header test_31 : test_31 d# 4  d# 1  rshift ;

\ - ( a b -- b-a )
header test_32 : test_32 d# 500  d# 300  - ;

\ 2* ( a -- a << 1 )
header test_33 : test_33 d# 512  2* ;

\ 2/ ( a -- a >> 1 )
header test_34 : test_34 d# 512  2/ ;

\ @ ( a -- [a] )
header test_35 : test_35 d# 0  @  d# 8  @  d# 16  @  d# 24  @  d# 32  @  d# 40  @ ;

\ ! ( a b -- )
header test_36 : test_36 d# 1024  d# 0  !  d# -1024  d# 8  !  d# 0  @  d# 8  @ ;

\ >r ( a -- R: a )
header test_37 : test_37 d# 1024  >r ;

\ r@ ( R:a -- a R: a )
header test_38 : test_38 d# 1024  >r  r@  r@ ;

\ r> ( R:a -- a )
header test_39 : test_39 d# 1024  >r  r@  2*  r> ;

\ = ( a b -- f )
header test_40 : test_40 d# 1024  d# 1024  =  d# -1024  d# 1024  =  d# -1024  d# -1024  =  ;

\ u< ( aU bU -- f )
header test_41 : test_41 d# -1024  d# 1024  u<  d# 1024  d# -1024  u< ;

\ < ( a b -- f )
header test_42 : test_42 d# -1024  d# 0  <  d# 0  d# 1024  <  d# 0  d# -1024  <  d# 2048  d# 1024  <  ;

\ exit ( -- )
header test_43 : test_43 d# 1024  >r  d# 0  >r  exit ;

\ 2dup< ( a b -- a b f )
header test_44 : test_44 d# 1024  d# 2048  2dup< ;

\ dup@ ( addr -- addr n )
header test_45 : test_45 d# 8  dup  @ ;

\ overand ( a b -- a f )
header test_46 : test_46 d# 1024  d# 2048  overand  d# 4096  d# 4096  overand ;

\ dup>r ( a -- a R: a )
header test_47 : test_47 d# 1024  dup>r ;

\ dup>r with multiple values
header test_48 : test_48 d# 10  d# 20  d# 30  dup>r ;

\ 2dup< ( a b -- a b f )
header test_49 : test_49 d# 1024  d# 2048  2dup<  d# 2048  d# 1024  2dup< ;

\ 2dupxor ( a b -- a b a^b )
header test_50 : test_50 d# 1024  d# 2048  2dupxor ;

\ over+ ( a b -- a a+b )
header test_51 : test_51 d# 1024  d# 4096  over+ ;

\ over= ( a b -- a a=b? )
header test_52 : test_52 d# 1024  d# 4096  over=  d# 2048  d# 2048  over= ;

\ @@ ( addr -- n )
header test_53 : test_53 d# 0  @@  d# 16  @@ ;

\ ! ( n addr -- )
header test_54 : test_54 d# 16  d# 0  d# 32  d# 8  !  !  d# 8  @  d# 0  @ ;

\ dup@ ( addr -- n )
header test_55 : test_55 d# 16  dup@ ;

\ dup@@ ( addr -- n )
header test_56 : test_56 d# 16  dup@@ ;

\ over@ ( addr a -- addr a n )
header test_57 : test_57 d# 0  d# 2048  over@ ;

\ @r ( R: addr -- n )
header test_58 : test_58 d# 8  >r  @r ;

\ swap>r ( a b -- b R: a )
header test_59 : test_59 d# 1024  d# 2048  swap>r ;

\ swapr> ( a b R: c -- b a c )
header test_60 : test_60 d# 1024  d# 2048  d# 4096  >r  swapr> ;

\ key ( -- ch )
header test_61 : test_61 key  emit  key  emit  key  emit  key  emit ;

\ +! ( n addr -- )
header test_62 : test_62 d# 0  >r  d# 4   r@  +!  r@  @  d# 16  r@  +!  r@  @  ;

\ r@; ( R: addr -- addr )
header test_63 : test_63 d# 4  >r  r@; ;

\ @and ( n addr -- n )
header test_64 : test_64 d# 65535  d# 0  @and ;

\ 2dup ( a b -- a b a b )
header test_65 : test_65 d# 1024  d# 2048  2dup ;

\ 2swap ( ab cd -- cd ab )
header test_66 : test_66 d# 1024  d# 2048  d# 4096  d# 8192  2swap ;

\ 2over ( ab cd - ab cd ab )
header test_67 : test_67 d# 1024  d# 2048  d# 4096  d# 8192  2over ;

\ 3rd ( abc -- abc a )
header test_68 : test_68 d# 1024  d# 2048  d# 4096  3rd ;

\ 3dup ( abc -- abc abc )
header test_69 : test_69 d# 1024  d# 2048  d# 4096  3dup ;

\ > ( a b -- f )
header test_70 : test_70 d# 1024  d# 2048  >  d# 2048  d# 1024  > ;

\ u> ( a b -- f )
header test_71 : test_71 d# 1024  d# 2048  u>  d# 2048  d# 1024  u> ;

\ 0= ( a -- f )
header test_72 : test_72 d# 0  0=  d# 1  0=  d# -1  0=  ;

\ 0< ( a -- f )
header test_73 : test_73 d# 0  0<  d# 1  0<  d# -1  0<  ;

\ 0> ( a -- f )
header test_74 : test_74 d# 0  0>  d# 1  0>  d# -1  0>  ;

\ 0<> ( a -- f )
header test_75 : test_75 d# 0  0<>  d# 1  0<>  d# -1  0<>  ;

\ 1- ( a -- a-1 )
header test_76 : test_76 d# 1  1-  d# 0  1-  d# -1  1-  ;

\ bounds ( a b -- a+b a )
header test_77 : test_77 d# 1024  d# 2048  bounds ;

\ s>d ( s -- s f )
header test_78 : test_78 d# 1024  s>d ;

\ 0xfffffffffffffff00
header test_79 : test_79 d# 18446744073709551360 ;

\ char literal ( -- c )
header test_80 : test_80 [char] a ;

\ c@ ( addr -- c )
header test_81 : test_81 d# 0  c@  d# 1  c@  d# 8  c@ ;

\ c! ( c addr -- )
header test_82 : test_82 [char] d  d# 0  c!  [char] e  d# 1  c!  d# 0  c@  d# 1  c@ ;

\ w@ ( addr -- w )
header test_83 : test_83 d# 0  w@  d# 2  w@ ;

\ w! ( w addr -- )
header test_84 : test_84 d# 43981  d# 0  w!  d# 61680  d# 2  w!  d# 0  @ ;

\ w@ with various addresses
header test_85 : test_85 d# 0  w@  d# 2  w@  d# 4  w@  d# 6  w@ ;

\ w! preserves other bytes
header test_86 : test_86 d# 43981  d# 0  w!  d# 48879  d# 5  w!  d# 0  @ ;

\ 2w@ ( addr -- w1 w2 )
header test_87 : test_87 d# 0  2w@ ;

\ 2w! ( w1 w2 addr -- )
header test_88 : test_88 d# 43981  d# 61680  d# 0  2w!  d# 0  @ ;

header main : main halt ;
header quit : quit halt ;
