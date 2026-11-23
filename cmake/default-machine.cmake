#
# The default machine is the one vm_constants.h describes when no machine
# is given.  Its definition for cross.fs is forth/basewords.fs, which is
# checked in.  This part has:
#
#   the two other generated Forth files, forth/folds.fs and forth/fused.fs
#   tabled(), which builds a system's decompiler table
#   default_machine_is(), which adds the default machine's tests and report
#   the tests that need no image: the instructions, and the search for
#     fewer instructions
#
# The default machine itself is built in cmake/machines.cmake, like every
# other machine, and only once.  It is the first machine in util/machines.
# It is built with forth/basewords.fs, and its decompiler table is
# forth/fused.fs.
#

# Two files of Forth are made, and checked in: each is made by a command of
# its own, from what that command says, and is among the sources of every
# image (FORTH_SOURCES).
#
# forth/folds.fs is what the nucleus's peephole is told of numbers, asked
# of the machine (util/joins.c).  It reads no image.
stepping_tool(joins util/joins.c)
set(FOLDS_FS ${CMAKE_SOURCE_DIR}/forth/folds.fs)
add_custom_command(
        OUTPUT            ${FOLDS_FS}
        COMMAND           joins forth/folds.fs
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        DEPENDS           joins)

# forth/fused.fs is the decompiler's table for the default machine: the
# words the decompiler shows each instruction as (util/fused.c).  Each
# machine has its own instructions, so every other system that includes
# the decompiler has its own table, in its directory of build/ (system,
# cmake/machines.cmake).  tabled() builds one.
#
# A table is made from an image and is also part of the image, so it
# cannot be made from the image that contains it.  tabled() builds in three
# steps:
#
#   1. an image with an empty table, in untabled/, from every source except
#      a table
#   2. the table, from that image
#   3. the nucleus, with the table
#
# The table is the last data in an image (forth/system.fs), so its length
# moves nothing else, and the images of steps 1 and 3 have the same
# instructions.  table_test() adds the test that checks this: the table
# made from the nucleus must be the table the nucleus was built with.
#
# The table also covers the instructions the nucleus's own compiler makes
# from the same source.  Those come from the boot image (BOOT=on: the
# system without the applications), which is built with the empty table in
# DIRECTORY/untabled/boot/.  Two things are done with it there:
#
#   The boot image's own code is typed at the nucleus.  The image is built
#   again with the peephole off, the two builds are cut into the same
#   straight-line runs (bench/runs.c), and each run is typed at the first
#   build's nucleus on the reference VM, which reports the instructions it
#   compiled (bench/typed.c).  fused takes them with --made.  The boot
#   image is used because the whole system with the peephole off does not
#   fit the machine of 64K cells.
#
#   The applications are compiled by a booted machine.  A disk the desktop
#   can start from is made from the boot image, the reference VM boots the
#   image from that disk and is stopped once it has booted, and fused takes
#   the VM's memory and the disk with --booted.
#
#   tabled(DIRECTORY TABLE      where the nucleus is built, and the table
#                               to make for it
#          OF text              the system's name, for the build's messages
#          [WORDS file]         the machine's definition, the settings for
#          [SET name=value...]  cross.fs, and what the images depend on,
#          [AFTER what...]      as image() takes them
#          [BOOTED files...])   the four files of a booted machine, as
#                               fused takes them after --booted: for a
#                               system the reference VM cannot boot,
#                               because it lacks the disk the ROM drives
#
# tabled() sets NUCLEUS_MADE to the --made and --booted arguments for
# fused, and BOOTED_FOR to the four files of the booted machine.
stepping_tool(fused util/fused.c util/image.c)
target_compile_definitions(fused PRIVATE GFORTH="${GFORTH}")
# The reference VM (main.c): hexaforth IMAGE.hex [DISK.img [MEMORY.out]].
machine_tool(hexaforth main.c vm.c)
# runs cuts two builds of an image into the same runs, and typed types the
# runs at the nucleus (bench/runs.c, bench/typed.c).
machine_tool(runs bench/runs.c)
machine_tool(typed bench/typed.c)
set(FUSED_FS ${CMAKE_SOURCE_DIR}/forth/fused.fs)
# FORTH_SOURCES is the files CMake found in forth/ when it was configured.
# A generated file that did not exist then is added here, so that every
# image still depends on it.
foreach(MADE ${FOLDS_FS} ${FUSED_FS})
    if(NOT MADE IN_LIST FORTH_SOURCES)
        list(APPEND FORTH_SOURCES ${MADE})
    endif()
