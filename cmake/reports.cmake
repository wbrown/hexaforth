#
# The reports.  Each is a target that builds what it wants, runs it on the
# first machine, and prints what it found; everything it makes on the way
# is kept in a directory of build/ that has the target's name.
#
set(OF ${BUILD}/${FIRST})
set(NUC ${OF}/nuc.hex)
set(TABLES ${OF}/nuc.tables)
set(SIZES ${OF}/nuc.sizes)

# The desktop's test on the VM that counts (COUNTED_SOURCES, with listed in
# cmake/machines.cmake, where what works the desktop on it, profiler, is
# too).
machine_tool(desktop_test_counted test/desktop_test.c ${DESK_SOURCES} ${COUNTED_SOURCES})
target_compile_definitions(desktop_test_counted PRIVATE COUNTING_A_TEST)

# What a machine's display shows, from its memory as the reference VM
# writes it when it has stopped (util/screen.c): `screen`, which the target
# `seen` builds, with what makes a PNG of one (util/png.c).
machine_tool(screen util/screen.c)
tool(png util/png.c)
add_custom_target(seen DEPENDS screen png)

# `hello`: a window made by what is typed at the prompt (doc/hello.fs,
# which DESKTOP.md is about), on the reference VM of the first machine: what
# it printed, and its display as a picture (build/hello/hello.png).
set(HELLO ${BUILD}/hello)
report(hello
        COMMAND           ${MKDIR} ${HELLO}
        COMMAND           hexaforth ${NUC} ${OF}/disk.img,${PICTURES} ${HELLO}/memory.out
                            < doc/hello.fs > ${HELLO}/hello.txt 2>&1
        COMMAND           ${CAT} ${HELLO}/hello.txt
        COMMAND           screen ${NUC} ${HELLO}/memory.out ${HELLO}/hello.pbm
        COMMAND           png ${HELLO}/hello.pbm ${HELLO}/hello.png)

# `screenshot`: the desktop with every window open and HexaPaint in front,
# as the README shows it (doc/desktop.png, which is checked in).  The
# desktop's test is run with a directory of its own for its screens, and
# the screen it leaves when HexaPaint has been drawn in, put behind another
# window and brought back to the front is made a PNG (util/png.c).  The
# target makes it again when the desktop looks otherwise.
set(SHOT ${BUILD}/screenshot)
report(screenshot
        COMMAND           ${MKDIR} ${SHOT}/desktop
        COMMAND           $<TARGET_FILE:desktop_test> ${NUC} ${OF}/disk.img ${PICTURES} ${PICTURE} forth
                            ${SHOT}/desktop names > ${SHOT}/desktop.log 2>&1
        COMMAND           png ${SHOT}/desktop/16-again.pbm doc/desktop.png
        COMMENT           "The desktop worked, and its screen with HexaPaint in front (what its test says is in ${SHOT}/desktop.log)")

# `profile`: where the desktop's instructions go.  The desktop is run on
# the VM that counts and worked through a few things a person would do
# (bench/profile.c, which says at its top what it prints; DETAIL=scroll in
# the environment has it print each word of an activity whose name has that
# in it).  What that work costs is then said three ways from the counts
# (bench/costs.c): each word with all it calls, the loops, and the
# variables, of every source file or of those PROFILE_FILES names.  Last
# the encoding is laid out against the work (util/survey.c), and the loops
# that ran are each looked at for fewer instructions that do a turn
# (bench/shorter.c).
set(PROFILE ${BUILD}/profile)
set(PROFILE_FILES "" CACHE STRING "the source files whose variables the profile says the cost of: all, if none")
stepping_tool(survey util/survey.c util/image.c)
set(PROFILED profiler ${NUC} ${TABLES} ${SIZES} ${OF}/disk.img ${PICTURES} ${PICTURE} forth)    # and where it leaves the work
report(profile
        COMMAND           ${MKDIR} ${PROFILE}
        COMMAND           ${PROFILED} ${PROFILE}
                            > ${PROFILE}/profile.txt 2> ${PROFILE}/profile.log
        COMMAND           ${CAT} ${PROFILE}/profile.txt
        COMMAND           costs ${NUC} ${TABLES} ${SIZES} ${PROFILE}/desktop.counts ${PROFILE} ${PROFILE_FILES}
                            2> ${PROFILE}/costs.log
        COMMAND           survey ${NUC} ${TABLES} ${PROFILE}/desktop.counts ${PROFILE}/survey.txt
                            2> ${PROFILE}/survey.log
        COMMAND           shorter ${NUC} ${TABLES} ${PROFILE}/desktop.counts ${PROFILE}/shorter.txt
                            2> ${PROFILE}/shorter.log
        COMMAND           ${KEEP} profile ${NUC} ${PROFILE}/profile.txt ${PROFILE}/desktop.counts ${PROFILE}/paths.txt
                            ${PROFILE}/loops.txt ${PROFILE}/vars.txt ${PROFILE}/survey.txt ${PROFILE}/shorter.txt
        COMMENT           "The desktop's work, by place (${PROFILE}/profile.log says what went wrong, if anything did)")

