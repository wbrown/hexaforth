# Hexaforth

Hexaforth is a stack machine, a Forth system that runs on it, and three implementations of the machine: `vm.c` (the reference VM, in C), two AArch64 kernels in `baremetal/` that run under QEMU, and Verilog in `rtl/`. The Forth system is a nucleus (kernel, interpreter, compiler, dictionary, tasks) and, where the machine has a display, a desktop of windows and applications.

`MODEL.md` specifies the machine, and the kernels and the Verilog are tested against `vm.c`. If code and `MODEL.md` disagree, report it and leave `MODEL.md` as it is. A change to what the machine does changes `MODEL.md`, `vm.c`, both kernels and the Verilog together.

## Rules

- Say that something builds, runs or passes only when a tool result in this session shows it, and quote that result. Quote only output that a tool actually printed. A tool call that returned an error left everything as it was, so make the call again correctly before relying on it.
- Run commands yourself with the tools, including any you would otherwise hand to the user.
- Do not work a whole solution out before running anything. A turn that ends inside its reasoning is discarded, so write one line, run it, and choose the next step from what it printed.
- Do not reason inside a tool call's arguments. Decide what a file will contain before you write it, and put only that in the call: `hexaforth` types a file line by line, so a note to yourself in it is a line of undefined words.
- When the user asks a question about work in progress, stop all running and editing, and answer the question: yes or no first, then what was done and why. End the answer there, because the user decides what happens next. "Stop" means the same.
- Report what was done, what failed, and what the user must decide. A decision stays decided once it is made, so act on it.
- The project is written in C and Forth, so a test, a generator or a measuring tool is C in the tree that CMake builds and runs. Python, shell scripts and Makefiles stay out of the tree.
- Each fact (an encoding, a layout, a table, a size) is defined in one place, and everything else is generated from it or reads it there. Refer to a size or an address by its name.
- Measure instead of estimating. Find out what instructions do by running them on `vm.c`, and what a change costs by running the benchmark before and after it. Runtime is counted in cycles, because a memory read, a shift and a multiply each take more than one.
- The ALU's operation field is full and the design depends on the fields composing, so look for speed in the fields the machine has, and leave its operations as they are.

In the shell:

- Edit a file with the `edit` tool and create one with the `write` tool. Read a file with the `read` tool, and search with the `grep` and `glob` tools. The shell is for running programs.
- A shell call is one program and its arguments, run from the top of the tree. Do not use `&&`, `||`, `;`, pipes, `<` or here-documents. Do not run `cd`, `echo`, `cat`, `head`, `tail`, `sed`, `awk`, `grep`, `sort`, `wc`, `diff`, `python` or a debugger.
- Ask the tree's own tools (Tools, below) about an image, a profile or a program's output, and extend one of them when the question is beyond them all.

## The machine

- A cell is 64, 32 or 16 bits. The system is built and tested at all three sizes (`util/machines`), and 64 is the default.
- Memory is an array of cells. An address is a cell number, and `@` and `!` fetch and store one cell. The machine counts in cells and instructions throughout.
- There are two stacks, data and return, both separate from memory. T is the top of the data stack, N the cell under it, and R the top of the return stack.
- An instruction is 16 bits, and code is addressed by instruction. A wider cell holds several instructions, the first in its low bits.

| Top bits | Instruction |
|---|---|
| `1` | literal |
| `000` | jump |
| `001` | conditional jump, which pops T and jumps if it was zero |
| `010` | call |
| `011` | ALU instruction |

A literal holds a 12-bit number and a shift of 0, 12 or 24, and either pushes the shifted number or adds it to T. The fourth value of its shift field makes it fetch or store one of the 4,096 cells starting at `VAR_BASE`, so a variable kept there costs one instruction to fetch or to store.

An ALU instruction is made of fields in place of an opcode. Its fields select the input (N, T, the cell T points to, or R), one of sixteen operations, where the result goes (T, R, the cell T points to, or port T), how each stack moves, and whether to return. Every combination is an instruction, and a baseword such as `dup` or `+` is a name for one of them (`vm_opcodes.h`).

