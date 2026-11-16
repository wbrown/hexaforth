#!/bin/sh
# What to write to get each instruction: the phrase book.
#
#   util/phrases.sh
#   LONGEST=4 util/phrases.sh       phrases of up to four words, which takes much longer
#
# Builds util/phrases.c, which says at its top what it finds and asks, and
# runs it where the Forth is, since it has cross.fs compile every phrase.
# The report is build/phrases.txt, and its first part is printed.
cd "$(dirname "$0")/.."
mkdir -p build
# cross.fs has gforth build C (util/fuse.c).  Where gforth is Homebrew's,
# the compiler has to be told where Homebrew's headers are, as
# CMakeLists.txt tells it.
if command -v brew > /dev/null 2>&1; then
    CPATH="$(brew --prefix)/include${CPATH:+:$CPATH}"
    export CPATH
fi
gcc -O2 -w -fcommon -Iutil -o build/phrases util/phrases.c util/image.c -lm || exit 1
cd forth && ../build/phrases ../build/phrases.txt
