#
# The default machine: the one vm_constants.h gives when nothing is said,
# whose definition for cross.fs is forth/basewords.fs, which is checked in.
# Here are the two other files of Forth that are made (forth/folds.fs and
# forth/fused.fs); its nucleus, in build/, which every machine's own is
# made after; and the tests of it: the instructions, the kernel, and the
# search for fewer instructions.
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

# forth/fused.fs is the words the decompiler shows an instruction as
# (util/fused.c).  It is made from an image, and is in the image: so it is
# not made from the image it is in.  An image of the default machine is
# built first with the table of no image, in a directory of its own, from
# every source but the table; the table is made of that one; and the
# nucleus is then built with it.  The table is laid after all an image's
# other data (forth/system.fs), where how long it is moves nothing, so the
# two images have the same instructions, and the test `tables` holds them
# to it: the table of the nucleus is the table it was built with.
stepping_tool(fused util/fused.c util/image.c)
target_compile_definitions(fused PRIVATE GFORTH="${GFORTH}")
set(FUSED_FS ${CMAKE_SOURCE_DIR}/forth/fused.fs)
set(UNTABLED ${BUILD}/untabled)
set(UNTABLED_SOURCES ${FORTH_SOURCES})
list(REMOVE_ITEM UNTABLED_SOURCES ${FUSED_FS})
image(${UNTABLED}
      SET FUSED=${UNTABLED}/fused.fs
      MAKES hex tables
      FIRST fused ${UNTABLED}/fused.fs
      FROM ${UNTABLED_SOURCES}
      AFTER fused
      IS "The nucleus with the decompiler's table of no image")
add_custom_command(
        OUTPUT            ${FUSED_FS}
        COMMAND           fused ${UNTABLED}/nuc.hex ${UNTABLED}/nuc.tables forth/fused.fs
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        DEPENDS           ${UNTABLED}/nuc.hex ${UNTABLED}/nuc.tables
                          fused)

# The nucleus of the default machine.  Every machine's own nucleus is built
# after this one (forth-env, as system says: cmake/machines.cmake), so that
# the two files above are made in one place.
image(${BUILD} MAKES hex tables IS "The nucleus of the default machine")

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

# Compile test_cases.fs to hex.  It is in build/ with the default machine's
# nucleus, so its log has a name of its own.
image(${BUILD}
      SOURCE test_cases.fs
      FROM ${CMAKE_SOURCE_DIR}/forth/test_cases.fs ${CMAKE_SOURCE_DIR}/forth/cross.fs
      LOG test_cases.log
      IS "The tests, as cross.fs compiles them")

# kick off our entire forth environment
add_custom_target(forth-env
        ALL
        DEPENDS           ${BUILD}/nuc.hex)

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
# reference VM itself is `hexaforth` (cmake/machines.cmake): one program,
# which is of the machine whose image it is given.
add_executable(hexaforth-debug main.c)
target_link_libraries(hexaforth-debug vm_core_debug)

# The tests of the default machine: the instructions, compiled two ways and
# run; the kernel, and what comes to each word of it; and the search for
# fewer instructions.
add_custom_target(test-cases DEPENDS ${BUILD}/test_cases.hex)
add_test(NAME instructions COMMAND hexaforth_test WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
needs(instructions hexaforth_test test-cases forth-env)
add_test(NAME kernel COMMAND kernel_test build/nuc.hex build/nuc.tables WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
needs(kernel kernel_test forth-env)
# The decompiler's table is the table of the image it is in (fused.fs, above).
add_test(NAME tables COMMAND fused build/nuc.hex build/nuc.tables forth/fused.fs --same
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
needs(tables fused forth-env)
add_test(NAME fewer COMMAND fewer_test WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
needs(fewer fewer_test)

# What an image is made of: its pages of code, the source files its code is
# from, and the rest of its memory (util/examine.c, util/layout.h).  The
# kernel is to fit in its ROM on every machine, and this is what says how
# long it is on each.  examined() makes the target that says it of the image
# in a directory: `examine`, of the default machine's nucleus, which
# forth-env makes, and examine-NAME, of the system NAME's.
tool(examine-image util/examine.c vm_opcodes.c vm_debug.c util/image.c)
function(examined TARGET AT)
    report(${TARGET}
            COMMAND           examine-image ${AT}/nuc.hex ${AT}/nuc.tables ${AT}/nuc.sizes > ${AT}/examine.txt
            COMMAND           ${CAT} ${AT}/examine.txt
            COMMAND           ${KEEP} examine ${AT}/nuc.hex ${AT}/examine.txt)
endfunction()
image_owned(forth-env ${BUILD})
examined(examine ${BUILD})

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
on_the_vm(instructions kernel tables fewer shadows)

# The names the system's own source defines again, of every machine's
# image, each said once (util/again.c): what quick and check show before
# they run their tests.
tool(again util/again.c)