A literal is unsigned, so adding 1 is one instruction and subtracting 1 is two, and loops count up. A shift count is taken modulo 64 at every cell size. Code is in pages of 4,096 instructions, and a call within a page, or into page 0 where the kernel is, is one instruction, while a call into any other page pushes the address and calls `execute`.

## Writing Forth

The system is built as an image, and `hexaforth` runs it. To write a word, put Forth in a file and run the file. `hexaforth` types the file at the system's prompt, a line at a time, as a person would.

```
build/cmake/hexaforth --typed build/try.fs
```

For example, with this in `build/try.fs`:

```forth
\ the square of a number, and a cell as 16 hex digits
: square ( n -- n*n ) dup * ;
: .cell ( x -- ) dup hi32 .x .x ;
7 square .
-1 .cell
s" hello" type
1 2 +
```

the command prints:

```
\ the square of a number, and a cell as 16 hex digits  ok
: square ( n -- n*n ) dup * ;  ok
: .cell ( x -- ) dup hi32 .x .x ;  ok
7 square . 49  ok
-1 .cell FFFFFFFF FFFFFFFF  ok
s" hello" type hello ok
1 2 +  ok
bye 
data stack (1 cell, top last): 3
halted
```

Each line of the file is echoed, followed by what it printed and `ok`. At the end `hexaforth` prints the data stack, in hex, and the reason the machine stopped. Quote this output when you report what a word does.

| In the output | Meaning |
|---|---|
| `ok` at the end of a line | the line ran |
| `error: -13 17 5 /mod remainder` | a word is undefined. After the number comes the line as far as the word the error came from, so the undefined word is the last one shown, here `remainder`. The rest of the line is skipped, and the stack is emptied |
| `error: -8` | the dictionary or the code page is full |
| `halted` | the file ran to its end, and the exit status is 0 |
| `halted after 1 uncaught error` | one line had an error, and the exit status is 1 |
| `end of file inside an unfinished definition or [if]` | a `;` or a `[then]` is missing |
| `stack out of range (data -1, return 4)` | a word took a cell from an empty stack. The machine stops there with exit status 3, and the line `while running line 3: ...` gives the line of the file |
| `the machine stopped: it ran on` | an infinite loop, in the line that `while running line` gives |

- `--machine 16` and `--machine 32` run the file on the machine with cells of that size: `build/cmake/hexaforth --machine 16 --typed build/try.fs`. The default is 64-bit cells.
- Keep a scratch file under `build/`, which git ignores.
- `words` prints every name in the dictionary, and `see square` prints a word's definition as the machine has it.
- `doc/words.md` lists every word of the system with its stack comment.

### Developing and debugging a word

Build a word a step at a time, and run the file after each step.

1. Try the words on numbers before defining anything. With `17 5 2dup mod` as the only line of `build/try.fs`, the run ends with

   ```
   data stack (3 cells, top last): 11 5 2
   ```

   so `2dup mod` left the two numbers and the remainder, 2. The stack is printed in hex, and `11` is 17. Add the next words to the line and run it again: `17 5 2dup mod -rot /` leaves `2 3`, the remainder and the quotient.

2. Put the words that worked in a definition, and test it with inputs that differ from each other and whose answers you worked out beforehand. Where the system has a word that does part of the same work, compare with it. The file

   ```forth
   : /mod ( n1 n2 -- rem quot ) 2dup mod -rot / ;
   17 5 /mod . .
   -17 5 /mod . .
   100 7 /mod  100 7 / = .  100 7 mod = .
   ```

   prints

   ```
   : /mod ( n1 n2 -- rem quot ) 2dup mod -rot / ;  ok
   17 5 /mod . . 3 2  ok
   -17 5 /mod . . -3 -2  ok
   100 7 /mod  100 7 / = .  100 7 mod = . -1 -1  ok
   ```