endforeach()
# The sources of an image that is built with another table: every source
# except forth/fused.fs.
set(SOURCES_BUT_TABLE ${FORTH_SOURCES})
list(REMOVE_ITEM SOURCES_BUT_TABLE ${FUSED_FS})
function(tabled AT TABLE)
    cmake_parse_arguments(IT "" "OF;WORDS" "SET;AFTER;BOOTED" ${ARGN})
    set(UNTABLED ${AT}/untabled)
    set(WORDS)
    if(IT_WORDS)
        set(WORDS WORDS ${IT_WORDS})
    endif()
    file(RELATIVE_PATH SAID ${CMAKE_SOURCE_DIR} ${TABLE})
    image(${UNTABLED}
          ${WORDS}
          SET ${IT_SET} FUSED=${UNTABLED}/fused.fs
          MAKES hex tables
          FIRST fused ${UNTABLED}/fused.fs
          FROM ${SOURCES_BUT_TABLE}
          AFTER fused ${IT_AFTER}
          IS "The nucleus of ${IT_OF} with an empty decompiler table")
    set(NUCLEUS ${IMAGE_FILES})
    set(BOOT ${UNTABLED}/boot)
    image(${BOOT}
          ${WORDS}
          SET ${IT_SET} FUSED=${UNTABLED}/fused.fs BOOT=on
          MAKES hex tables inline
          FROM ${SOURCES_BUT_TABLE}
          AFTER ${NUCLEUS} ${IT_AFTER}
          IS "Its boot image, with that table")
    set(BOOTS ${IMAGE_FILES})
    image(${BOOT}/plain
          ${WORDS}
          SET ${IT_SET} FUSED=${UNTABLED}/fused.fs BOOT=on PEEPHOLE=off
          MAKES hex tables
          FIRST ${COPY} ${BOOT}/nuc.inline ${BOOT}/plain/nuc.inline
          FROM ${SOURCES_BUT_TABLE}
          AFTER ${BOOTS} ${IT_AFTER}
          IS "The same, with the peephole off")
    add_custom_command(
            OUTPUT            ${BOOT}/typed.txt
            COMMAND           runs ${BOOT}/plain/nuc.hex ${BOOT}/plain/nuc.tables ${BOOT}/nuc.hex ${BOOT}/nuc.tables
                                > ${BOOT}/runs.txt 2> ${BOOT}/runs.log
            COMMAND           typed type ${BOOT}/runs.txt ${BOOT}/nuc.hex ${BOOT}/nuc.tables
                                > ${BOOT}/typed.fs 2> ${BOOT}/typed.log
            COMMAND           hexaforth ${BOOT}/nuc.hex < ${BOOT}/typed.fs > ${BOOT}/typed.out 2>&1
            COMMAND           typed read ${BOOT}/runs.txt ${BOOT}/typed.out > ${BOOT}/typed.txt 2> ${BOOT}/typed.log
            COMMENT           "The instructions the nucleus compiles from that image's code (each step's log is in ${BOOT})"
            DEPENDS           ${BOOTS} ${IMAGE_FILES} runs typed hexaforth)
    set(BOOTED ${IT_BOOTED})
    if(NOT BOOTED)
        set(BOOTED ${BOOT}/nuc.hex ${BOOT}/nuc.tables ${BOOT}/memory ${BOOT}/booted.img)
        add_custom_command(
                OUTPUT            ${BOOT}/memory ${BOOT}/booted.img
                COMMAND           mkdisk --boot-desk ${BOOT}/nuc.hex ${BOOT}/booted.img ${BOOT}/booted.ipl
                                    ${BOOT}/nuc.tables ${CMAKE_SOURCE_DIR}/forth/apps.fs > ${BOOT}/disk.log 2>&1
                COMMAND           ${WITH} STOP_BUDGET=2000000000
                                    $<TARGET_FILE:hexaforth> ${BOOT}/nuc.hex ${BOOT}/booted.img ${BOOT}/memory
                                    < ${CMAKE_SOURCE_DIR}/bench/idle.fs > ${BOOT}/booted.out 2>&1
                COMMENT           "Booting that image from its disk on the reference VM (its output is in ${BOOT}/booted.out)"
                DEPENDS           ${BOOTS} mkdisk hexaforth ${SOURCES_BUT_TABLE} bench/idle.fs)
    endif()
    set(MADE --made ${BOOT}/typed.txt --booted ${BOOTED})
    add_custom_command(
            OUTPUT            ${TABLE}
            COMMAND           fused ${UNTABLED}/nuc.hex ${UNTABLED}/nuc.tables ${SAID} ${MADE}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT           "The decompiler's table of ${IT_OF}"
            DEPENDS           ${NUCLEUS} ${BOOT}/typed.txt ${BOOTED}
                              fused)
    set(NUCLEUS_MADE ${MADE} PARENT_SCOPE)
    set(BOOTED_FOR ${BOOTED} PARENT_SCOPE)
