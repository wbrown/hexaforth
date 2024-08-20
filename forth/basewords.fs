
\ input_mux
: N->IN      h# 0000 ;
: T->IN      h# 0400 ;
: [T]->IN    h# 0800 ;
: R->IN      h# 0c00 ;

\ alu_op
: IN->       h# 0000 or ;
: T<>N,IN->  h# 0010 or ;
: T->N,IN->  h# 0020 or ;
: IN+N       h# 0030 or ;
: IN&N       h# 0040 or ;
: IN|N       h# 0050 or ;
: T|N        h# 0450 ;
: IN^N       h# 0060 or ;
: IN*N       h# 0070 or ;
: ~IN        h# 0080 or ;
: ~T         h# 0480 ;
: IN==N      h# 0090 or ;
: N<IN       h# 00a0 or ;
: IN>>T      h# 00c0 or ;
: IN<<T      h# 00d0 or ;
: N<<T       h# 00d0 ;
: [IN]       h# 00e0 or ;
: io[IN]     h# 00f0 or ;
: Nu<IN      h# 00b0 or ;

\ output_mux
: ->T        h# 0000 or ;
: ->R        h# 0100 or ;
: ->io[T]    h# 0200 or ;
: ->[T]      h# 0300 or ;

\ stack_ops
: d+1        h# 0004 or ;
: d+0        h# 0000 or ;
: d-1        h# 000c or ;
: d-2        h# 0008 or ;
: r+1        h# 0001 or ;
: r+0        h# 0000 or ;
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
:: halt      $0000                                              ubranch ;
:: noop              T->IN   IN->      ->T     d+0    r+0       alu     ;
:: xor               T->IN   IN^N      ->T     d-1    r+0       alu     ;
:: and               T->IN   IN&N      ->T     d-1    r+0       alu     ;
:: or                T->IN   IN|N      ->T     d-1    r+0       alu     ;
:: invert            T->IN   ~IN       ->T     d+0    r+0       alu     ;
:: +                 T->IN   IN+N      ->T     d-1    r+0       alu     ;
:: *                 T->IN   IN*N      ->T     d-1    r+0       alu     ;
:: =                 T->IN   IN==N     ->T     d-1    r+0       alu     ;
:: <                 T->IN   N<IN      ->T     d-1    r+0       alu     ;
:: u<                T->IN   Nu<IN     ->T     d-1    r+0       alu     ;
:: swap              N->IN   T->N,IN-> ->T     d+0    r+0       alu     ;
:: dup               T->IN   IN->      ->T     d+1    r+0       alu     ;
:: nip               T->IN   T->N,IN-> ->T     d-1    r+0       alu     ;
:: tuck              T->IN   T<>N,IN-> ->T     d+1    r+0       alu     ;
:: drop              N->IN   IN->      ->T     d-1    r+0       alu     ;
:: 2drop             R->IN   IN->      ->R     d-2    r+0       alu     ;
:: rdrop             T->IN   IN->      ->T     d+0    r-1       alu     ;
:: over              N->IN   IN->      ->T     d+1    r+0       alu     ;
:: >r                T->IN   IN->      ->R     d-1    r+1       alu     ;
:: r>                R->IN   IN->      ->T     d+1    r-1       alu     ;
:: over>r            N->IN   IN->      ->R     d+0    r+1       alu     ;
:: r@                R->IN   IN->      ->T     d+1    r+0       alu     ;
:: @                 [T]->IN IN->      ->T     d+0    r+0       alu     ;
:: @+                [T]->IN IN+N      ->T     d-1    r+0       alu     ;
:: @and              [T]->IN IN&N      ->T     d-1    r+0       alu     ;
:: !                 N->IN   IN->      ->[T]   d-2    r+0       alu     ;
:: io@               T->IN   io[IN]    ->T     d+0    r+0       alu     ;
:: io!               N->IN   IN->      ->io[T] d-2    r+0       alu     ;
:: +!                [T]->IN IN+N      ->[T]   d-2    r+0       alu     ;
:: rshift            N->IN   IN>>T     ->T     d-1    r+0       alu     ;
:: lshift            N->IN   IN<<T     ->T     d-1    r+0       alu     ;
:: exit              T->IN   IN->      ->T     d+0    r-1  RET  alu     ;
:: @@                [T]->IN [IN]      ->T     d+0    r+0       alu     ;
:: dup@              [T]->IN IN->      ->T     d+1    r+0       alu     ;
:: dup@@             [T]->IN [IN]      ->T     d+1    r+0       alu     ;
:: over@             N->IN   [IN]      ->T     d+1    r+0       alu     ;
:: @r                R->IN   [IN]      ->T     d+1    r+0       alu     ;
:: r@;               R->IN   IN->      ->T     d+1    r-1  RET  alu     ;
:: 2dup<             T->IN   N<IN      ->T     d+1    r+0       alu     ;
:: overand           T->IN   IN&N      ->T     d+0    r+0       alu     ;
:: dup>r             T->IN   IN->      ->R     d+0    r+1       alu     ;
:: 2dupxor           T->IN   IN^N      ->T     d+1    r+0       alu     ;
:: over+             T->IN   IN+N      ->T     d+0    r+0       alu     ;
:: over=             T->IN   IN==N     ->T     d+0    r+0       alu     ;
:: swap>r            N->IN   T->N,IN-> ->R     d-1    r+1       alu     ;
:: swapr>            R->IN   T<>N,IN-> ->T     d+1    r-1       alu     ;
:: 1+             1          imm+                               imm     ;
:: 2+             2          imm+                               imm     ;
:: 2*             1                                             imm    
                     N->IN   IN<<T     ->T     d-1    r+0       alu     ;