3. When a run fails, read the line the failure names, change one thing, and run again. Two failures look like this:

   ```
   17 5 /mod remainder . error: -13 17 5 /mod remainder 
   17 5 /mod . . . 3 2 
   stack out of range (data -1, return 4) at 31
   the return stack: 322 57e 6e8 2
   while running line 3: 17 5 /mod . . .
   ```

   The error ends with `remainder`, which is the undefined word. In line 3 the third `.` found the stack empty, because `/mod` leaves two cells. Count what each word of the named line takes and leaves, from its stack comment.

4. To see what a definition does an instruction at a time, write `trace-on` after it and then run it:

   ```forth
   : sum ( n -- total ) 0 swap 0 do i + loop ;
   trace-on
   2 sum .
   trace-off
   ```

   From `trace-on` to `trace-off`, `hexaforth` prints a line for each instruction that runs in a definition of the file, once the instruction has run: where it is, what it is called, its fields as `vm_opcodes.h` writes them, and the data and return stacks as it left them, in hex with the top last. The start of that run, and the last turn of the loop:

   ```
   sum              false                N->IN   IN^N      ->T     d+1    r+0       alu      data: 2 0  return: 1 6e8 57e
   sum+1            swap                 N->IN   T<>N,IN-> ->T     d+0    r+0       alu      data: 0 2  return: 1 6e8 57e
   sum+2            false                N->IN   IN^N      ->T     d+1    r+0       alu      data: 0 2 0  return: 1 6e8 57e
   sum+3            (do)         $0494                                              scall    data: 0 fffffffffffffffe  return: 1 6e8 57e 0
   ```

   ```
   sum+4            >r                   T->IN   IN->      ->R     d-1    r+1       alu      data: 0  return: ... 6e8 57e 0 ffffffffffffffff
   sum+5            i            $04ef                                              scall    data: 0 1  return: ... 6e8 57e 0 ffffffffffffffff
   sum+6            +                    T->IN   IN+N      ->T     d-1    r+0       alu      data: 1  return: ... 6e8 57e 0 ffffffffffffffff
   sum+7            r>                   R->IN   IN->      ->T     d+1    r-1       alu      data: 1 ffffffffffffffff  return: 1 6e8 57e 0
   sum+8            (loopnext)   $04a2                                              scall    data: 1 0 0  return: 1 6e8 57e 0
   sum+9            sum+11       $1cf4                                              0jump    data: 1 0  return: 1 6e8 57e 0
   sum+11           (loopdone)   $03ee                                              scall    data: 1  return: 1 6e8 57e
   sum+12           exit                 T->IN   IN->      ->T     d+0    r-1  RET  alu      data: 1  return: 1 6e8
   ```

   A `0` in a definition is compiled as `false`. A call to a word of the system, such as `i`, is one line, printed when that word has returned. A call to another definition of the file is followed into it. A jump or a branch is called by the place it goes to. The trace stops after 500 instructions in one run, which `TRACE_MOST=2000 build/cmake/hexaforth --typed build/try.fs` raises. On a machine other than the reference VM the two words do nothing.

