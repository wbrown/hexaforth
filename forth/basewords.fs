
\ input_mux
: N->I       h# 0000 ;
: T->I       h# 0400 ;
: [T]->I     h# 0800 ;
: R->I       h# 0c00 ;

\ alu_op
: I->I       h# 0000 or ;
: N->T,I     h# 0010 or ;
: T+I        h# 0020 or ;
: T&I        h# 0030 or ;
: T|I        h# 0040 or ;
: T|N        h# 0040 ;
: T^I        h# 0050 or ;
: ~I         h# 0060 or ;
: ~T         h# 0460 ;
: T==I       h# 0070 or ;
: I<T        h# 0080 or ;
: I>>T       h# 0090 or ;
: I<<T       h# 00a0 or ;
: N<<T       h# 04a0 ;
: [I]        h# 00b0 or ;
: I->io[T]   h# 00c0 or ;
: io[I]      h# 00d0 or ;
: status     h# 00e0 or ;
: Iu<T       h# 00f0 or ;

\ output_mux
: ->T        h# 0000 or ;
: ->R        h# 0100 or ;
: ->[T]      h# 0200 or ;
: ->NULL     h# 0300 or ;

\ stack_ops
: d+1        h# 0004 or ;
: d-1        h# 000c or ;
: d-2        h# 0008 or ;
: r+1        h# 0001 or ;
: r-1        h# 0003 or ;
: r-2        h# 0002 or ;
: RET        h# 1000 or ;

\ literals
: imm+       h# 4000 or ;
: imm<<12    h# 1000 or ;
: imm<<24    h# 2000 or ;
: imm<<36    h# 3000 or ;
: imm        h# 8000 or tcode, ;

\ op_type
: ubranch    h# 0000 or tcode, ;
: 0branch    h# 2000 or tcode, ;
: scall      h# 4000 or tcode, ;
: alu        h# 6000 or tcode, ;

\ words
:: noop             N->I             ->NULL              alu ;
:: +                N->I   T+I       ->T       d-1       alu ;
:: xor              N->I   T^I       ->T       d-1       alu ;
:: and              N->I   T&I       ->T       d-1       alu ;
:: or               N->I   T|I       ->T       d-1       alu ;
:: invert           T->I   ~I        ->T                 alu ;
:: =                N->I   T==I      ->T       d-1       alu ;
:: <                N->I   I<T       ->T       d-1       alu ;
:: u<               N->I   Iu<T      ->T       d-1       alu ;
:: swap             N->I   N->T,I    ->T                 alu ;
:: dup              T->I             ->T       d+1       alu ;
:: nip              T->I   N->T,I    ->T       d-1       alu ;
:: drop             N->I             ->T       d-1       alu ;
:: over             N->I             ->T       d+1       alu ;
:: >r               T->I             ->R       d-1  r+1  alu ;
:: r>               R->I             ->T       d+1  r-1  alu ;
:: r@               R->I             ->T       d+1       alu ;
:: @                [T]->I           ->T                 alu ;
:: !                N->I             ->[T]     d-2       alu ;
:: io@              T->I   io[I]     ->T                 alu ;
:: io!              N->I   I->io[T]  ->T       d-2       alu ;
:: rshift           N->I   I>>T      ->T       d-1       alu ;
:: lshift           N->I   I<<T      ->T       d-1       alu ;
:: depths           T->I   status    ->T                 alu ;
:: depthr           R->I   status    ->T                 alu ;
:: exit             T->I             ->T  RET       r-1  alu ;
:: 2dup<            N->I   I<T       ->T       d+1       alu ;
:: dup@             [T]->I           ->T       d+1       alu ;
:: overand          N->I   T&I       ->T                 alu ;
:: dup>r            N->I             ->R            r+1  alu ;
:: 2dupxor          T->I   T^I       ->R       d+1       alu ;
:: over+            T->I   T+I       ->T       d+1       alu ;
:: over=            T->I   T==I      ->T       d+1       alu ;
:: rdrop            T->I             ->T            r-1  alu ;
:: 1+        1      imm+                                 imm ;
:: 2+        2      imm+                                 imm ;