:: 2/             1                                             imm    
                     N->IN   IN>>T     ->T     d-1    r+0       alu     ;
:: negate            T->IN   ~IN       ->T     d+0    r+0       alu    
                  1          imm+                               imm     ;
:: -                 T->IN   ~IN       ->T     d+0    r+0       alu    
                  1          imm+                               imm    
                     T->IN   IN+N      ->T     d-1    r+0       alu     ;
:: emit         241                                             imm    
                     N->IN   IN->      ->io[T] d-2    r+0       alu     ;
:: 8emit        240                                             imm    
                     N->IN   IN->      ->io[T] d-2    r+0       alu     ;
:: key          224                                             imm    
                     T->IN   io[IN]    ->T     d+0    r+0       alu     ;
:: rot               T->IN   IN->      ->R     d-1    r+1       alu    
                     N->IN   T->N,IN-> ->T     d+0    r+0       alu    
                     R->IN   IN->      ->T     d+1    r-1       alu    
                     N->IN   T->N,IN-> ->T     d+0    r+0       alu     ;
:: -rot              N->IN   T->N,IN-> ->R     d-1    r+1       alu    
                     R->IN   T<>N,IN-> ->T     d+1    r-1       alu     ;
:: 2dup              N->IN   IN->      ->T     d+1    r+0       alu    
                     N->IN   IN->      ->T     d+1    r+0       alu     ;
:: 2swap             T->IN   IN->      ->R     d-1    r+1       alu    
                     N->IN   T->N,IN-> ->T     d+0    r+0       alu    
                     R->IN   IN->      ->T     d+1    r-1       alu    
                     N->IN   T->N,IN-> ->T     d+0    r+0       alu    
                     T->IN   IN->      ->R     d-1    r+1       alu    
                     T->IN   IN->      ->R     d-1    r+1       alu    
                     N->IN   T->N,IN-> ->T     d+0    r+0       alu    
                     R->IN   IN->      ->T     d+1    r-1       alu    
                     N->IN   T->N,IN-> ->T     d+0    r+0       alu    
                     R->IN   IN->      ->T     d+1    r-1       alu     ;
:: 2over             T->IN   IN->      ->R     d-1    r+1       alu    
                     T->IN   IN->      ->R     d-1    r+1       alu    
                     N->IN   IN->      ->T     d+1    r+0       alu    
                     N->IN   IN->      ->T     d+1    r+0       alu    
                     R->IN   IN->      ->T     d+1    r-1       alu    
                     R->IN   T<>N,IN-> ->T     d+1    r-1       alu    
                     N->IN   T->N,IN-> ->R     d-1    r+1       alu    
                     T->IN   IN->      ->R     d-1    r+1       alu    
                     R->IN   T<>N,IN-> ->T     d+1    r-1       alu    
                     R->IN   T<>N,IN-> ->T     d+1    r-1       alu     ;
:: 3rd               T->IN   IN->      ->R     d-1    r+1       alu    
                     N->IN   IN->      ->T     d+1    r+0       alu    
                     R->IN   IN->      ->T     d+1    r-1       alu    
                     N->IN   T->N,IN-> ->T     d+0    r+0       alu     ;
:: 3dup              T->IN   IN->      ->R     d-1    r+1       alu    
                     N->IN   IN->      ->T     d+1    r+0       alu    
                     R->IN   IN->      ->T     d+1    r-1       alu    
                     N->IN   T->N,IN-> ->T     d+0    r+0       alu    
                     T->IN   IN->      ->R     d-1    r+1       alu    
                     N->IN   IN->      ->T     d+1    r+0       alu    
                     R->IN   IN->      ->T     d+1    r-1       alu    
                     N->IN   T->N,IN-> ->T     d+0    r+0       alu    
                     T->IN   IN->      ->R     d-1    r+1       alu    
                     N->IN   IN->      ->T     d+1    r+0       alu    
                     R->IN   IN->      ->T     d+1    r-1       alu    
                     N->IN   T->N,IN-> ->T     d+0    r+0       alu     ;
:: >                 N->IN   T->N,IN-> ->T     d+0    r+0       alu    
                     T->IN   N<IN      ->T     d-1    r+0       alu     ;
:: u>                N->IN   T->N,IN-> ->T     d+0    r+0       alu    
                     T->IN   Nu<IN     ->T     d-1    r+0       alu     ;
:: 0=             0                                             imm    
                     T->IN   IN==N     ->T     d-1    r+0       alu     ;
:: 0<             0                                             imm    
                     T->IN   N<IN      ->T     d-1    r+0       alu     ;
:: 0>             0                                             imm    
                     N->IN   T->N,IN-> ->T     d+0    r+0       alu    
                     T->IN   N<IN      ->T     d-1    r+0       alu     ;
:: <>                T->IN   IN==N     ->T     d-1    r+0       alu    
                     T->IN   ~IN       ->T     d+0    r+0       alu     ;
:: 0<>            0                                             imm    
                     T->IN   IN==N     ->T     d-1    r+0       alu    
                     T->IN   ~IN       ->T     d+0    r+0       alu     ;
:: 1-             1                                             imm    
                     T->IN   ~IN       ->T     d+0    r+0       alu    
                  1          imm+                               imm    
                     T->IN   IN+N      ->T     d-1    r+0       alu     ;
:: bounds            T->IN   IN+N      ->T     d+0    r+0       alu    
                     N->IN   T->N,IN-> ->T     d+0    r+0       alu     ;
:: s>d               T->IN   IN->      ->T     d+1    r+0       alu    
                  0                                             imm    
                     T->IN   N<IN      ->T     d-1    r+0       alu     ;
:: .s             0                                             imm    
                224                                             imm    
                     N->IN   IN->      ->io[T] d-2    r+0       alu     ;