- `.s` prints the data stack, the top last, and leaves it as it was: `1 2 3 .s` prints `1 2 3`.
- `see /mod` prints the definition as the machine compiled it.
- A comment is `\` and the rest of the line, or `( ... )` with a space after the `(`. Do not start a comment with `#`, `//` or `/*`, because Forth reads each of them as a word, which is undefined here, and the line ends in `error: -13`. A name inside a stack comment, such as `rem`, is a label for the reader. Do not use one as a word.
- A test whose two inputs are equal hides a word that confuses them, so give the inputs different values.
- Report the output of a run made after the last edit to the file.

### What differs from standard Forth

| Standard Forth | Here |
|---|---|
| an address counts bytes | an address `a` is the number of a cell, so `cell+` adds 1. `c@` and `c!` take a byte address `c-addr`, which `>c` makes from a cell address |
| `u.` | absent. `.` prints a signed decimal number, and `.x` prints the low 32 bits as 8 hex digits |
| `."` | absent. Write `s" text" type` |
| `/mod`, `*/`, `*/mod`, `m*`, `fm/mod`, `sm/rem` | absent. `/`, `mod`, `um*`, `um/mod` and `u/mod` are there |
| `abort`, `abort"` | absent. Write `-1 throw` |
| `spaces`, `move`, `erase`, `char+`, `chars` | absent. `cmove` and `cells-move` are there |
| `<#`, `#`, `#s`, `#>`, `hold`, `sign`, `.r`, `u.r` | absent |
| `pick`, `roll`, `2>r`, `2r>` | absent. `3rd` copies the third cell |
| `case`, `within`, `value`, `defer`, `pad` | absent |
| `find`, `word`, `>body`, `environment?` | absent. `sfind` and `parse-name` are there |
| numbers | decimal. `$ff` is hex, and `'q'` is the character q |
| `um*` | takes a step for every bit of a cell, so a word that multiplies heavily defines its own |

### The core words

`x` is any cell, `n` a signed number, `u` an unsigned number, `d` and `ud` a number in two cells with the high cell on top, `f` a flag (0 or -1), `c` a character, `a` a cell address, `c-addr` a byte address and `xt` an execution token.

| Word | Stack | What it does |
|---|---|---|
| `dup` | `( x -- x x )` | copies the top cell |
| `drop` | `( x -- )` | removes it |
| `swap` | `( x1 x2 -- x2 x1 )` | exchanges the top two |
| `over` | `( x1 x2 -- x1 x2 x1 )` | copies the second |
| `nip` | `( x1 x2 -- x2 )` | removes the second |
| `tuck` | `( x1 x2 -- x2 x1 x2 )` | copies the top under the second |
| `rot` | `( x1 x2 x3 -- x2 x3 x1 )` | brings the third to the top |
| `-rot` | `( x1 x2 x3 -- x3 x1 x2 )` | puts the top under the other two |
| `?dup` | `( x -- 0 \| x x )` | copies the top if it is non-zero |
| `2dup` | `( x1 x2 -- x1 x2 x1 x2 )` | copies the top pair |
| `2drop` | `( x1 x2 -- )` | removes the top pair |
| `2swap` | `( x1 x2 x3 x4 -- x3 x4 x1 x2 )` | exchanges the top two pairs |
| `2over` | `( x1 x2 x3 x4 -- x1 x2 x3 x4 x1 x2 )` | copies the second pair |
| `depth` | `( -- n )` | the number of cells on the stack |
| `+` | `( n1 n2 -- n3 )` | adds |
| `-` | `( n1 n2 -- n3 )` | subtracts n2 from n1 |
| `*` | `( n1 n2 -- n3 )` | multiplies, keeping one cell of the product |
| `/` | `( n1 n2 -- n3 )` | divides n1 by n2, rounding towards zero |
| `mod` | `( n1 n2 -- n3 )` | the remainder of that division |
| `1+` | `( n -- n+1 )` | adds 1 |
| `1-` | `( n -- n-1 )` | subtracts 1 |
| `2*` | `( x -- x' )` | shifts left one bit |
| `2/` | `( n -- n/2 )` | shifts right one bit, keeping the sign |
| `negate` | `( n -- -n )` | changes the sign |
| `abs` | `( n -- u )` | the absolute value |
| `min` | `( n1 n2 -- n3 )` | the smaller |
| `max` | `( n1 n2 -- n3 )` | the larger |
| `um*` | `( u1 u2 -- ud )` | the two-cell product of two unsigned cells |
| `um/mod` | `( ud u -- rem quot )` | divides a two-cell number by a cell |
| `u/mod` | `( u1 u2 -- rem quot )` | divides u1 by u2 |
| `and` | `( x1 x2 -- x3 )` | bitwise and |
| `or` | `( x1 x2 -- x3 )` | bitwise or |
| `xor` | `( x1 x2 -- x3 )` | bitwise exclusive or |
| `invert` | `( x -- x' )` | flips every bit |
| `lshift` | `( x u -- x' )` | shifts x left u bits |
| `rshift` | `( x u -- x' )` | shifts x right u bits, filling with zeros |
| `hi32` | `( dw -- w )` | the upper 32 bits of a 64-bit cell |
| `lo32` | `( dw -- w )` | the lower 32 bits |
| `=` | `( x1 x2 -- f )` | true if they are equal |
| `<>` | `( x1 x2 -- f )` | true if they differ |
| `<` | `( n1 n2 -- f )` | true if n1 is less, signed |
| `>` | `( n1 n2 -- f )` | true if n1 is greater, signed |
| `u<` | `( u1 u2 -- f )` | true if u1 is less, unsigned |
| `u>` | `( u1 u2 -- f )` | true if u1 is greater, unsigned |
| `0=` | `( x -- f )` | true if x is zero |
| `0<>` | `( x -- f )` | true if x is non-zero |
| `0<` | `( n -- f )` | true if n is negative |
| `0>` | `( n -- f )` | true if n is positive |
| `true` | `( -- -1 )` | the true flag |
| `false` | `( -- 0 )` | the false flag |
| `@` | `( a -- x )` | fetches the cell at a |
| `!` | `( x a -- )` | stores x in the cell at a |
| `+!` | `( n a -- )` | adds n to the cell at a |
| `c@` | `( c-addr -- c )` | fetches a byte |
| `c!` | `( c c-addr -- )` | stores a byte |
| `cell+` | `( a -- a+1 )` | the address of the next cell |
| `>c` | `( a -- c-addr )` | the byte address of a cell |
| `here` | `( -- a )` | the cell that the next `,` fills |
| `,` | `( x -- )` | puts a cell at `here` |
| `c,` | `( c -- )` | puts a byte there |
| `allot` | `( n -- )` | reserves n bytes |
| `emit` | `( c -- )` | prints a character |
| `cr` | `( -- )` | ends the line |
| `space` | `( -- )` | prints a space |
| `type` | `( c-addr u -- )` | prints u characters |
| `.` | `( n -- )` | prints a signed decimal number and a space |
| `.x` | `( u -- )` | prints 8 hex digits and a space |
| `.x2` | `( u -- )` | prints 2 hex digits and a space |
| `.s` | `( -- )` | prints the data stack, the top last, and keeps it |
| `s>d` | `( n -- d )` | makes a cell a two-cell number |
| `d+` | `( d1 d2 -- d3 )` | adds two-cell numbers |
| `dnegate` | `( d -- -d )` | changes the sign of a two-cell number |
| `execute` | `( xt -- )` | runs the word xt names |
| `'` | `( "name" -- xt )` | the execution token of the next word in the text |
| `catch` | `( xt -- 0 \| n )` | runs xt, and returns what it threw, or 0 |
| `throw` | `( n -- )` | returns to the nearest `catch` if n is non-zero |
| `words` | `( -- )` | prints every name |
| `see` | `( "name" -- )` | prints a word's definition |
| `bye` | `( -- )` | halts the machine |

Control structures and defining words work as in standard Forth, inside a definition:

| Written | Notes |
|---|---|
| `: name ... ;` | defines a word. `recurse` calls the word being defined, and `exit` returns from it |
| `f if ... then` and `f if ... else ... then` | |
| `begin ... f until`, `begin ... f while ... repeat`, `begin ... again` | |
| `limit start do ... loop` | `i` is the index, `leave` ends the loop, and `n +loop` steps by n. `?do` skips the loop when limit equals start |
| `>r`, `r>`, `r@`, `rdrop` | move a cell to the return stack, move it back, copy it, and remove it. Balance them within one definition |
| `variable name`, `x constant name`, `create name` | `create name 0 , 0 ,` makes two cells at the address `name` gives |
| `[char] q` in a definition, `char q` outside one | the character q |

Style:

- Keep working values on the stacks, where T, N and R are inputs of the ALU. A value that is the same on every turn of a loop goes on the return stack.
- A loop over memory uses its addresses as the count: `bounds begin 2dup xor while ... 1+ repeat 2drop`.
- Give every definition a stack comment after its name.

### The system's source

The system is the Forth in `forth/`, and `cmake --build build/cmake` compiles it into the image. Source in `forth/` is written a little differently from text for `hexaforth --typed`:

| In `forth/` | Typed |
|---|---|
| `d# 10`, `h# ff` | `10`, `$ff` |
| `[char] q` | `'q'` |
| `header name : name ... ;` for a word that another file or the prompt uses | `: name ... ;` |

`build/cmake/disasm_hex build/64/nuc.hex NAME --no-color` prints the instructions of a word of the system. `hexaforth-debug` runs like `hexaforth` and traces every instruction with its fields and both stacks.

## Building and testing

| Command | What it does |
|---|---|
| `cmake -S . -B build/cmake` | configures the build, installing any missing dependency with Homebrew or apt |
| `cmake --build build/cmake` | builds every tool and runs `quick`, the tests that need only the reference VM |
| `cmake --build build/cmake --target check` | runs every test, on the reference VM, QEMU and the Verilog |
| `ctest --test-dir build/cmake -V -R NAME` | runs the tests whose names contain `NAME`, building what they need first |

Run `check` after any change to `cross.fs`, `nuc.fs` or the machine. Code is safe at a cell size only if a test has run it at that size. Run a tool by hand only after `cmake --build build/cmake` on its own, which leaves every tool in `build/cmake` up to date.

## Tools

A target is run with `cmake --build build/cmake --target NAME`.

| Command or target | What it shows |
|---|---|
| `build/cmake/hexaforth --typed FILE.fs` | types a file of Forth at the built system, and prints what it printed, the stack and why the machine stopped (Writing Forth, above) |
| `build/cmake/hexaforth IMAGE.hex [DISK.img [MEMORY.out]]` | runs an image on the reference VM with stdin as its console, and writes the VM's memory to `MEMORY.out` when it stops |
| `build/cmake/hexaforth-debug IMAGE.hex` | the same run with a trace of every instruction |
| `build/cmake/disasm_hex IMAGE.hex [NAME...]` | an image's code, one instruction to a line, for every word or for the named ones |
| `build/cmake/disasm_hex IMAGE.hex --typed FILE.fs` | what the nucleus compiles when `FILE.fs` is typed at it |
| `build/cmake/disasm_hex IMAGE.hex NAME --counts WORK` | how many times each instruction of a word ran |
| target `examine` | an image's words, headers and bytes, by page and by file |
| target `bench` | what each benchmark costs in cycles on the Verilog CPU |
| target `profile` | where the desktop's instructions go, by file, by word and by kind of instruction |

## Where things are

| Path | What is there |
|---|---|
| `MODEL.md`, `UNDERSTANDING.md`, `DESKTOP.md` | the machine's specification, why it is designed as it is, and how to use and program the desktop |
| `vm.c`, `vm_loop.h`, `vm_step.h` | the reference VM |
| `vm_constants.h`, `vm_instruction.h`, `vm_opcodes.h` | the machine's sizes, the fields of an instruction, and the names of the fields and the basewords |
| `forth/cross.fs` | the cross-compiler, which gforth runs, and its peephole |
| `forth/basewords.fs` | the 64-bit machine's definition for `cross.fs`, which is generated |
| `forth/nuc.fs` | the nucleus: the kernel on page 0, the interpreter, the compiler, the dictionary and tasks |
| `forth/join.fs`, `forth/compose.fs` | the nucleus's own peephole |
| `forth/system.fs`, `forth/apps.fs` | the system's files and the applications, in the order their code is placed |
| `util/` | the tools: the disassembler, the image readers, the generators and the disk maker |
| `test/` | the tests |
| `bench/` | the benchmarks, the profile and the reports |
| `baremetal/` | the AArch64 kernels and the QEMU test harness |
| `rtl/` | the Verilog CPU, its memory and its test bench |
| `cmake/` | the build, one file to a subject |
| `doc/` | the other documents, among them `doc/words.md`, which lists every word with its stack comment, and `doc/formatting.md` on how Forth source is laid out |