# `subtracts`: where the desktop's work takes one number from another, how
# often, and what is taken away (bench/uses.c, which says how it finds a
# word that was compiled in place of a call to it).  The machine has no
# instruction that subtracts: `-` is negate and +, three instructions, and
# 1- is two and a +.  But a fetch and an add are one instruction, and so
# are R and an add: what is taken away costs one where its negation is
# kept in a cell or on the return stack.  This says which of the work's
# subtractions are of something that could be.  The work is counted once
# for an image and kept (build/uses/), and the report is
# build/uses/subtracts.txt.
set(USES ${BUILD}/uses)
stepping_tool(uses bench/uses.c util/image.c)
add_custom_command(
        OUTPUT            ${USES}/desktop.counts
        COMMAND           ${MKDIR} ${USES}
        COMMAND           ${PROFILED} ${USES} > ${USES}/profile.txt 2> ${USES}/profile.log
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT           "The desktop's work, counted (${USES}/profile.log says what went wrong, if anything did)"
        DEPENDS           profiler machine-${FIRST} ${NUC} ${OF}/disk.img picture ${PICTURES})
report(subtracts
        COMMAND           uses ${OF}/plain/nuc.hex ${OF}/plain/nuc.tables ${NUC} ${TABLES} ${USES}/desktop.counts
                            - 1- negate > ${USES}/subtracts.txt
        COMMAND           ${CAT} ${USES}/subtracts.txt
        COMMAND           ${KEEP} subtracts ${NUC} ${USES}/subtracts.txt ${USES}/desktop.counts
        DEPENDS           ${USES}/desktop.counts)