endfunction()
# table_test(TEST DIRECTORY TABLE MADE args... NEEDS targets...) adds the
# test that a nucleus was built with its own table.  fused makes the table
# again from the nucleus in DIRECTORY and from the --made and --booted
# arguments tabled() left (MADE).  The test fails if TABLE differs, and
# lists the entries TABLE lacks.  NEEDS is what builds that nucleus.
function(table_test TEST AT TABLE)
    cmake_parse_arguments(IT "" "" "MADE;NEEDS" ${ARGN})
    file(RELATIVE_PATH SAID ${CMAKE_SOURCE_DIR} ${TABLE})
    add_test(NAME ${TEST}
             COMMAND fused ${AT}/nuc.hex ${AT}/nuc.tables ${SAID} ${IT_MADE} --same
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    needs(${TEST} fused ${IT_NEEDS})
endfunction()

# forth-env makes the two generated files.  forth/fused.fs is made from the
# default machine's images (system, cmake/machines.cmake).  Every nucleus
# that includes either file depends on forth-env, so the commands that make
# them run in one target only.
add_custom_target(forth-env
        ALL
        DEPENDS           ${FOLDS_FS} ${FUSED_FS})

# basewords.fs opcode description for forth environment
add_custom_command(
        COMMAND           ${CMAKE_BINARY_DIR}/basewords-fs-gen
                            ${CMAKE_SOURCE_DIR}/forth/basewords.fs
        OUTPUT            ${CMAKE_SOURCE_DIR}/forth/basewords.fs
        DEPENDS           basewords-fs-gen)
tool(basewords-fs-gen util/baseword_fs_gen.c util/image.c vm_opcodes.c vm_debug.c)

# Test case generator
tool(generate-test-cases test/generate_test_cases.c vm_opcodes.c vm_debug.c)

# Generate test_cases.fs from tests.h
# (It writes forth/test_cases.fs, from where the source is.)
add_custom_command(
        COMMAND           ${CMAKE_BINARY_DIR}/generate-test-cases
        OUTPUT            ${CMAKE_SOURCE_DIR}/forth/test_cases.fs
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        DEPENDS           generate-test-cases
                          test/tests.h)

# Compile test_cases.fs to hex.  It is in build/, and its log has a name of
# its own.
image(${BUILD}
      SOURCE test_cases.fs
      FROM ${CMAKE_SOURCE_DIR}/forth/test_cases.fs ${CMAKE_SOURCE_DIR}/forth/cross.fs
      LOG test_cases.log
      IS "The tests, as cross.fs compiles them")

# The VM itself, with the trace of every instruction (DEBUG): what is
# linked with it is compiled with the trace too
add_library(vm_core_debug vm.c vm_opcodes.c vm_debug.c util/stack.c util/image.c)
target_compile_definitions(vm_core_debug PUBLIC DEBUG)

# VM tests
add_executable(hexaforth_test vm_opcodes.c test/main.c test/vm_test.c test/compiler.c)
target_link_libraries(hexaforth_test vm_core_debug)
target_compile_definitions(hexaforth_test PUBLIC TEST)

# The cases the kernels and the Verilog are run against, each with what
# vm.c left of it (baremetal/difftest_dump.c): difftest_dump CASES.bin
# [CELL], of a machine of that cell.
tool(difftest_dump baremetal/difftest_dump.c vm.c vm_opcodes.c vm_debug.c util/stack.c util/image.c test/compiler.c test/vm_test.c)

# What reaches each word of the kernel (util/reach.h): nothing of it is to
# be there that nothing comes to.  It has the machine in it, and is built
# without the trace.
tool(kernel_test test/kernel_test.c vm_opcodes.c vm_debug.c util/stack.c util/image.c)

# The search for the fewest instructions that do the work of a run, and of
# a turn of a loop (util/fewer.h, util/turns.h), held to runs whose fewest
# is known.  It has fuse.h in it, which has the machine.  How long a search
# may take, in what it looks at, is said here and nowhere else: what is
# looked for, how deep and how long, is the run's or the turn's to say.
set(SEARCH_STATES 1000000 CACHE STRING "the states a search for fewer instructions looks at before it is given up (util/fewer.h)")
tool(fewer_test test/fewer_test.c util/image.c)
target_compile_definitions(fewer_test PRIVATE SEARCH_STATES=${SEARCH_STATES})

# The reference VM with the trace of every instruction (DEBUG, above).  The
# reference VM itself is `hexaforth` (above): one program, which is of the
# machine whose image it is given.
add_executable(hexaforth-debug main.c)
target_link_libraries(hexaforth-debug vm_core_debug)

# The tests that need no image: the instructions, compiled two ways and
# run; and the search for fewer instructions.
add_custom_target(test-cases DEPENDS ${BUILD}/test_cases.hex)
add_test(NAME instructions COMMAND hexaforth_test WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
needs(instructions hexaforth_test test-cases forth-env)
add_test(NAME fewer COMMAND fewer_test WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
needs(fewer fewer_test)

# What an image is made of: its pages of code, the source files its code is
# from, and the rest of its memory (util/examine.c, util/layout.h).  The
# kernel is to fit in its ROM on every machine, and this is what says how
# long it is on each.  examined() makes the target that says it of the image
# in a directory: `examine`, of the default machine's nucleus, and
# examine-NAME, of the system NAME's.
tool(examine-image util/examine.c vm_opcodes.c vm_debug.c util/image.c)
function(examined TARGET AT)
    report(${TARGET}
            COMMAND           examine-image ${AT}/nuc.hex ${AT}/nuc.tables ${AT}/nuc.sizes > ${AT}/examine.txt
            COMMAND           ${CAT} ${AT}/examine.txt
            COMMAND           ${KEEP} examine ${AT}/nuc.hex ${AT}/examine.txt)
endfunction()

# default_machine_is(NAME DEFINITION) adds the tests and the report of the
# default machine, whose nucleus is the system NAME's:
#
#   kernel            what reaches each word of the kernel (kernel_test)
#   examine           what the image is made of
#   default-machine   the system NAME has the default machine's sizes
#
# Two places state those sizes: vm_constants.h, which forth/basewords.fs is
# generated from, and util/machines.  DEFINITION is the definition
# generated from the sizes in util/machines.  The test default-machine
# fails if it differs from forth/basewords.fs, the file the system is built
# with.
function(default_machine_is NAME DEFINITION)
    set(AT ${BUILD}/${NAME})
    add_test(NAME kernel COMMAND kernel_test ${AT}/nuc.hex ${AT}/nuc.tables WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    needs(kernel kernel_test machine-${NAME})
    add_test(NAME default-machine
             COMMAND ${CMAKE_COMMAND} -E compare_files ${DEFINITION} ${CMAKE_SOURCE_DIR}/forth/basewords.fs)
    needs(default-machine machine-${NAME})
    on_the_vm(kernel default-machine)
    examined(examine ${AT})
endfunction()

# The names cross.fs notes as defined again (cross.fs, names defined again):
# forth/shadowed.fs is a program that has one of each kind, and two that
# are not to be noted, and test/shadowed.shadows is what cross.fs is to
# write of it.
image(${BUILD}/shadowed
      SOURCE shadowed.fs
      MAKES shadows
      FROM ${CMAKE_SOURCE_DIR}/forth/shadowed.fs ${CMAKE_SOURCE_DIR}/forth/cross.fs
      IS "A program with a name defined again each way that is noted")
add_custom_target(shadowed DEPENDS ${BUILD}/shadowed/shadowed.shadows)
add_test(NAME shadows COMMAND ${CMAKE_COMMAND} -E compare_files ${BUILD}/shadowed/shadowed.shadows test/shadowed.shadows
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
needs(shadows shadowed)

# Tests of cross.fs itself.  forth/loops.fs has a while ended by until and
# a loop with two whiles, and prints what each left: the test loops runs
# it and checks the output.  Each of forth/refused-if.fs, refused-then.fs,
# refused-into.fs and refused-empty.fs has one error that cross.fs must
# refuse, and its test checks the message: a branch that never gets a
# target, a then with no branch to resolve, a word that falls through
# (;into) into a definition other than the one it names, and a word with no
# instructions that falls through and has no header.
image(${BUILD}/loops
      SOURCE loops.fs
      FROM ${CMAKE_SOURCE_DIR}/forth/loops.fs ${CMAKE_SOURCE_DIR}/forth/cross.fs
      IS "A program with loops that end in until and in a second while")
add_custom_target(looped DEPENDS ${BUILD}/loops/loops.hex)
add_test(NAME loops COMMAND hexaforth build/loops/loops.hex WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(loops PROPERTIES PASS_REGULAR_EXPRESSION "321B0")
needs(loops looped hexaforth)
file(MAKE_DIRECTORY ${BUILD}/refused)
function(refused NAME SAYS)
    add_test(NAME refused-${NAME}
             COMMAND ${WITH} BUILD=${BUILD}/refused/ ${RUN_GFORTH} ./cross.fs ./basewords.fs ./refused-${NAME}.fs
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/forth)
    set_tests_properties(refused-${NAME} PROPERTIES PASS_REGULAR_EXPRESSION "${SAYS}")
    needs(refused-${NAME} forth-env)
endfunction()
refused(if "never given where it goes")
refused(then "not a branch that waits")
refused(into "runs on into another")
refused(empty "of no instructions that runs on")
on_the_vm(instructions fewer shadows loops refused-if refused-then refused-into refused-empty)

# The names the system's own source defines again, of every machine's
# image, each said once (util/again.c): what quick and check show before
# they run their tests.
tool(again util/again.c)
