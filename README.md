# hexaforth

Work-in-progress experiments in unencoded instructions for performant stack machines; incomplete.

# Why?

Traditionally, many p-code VMs are implemented as bytecode. I wanted to investigate how encoded instructions such as implemented by [Novix NC4016](https://users.ece.cmu.edu/~koopman/stack_computers/sec4_4.html) or the [J1 Forth CPU](https://excamera.com/files/j1.pdf) used perform on modern architectures with a VM.

# What?

The instruction encoding draws heavily from the J1b Forth CPU described at [swapforth/j1b/basewords.fs](https://github.com/jamesbowman/swapforth/blob/master/j1b/basewords.fs).  There are some notable differences, as I wanted to implement a 64-bit VM rather than a 16-bit VM, specifically how literals are handled.

## Literal Handling

```c
        struct {
            bool  lit_f: 1;
            bool  lit_add: 1;       // add
            BYTE  lit_shifts: 2;    // 0-3 shifts
            WORD  lit_v: 12;
        } lit;
```

If the first bit of the instruciton is on, it's a literal.  The literal is a 12-bit unsigned number, in `lit_v`.  Of particular interest is the `lit_shifts` field, which tells the VM to shift the `lit_v` number left by `lit_shifts * 12` bits.  This allows us to express 48 bits of magnitude in a 12-bit literal instruction.

The `lit_add` flag determines if the literal should be added to `T`, the top element of the stack, and has the following characteristics:
* `true` -- add shifted `lit_v` value to `T`, and do *not* advance the `SP`; we are working with the existing value in place.
* `false` -- advance the `SP` and set the new `T` to the shifted `lit_v` value.

An astute reader might have noted that the stack is 64-bit signed, but that our literals are unsigned.  This is taken care of by the literal encoder, inserting an instruction to invert the number on the stack.

A bonus characteristic of this literal implementation is that the literal instruction can be used as an in-line increment instruction for numbers up to `4096`, without having to push the value onto the stack, doing in one VM cycle what would normally take three.

Some examples of the literal encoding, such as for `-20`.  As it is less than `4096`, we encode it as a single literal, and then we negate it with the following `invert` opcode call.

```
COMPILE_LITERAL: -20
HERE[0]: 0x0141 => lit: {lit_f=1, lit_add=0, shifts=0, lit.lit_v=20 => 20}
HERE[1]: 0x0036 => alu: {alu_op: 3, op: ~T, DSTACK: 0 RSTACK: 0, N->[T]: 0} => invert
```

An example of how we leverage the `lit_add` field is the number `-24923`.  It's turned into an *unsigned* `24923`, the rightmost 12-bits are encoded as `347`, and then the next 12-bits are encoded as `24576` with the `lit_add` flag set to `1`.  Then we `invert`.

```
COMPILE_LITERAL: -24923
HERE[11]: 0x15b1 => lit: {lit_f=1, lit_add=0, shifts=0, lit.lit_v=347 => 347}
HERE[12]: 0x0067 => lit: {lit_f=1, lit_add=1, shifts=1, lit.lit_v=6 => 24576}
HERE[13]: 0x0036 => alu: {alu_op: 3, op: ~T, DSTACK: 0 RSTACK: 0, N->[T]: 0} => invert
```

The literal compiler is smart enough not to bother encoding 12-bit segments that are `0`, skipping over them such as for `32768`.  This makes the literal encoding perfect for memory offset calculations up to 48-bits.

```
COMPILE_LITERAL: 32768
HERE[14]: Skipping LIT instruction as lit_v == 0, shift=0
HERE[14]: 0x0085 => lit: {lit_f=1, lit_add=0, shifts=1, lit.lit_v=8 => 32768}
```

## ALU Instructions

ALU instrucitons are roughly divided into two sections.  When `op_type` is `OP_TYPE_ALU`, we use `alu_op` for the actual operation and `mem_op` for 'what to do with the result of the ALU op'.  We also have fields for whether we should adjust the data stack or return stack, and whether we should write to memory.

```c
        struct {
            bool    lit_f: 1;
            BYTE    op_type: 2;
            BYTE    alu_op: 4;
            BYTE    mem_op: 3;
            bool    __unused: 1;
            BYTE    dstack: 2;
            BYTE    rstack: 2;
            bool    ram_write: 1;
        } alu;
```

We have the following `alu_op`s:

|---------|-----|----------------------------------------------|
| ALU Op  | Op  | Descripton                                   |
|---------|-----|----------------------------------------------|
| `T`     | 0x0 | Do something with `T`                        |
| `N`     | 0x1 | Set `T` to N                                 |
| `T+N`   | 0x2 | Add `T` and `N`                              |
| `T&N`   | 0x3 | Bitwise AND of `T` and `N`                   |
| `T|N`   | 0x4 | Bitwise OR of `T` and `N`                    |
| `T^N`   | 0x5 | Bitwise XOR of `T` and `N`                   |
| `~T`    | 0x6 | Bitwise inversion of`T`                      |                  
| `T==N`  | 0x7 | Test if `T` and `N` are equivalent           |
| `T>N`   | 0x8 | Test if `T` is greater than `N`|             |
| `N>>T`  | 0x9 | Bitshift `N` left by `T` bits.               |
| `N<<T`  | 0xA | Bitshift `N` right by `T` bits.              |
| `R->T`  | 0xB | Copy the top of return stack `R` to `T`      |
| `[T]`   | 0xC | Load address located at `T` into `T`.        |
| `io[T]` | 0xD | IO read at `T`.                              |
| `depth` | 0xE | Depth of the stack                           |
| `T>Nu`  | 0xF | Unsigned test for if `T` is greater than `N` |

Then we have the following `mem_op`s:

|-----------|-----|------------------------------------------------|
| Mem Op    | Op  | Descripton                                     |
|-----------|-----|------------------------------------------------|
| `T->N`    | 0x0 | Copy `T` into `N`                              |
| `T->R`    | 0x1 | Copy `T` into `R`, the top of the return stack |
| `N->[T]`  | 0x2 | Store `N` into address `T`                     |
| `io[N]->T`| 0x3 | Write IO operation `N`into `T`                 |
| `io@`     | 0x4 | Read IO operation into `T`                     |
| `R->EIP`  | 0x5 | Copy the R stack into the IP pointer           |

Then with `dstack` and `rstack`, we can adjust the size of the data and return stacks.

Finally, we have `ram_write`, which is to write `N` into the address at `T`.

Effectively, this encoding allows us to have microinstructions that we construct Forth words out of.

An example is `!` which is 'write N to [T]', encoded as such:

```c
        {"!", {
                        { .alu.op_type = OP_TYPE_ALU,
                          .alu.alu_op = ALU_N,
                          .alu.dstack = -1,
                          .alu.ram_write = 1 }}},
```
