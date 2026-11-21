# Formatting Forth source

How the Forth in `forth/` is laid out: by whoever writes it, and by the
formatter (`util/wrap.c`, the target `wrap`), which follows these rules
and no others. The formatter lays out only what does not fit a block's
line, and leaves a line that fits as its writer laid it.

The Forth standard (Forth-2012) says what words do. It says nothing about
layout. The conventions here are Forth's as Leo Brodie gives them in
*Thinking Forth*, chapter 5, "Implementation: Elements of Forth Style"
(<https://thinking-forth.sourceforge.net/>), which the quotations below
are from. Where this tree's source does something else consistently, the
rule says so, and keeps to the tree: the book itself says of its spacing
rules, "Whatever you choose, be consistent."

## The line and the block

Source that goes on the disk is read a block at a time, and a block is
16 lines of 64 characters (`DISK_BLOCK` and `DISK_LINE` in
`vm_constants.h`). The book's rules are written for such blocks
("screens").

- A line is at most 64 characters.
- "The last position of each line should be blank except for: quoted
  strings that continue onto the next line, or the end of a comment." A
  block has nothing between its lines, so a word in the last column runs
  into the first word of the next line.
- A definition is never cut by the end of a block: "it's *never* good
  style to let a colon definition cross screen boundaries. (They should
  never be that long!)" `mkdisk` lays a run of lines that has code in it
  and no blank line in one block. So such a run is at most 16 lines, and
  a definition longer than that is factored.
- "Don't skip a line between each definition. You may, however, skip a
  line between *groups* of definitions." A blank line is also where a
  block may end.
- A table is not a definition. It is read a line at a time, and what it
  lays down goes on from one block to the next, so a table longer than a
  block is cut with a blank line between two of its entries.

## Definitions

- "Begin all definitions at the left edge of the screen, and define only
  one word per line."
- "When definitions continue for more than one line, the subsequent lines
  should always be indented." The book indents 3 spaces, and 3 more for
  each level inside an `if`, a `begin` or a `do`. This tree indents 4,
  and 4 more for each level.
- The stack comment comes right after the name. This tree puts one space
  between them (`: cpop ( -- x )`). The book asks for two; what it asks
  above all is that it is the same everywhere.
- A definition too long for one line has its name and comments on the
  first line, and its code on the lines after, indented:

  ```
  : RIDDANCE  ( thing-never-to-darken-again -- )
     DARKEN  NEVER AGAIN ;
  ```

- A name is never on a line by itself without its stack comment, and a
  word that reads the next word (`:`, `create`, `[char]`, `d#`) is never
  on a different line from it.
- `variable`s, `constant`s and `create`s are "defined one per line ...
  This leaves room for an explanatory comment on the same line." A family
  of them that needs no comment of its own may share lines.

## Phrases

- "1 space between words/numbers within a phrase", "2 or 3 spaces between
  phrases". This tree uses 2.
- A line of code is cut only where a phrase ends:
  - where two spaces are;
  - after an entry of a table (`,` `prow,` `,"`);
  - where neither is, at the end of an `if`, a `begin` or a `do`.

  "Phrasing is a subjective art; I've yet to see a useful set of formal
  rules." Where the writer has not marked where a phrase ends, the line
  is cut by hand, by someone who reads what the words do.
- An `if`, a `begin` or a `do` too long for its line is laid over lines:
  `if` ends its line, what it holds is indented, and `else` and `then`
  start lines at the outer indent. A `begin` keeps its test and `while` on
  its line when they fit there (`begin dup while`).

## Comments

The book names five kinds: stack-effect, data-structure, input-stream,
purpose and narrative.

- **Stack effect.** "Every colon or code definition that consumes and/or
  returns any arguments on the stack must include a stack-effect
  comment", in parentheses after the name: `( before -- after )`.
- **Order.** "Indicate the type of comment by ordering: input-stream
  comments first, stack-effect comments second, purpose comments last."

  ```
  : BLOCK  ( n -- a)  \   ensure block n in buffer at a
  ```

- **Purpose.** What the definition does, in as few words as possible,
  "never more than a full line". It follows the stack comment, with `\`,
  after one space as this tree writes it; and the code then starts on the
  next line. "If necessary, you can put the purpose comment on a second
  line", indented as the code is:

  ```
  : WORD   \   name   ( c -- a)
     \ scan for string delimt'd by "c"; leave at a
     ...  ;
  ```

- **Narrative.** A comment beside a line of code says what that line
  does. "Comments will appear to the right of the source code, but may
  continue to engulf the next line totally if needed": a narrative
  comment too long for its line goes on under itself, at its column. The
  comments of lines one after another are at one column. Where the code
  leaves fewer than 24 characters beside it, the comment goes on the line
  above the code, at its indent; on the line after it, if the code only
  ends an `if` or a loop (`then`, `repeat`). The other comments of its
  column go above their code too, so that they are all one way. That is
  this tree's rule, not the book's.
- **Data structures and variables.** A variable's or a table's comment is
  on its line. A record's fields, one to a line, have their comments in
  a column.
- **More than a line of prose.** "Such descriptions properly belong in
  auxiliary documentation or in the chapter preamble": a paragraph of
  comment before the group of definitions it explains. A purpose comment
  longer than a line is such prose, and goes before its definition.
  Paragraphs are separated by a blank line, not by a line that is only
  `\`.
