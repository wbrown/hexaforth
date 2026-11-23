# Formatting Forth source

How the Forth in `forth/` is formatted, by the person who writes it and by
the formatter (`util/wrap.c`, the target `wrap`). The formatter follows
these rules and no others. It reformats only a line that does not fit a
block's line, and leaves a line that fits as its writer wrote it.

The Forth standard (Forth-2012) defines what words do and has no rules for
layout. The conventions here are the ones Leo Brodie gives in *Thinking
Forth*, chapter 5, "Implementation: Elements of Forth Style"
(<https://thinking-forth.sourceforge.net/>). The quotations below are from
that chapter. Where this tree's source consistently does something else,
the rule says so and follows the tree. The book says of its own spacing
rules, "Whatever you choose, be consistent."

## The line and the block

Source that goes on the disk is read a block at a time, and a block is
16 lines of 64 characters (`DISK_BLOCK` and `DISK_LINE` in
`vm_constants.h`). The book's rules are written for such blocks
("screens").

- A line is at most 64 characters.
- "The last position of each line should be blank except for: quoted
  strings that continue onto the next line, or the end of a comment." A
  block has no characters between its lines, so a word in the last column
  runs into the first word of the next line.
- A definition never crosses the end of a block: "it's *never* good
  style to let a colon definition cross screen boundaries. (They should
  never be that long!)" `mkdisk` puts a run of lines that has code and no
  blank line into one block. So such a run is at most 16 lines, and a
  longer definition must be factored.
- "Don't skip a line between each definition. You may, however, skip a
  line between *groups* of definitions." A blank line is also where a
  block may end.
- A table is not a definition. It is read a line at a time, and the data
  it compiles continues from one block to the next. So a table longer
  than a block is broken with a blank line between two of its entries.

## Definitions

- "Begin all definitions at the left edge of the screen, and define only
  one word per line."
- "When definitions continue for more than one line, the subsequent lines
  should always be indented." The book indents 3 spaces, and 3 more for
  each level inside an `if`, a `begin` or a `do`. This tree indents 4,
  and 4 more for each level.
- The stack comment comes right after the name. This tree puts one space
  between them (`: cpop ( -- x )`). The book asks for two, and asks above
  all for the same spacing everywhere.
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
- A line of code is broken only where a phrase ends:
  - at two spaces;
  - after an entry of a table (`,` `prow,` `,"`);
  - where the line has neither, at the end of an `if`, a `begin` or a
    `do`.

  "Phrasing is a subjective art; I've yet to see a useful set of formal
  rules." Where the writer has not marked the end of a phrase, a person
  who reads what the words do breaks the line by hand.
- An `if`, a `begin` or a `do` that is too long for its line is spread
  over several lines: `if` ends its line, its body is indented, and
  `else` and `then` start lines at the outer indent. A `begin` keeps its
  test and `while` on its line when they fit there (`begin dup while`).

## Comments

A comment says what a word, a line or a table does or is, and only where
the code does not say it. It does not explain why the code is written that
way, warn, compare the code with another way of writing it, or tell its
history. Those belong in the commit message and in
`doc/superoptimizations.md`.

That leaves four kinds of comment: a file's first line; a line after a
word's stack comment; the layout of a record or a format, one field to a
line; and one to three lines for a fact the code does not show.

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
  comment that is too long for its line continues on the next line, in
  the same column. The comments on consecutive lines share one column.
  Where the code leaves fewer than 24 characters beside it, the comment
  goes on the line above the code, at the code's indent. If the code only
  ends an `if` or a loop (`then`, `repeat`), the comment goes on the line
  after it. The other comments in that column then go above their code
  too, so that all of them are placed the same way. This rule is this
  tree's, not the book's.
- **Data structures and variables.** A variable's or a table's comment is
  on its line. A record's fields, one to a line, have their comments in
  a column.
- **More than a line.** "Such descriptions properly belong in auxiliary
  documentation or in the chapter preamble." A comment longer than a
  line goes before its definition, or before the group of definitions it
  describes: the layout of a record or a format, one field to a line, or
  else one to three lines. Anything longer belongs in the documents.
  Paragraphs are separated by a blank line, not by a line that is only
  `\`.