# `peephole`: what the nucleus's compiler is worth (forth/join.fs).  The
# nucleus compiles what is typed and what is read from blocks, and cross.fs
# compiles the image; the system's own source is the code there is to try
# the nucleus's compiler on.
#
#   the image built with the peephole and the image built without are cut
#     into the same straight-line runs (bench/runs.c): a run as it is
#     typed, and what cross.fs made of it
#   each run as typed is given to the nucleus's emit1 at the prompt of the
#     reference VM, an instruction at a time, and the nucleus says what the
#     run is after each, which says what it joined (bench/typed.c)
#   the work is done on the VM that counts: the machine coming up and
#     being stopped (bench/idle.fs), which is taken from the rest; the
#     desktop's test; and the drawing benchmark (bench/draw.fs)
#   and the runs are weighed by that (bench/weigh.c), which says at its top
#     what it prints
#   and what the nucleus joined is held to the machine's step, as the
#     target `pairs` holds every pair there could be (util/pairs.c, seen)
#   and each instruction it made is looked for in the decompiler's table,
#     which is made of what cross.fs put in the image (bench/typed.c, shown)
set(PEEPHOLE ${BUILD}/peephole)
machine_tool(runs bench/runs.c)
machine_tool(typed bench/typed.c)
tool(weigh bench/weigh.c vm_opcodes.c vm_debug.c)
report(peephole
        COMMAND           ${MKDIR} ${PEEPHOLE}/desktop
        COMMAND           runs ${OF}/plain/nuc.hex ${OF}/plain/nuc.tables ${NUC} ${TABLES}
                            > ${PEEPHOLE}/runs.txt 2> ${PEEPHOLE}/runs.log
        COMMAND           typed type ${PEEPHOLE}/runs.txt ${NUC} ${TABLES}
                            > ${PEEPHOLE}/typed.fs 2> ${PEEPHOLE}/typed.log
        COMMAND           hexaforth ${NUC} < ${PEEPHOLE}/typed.fs > ${PEEPHOLE}/typed.out 2>&1
        COMMAND           typed read ${PEEPHOLE}/runs.txt ${PEEPHOLE}/typed.out
                            > ${PEEPHOLE}/typed.txt 2> ${PEEPHOLE}/typed.log
        COMMAND           ${COPY} ${OF}/disk.img ${PEEPHOLE}/disk.img
        COMMAND           ${REMOVE} ${PEEPHOLE}/idle.counts ${PEEPHOLE}/draw.counts ${PEEPHOLE}/desktop.counts
        COMMAND           ${WITH} RUN_COUNTS=${PEEPHOLE}/idle.counts
                            $<TARGET_FILE:counting> ${NUC} ${PEEPHOLE}/disk.img
                            < bench/idle.fs > ${PEEPHOLE}/idle.log 2>&1
        COMMAND           ${WITH} RUN_COUNTS=${PEEPHOLE}/draw.counts RUN_WORK=${PEEPHOLE}/draw.work
                            $<TARGET_FILE:counting> ${NUC} ${PEEPHOLE}/disk.img
                            < bench/draw.fs > ${PEEPHOLE}/draw.log 2>&1
        COMMAND           ${WITH} RUN_COUNTS=${PEEPHOLE}/desktop.counts
                            $<TARGET_FILE:desktop_test_counted> ${NUC} ${PEEPHOLE}/disk.img ${PICTURES}
                            ${PICTURE} forth ${PEEPHOLE}/desktop names > ${PEEPHOLE}/desktop.log 2>&1
        COMMAND           weigh ${PEEPHOLE}/typed.txt ${SIZES} ${PEEPHOLE} desktop draw
                            > ${PEEPHOLE}/peephole.txt 2> ${PEEPHOLE}/weigh.log
        COMMAND           pair-rules seen ${NUC} ${PEEPHOLE}/typed.txt > ${PEEPHOLE}/seen.txt 2> ${PEEPHOLE}/seen.log
        COMMAND           typed shown ${PEEPHOLE}/typed.txt forth/fused.fs
                            > ${PEEPHOLE}/shown.txt 2> ${PEEPHOLE}/shown.log
        COMMAND           ${CAT} ${PEEPHOLE}/peephole.txt ${PEEPHOLE}/seen.txt ${PEEPHOLE}/shown.txt
        COMMAND           ${KEEP} peephole ${NUC} ${PEEPHOLE}/peephole.txt ${PEEPHOLE}/seen.txt ${PEEPHOLE}/shown.txt
                            ${PEEPHOLE}/words.txt ${PEEPHOLE}/typed.txt ${PEEPHOLE}/idle.counts ${PEEPHOLE}/draw.counts
                            ${PEEPHOLE}/draw.work ${PEEPHOLE}/desktop.counts
        COMMENT           "The nucleus's compiler, weighed (each step's log is in ${PEEPHOLE})")

# `pairs`: what the nucleus works out two instructions to be together
# (forth/compose.fs), held to the machine's step for every pair it can
# meet and every three that have a name (util/pairs.c, which says at its
# top what it types and what it prints).  It fails if the nucleus was wrong
# of any.  What was typed and what the nucleus said are in build/pairs/.
set(PAIRS ${BUILD}/pairs)
stepping_tool(pair-rules util/pairs.c util/image.c)
report(pairs
        COMMAND           ${MKDIR} ${PAIRS}
        COMMAND           pair-rules type ${NUC} ${TABLES} > ${PAIRS}/typed.fs
        COMMAND           hexaforth ${NUC} < ${PAIRS}/typed.fs > ${PAIRS}/typed.out 2>&1
        COMMAND           pair-rules read ${NUC} ${PAIRS}/typed.out
        COMMENT           "What the nucleus works two instructions out to be, held to the step (${PAIRS})")

# `phrases`: the phrase book, what to write to get each instruction
# (util/phrases.c, which says at its top what it finds and asks).  It is
# run where the Forth is, since it has cross.fs compile every phrase; the
# report is build/phrases.txt, and its first part is printed.  LONGEST=4 in
# the environment is phrases of up to four words, which takes much longer.
stepping_tool(phrase-book util/phrases.c util/image.c)
target_compile_definitions(phrase-book PRIVATE GFORTH="${GFORTH}")
add_custom_target(phrases
        COMMAND           ${MKDIR} ${BUILD}
        COMMAND           ${WITH_HEADERS} $<TARGET_FILE:phrase-book> ${BUILD}/phrases.txt
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/forth)
