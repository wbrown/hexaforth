cmake_minimum_required(VERSION 3.17)
project(hexaforth C)

set(DEBUG true)
set(CMAKE_C_STANDARD 99)

# Built as a release unless it is said otherwise: built with no flags, the
# tests do not link.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_C_FLAGS_RELEASE "-save-temps -O2 -fverbose-asm -fcommon")
set(CMAKE_C_FLAGS_DEBUG "-save-temps -g -fverbose-asm -fcommon")

if(DEBUG)
   add_compile_definitions(DEBUG)
endif()

# cross.fs writes its output to build/, which is not checked in
set(MAKE_BUILD_DIR ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/build)

# An image is made from every Forth file: nuc.fs includes system.fs and
# through it the rest, and cross.fs includes its tables.  So an image is
# out of date when any of them changes, and a file that is added is found
# the next time anything is built.  test_cases.fs is made here, and only
# its own image needs it.
file(GLOB FORTH_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/forth/*.fs)
list(FILTER FORTH_SOURCES EXCLUDE REGEX "/test_cases\\.fs$")

# cross.fs asks the machine which instructions do the work of which
# (util/fuse.c, which has vm.c in it), through gforth's C interface.  So an
# image is out of date when the machine changes too.  gforth builds that C
# with libtool, which has to be there; and where gforth is Homebrew's, the
# compiler has to be told where Homebrew's headers are, gforth's own among
# them.
set(MACHINE_SOURCES util/fuse.c util/named.c util/room.c vm.c vm_step.h vm.h vm_opcodes.c
                    vm_opcodes.h vm_instruction.h vm_constants.h util/stack.c)
set(WITH_HEADERS)                   # what is put before a command that has gforth build C
find_program(BREW brew)
if(BREW)
    execute_process(COMMAND ${BREW} --prefix
                    OUTPUT_VARIABLE BREW_PREFIX OUTPUT_STRIP_TRAILING_WHITESPACE)
    set(BREW_CPATH ${BREW_PREFIX}/include)
    if(DEFINED ENV{CPATH})
        set(BREW_CPATH ${BREW_CPATH}:$ENV{CPATH})
    endif()
    set(WITH_HEADERS ${CMAKE_COMMAND} -E env CPATH=${BREW_CPATH})
endif()
set(GFORTH ${WITH_HEADERS} gforth)

# The kernels are run against vm.c on Unicorn, which is a C library
# (baremetal/difftest.c, which asks pkg-config where it is).  Where it is
# not there and Homebrew is, Homebrew is asked to install it.
find_program(PKG_CONFIG pkg-config)
set(NO_UNICORN 1)
if(PKG_CONFIG)
    execute_process(COMMAND ${PKG_CONFIG} --exists unicorn RESULT_VARIABLE NO_UNICORN)
endif()
if(NO_UNICORN AND BREW)
    message(STATUS "Unicorn's library is not here: asking Homebrew to install it")
    execute_process(COMMAND ${BREW} install unicorn RESULT_VARIABLE NO_UNICORN)
    if(NO_UNICORN)
        message(WARNING "Homebrew did not install Unicorn: the kernels cannot be run against vm.c (make -C baremetal test)")
    endif()
endif()

# hexaforth nucleus image from swapforth
add_custom_command(
        COMMAND           ${MAKE_BUILD_DIR}
        COMMAND           ${GFORTH} cross.fs basewords.fs nuc.fs
        OUTPUT            ${CMAKE_SOURCE_DIR}/build/nuc.hex
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/forth
        DEPENDS           forth/basewords.fs
                          ${FORTH_SOURCES}
                          ${MACHINE_SOURCES}
                          basewords-fs-gen)

# basewords.fs opcode description for forth environment
add_custom_command(
        COMMAND           ${CMAKE_BINARY_DIR}/basewords-fs-gen
                            ${CMAKE_SOURCE_DIR}/forth/basewords.fs
        OUTPUT            ${CMAKE_SOURCE_DIR}/forth/basewords.fs
        DEPENDS           basewords-fs-gen)

add_executable(basewords-fs-gen
        util/baseword_fs_gen.c
        vm_opcodes.c
        vm_opcodes.h
        vm.h
        vm_debug.c
        vm_debug.h)

# Test case generator
add_executable(generate-test-cases
        test/generate_test_cases.c
        test/tests.h
        test/vm_test.h
        vm_opcodes.c
        vm_opcodes.h
        vm.h
        vm_debug.c
        vm_debug.h)

# Generate test_cases.fs from tests.h
# (It writes forth/test_cases.fs, from where the source is.)
add_custom_command(
        COMMAND           ${CMAKE_BINARY_DIR}/generate-test-cases
        OUTPUT            ${CMAKE_SOURCE_DIR}/forth/test_cases.fs
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        DEPENDS           generate-test-cases
                          test/tests.h)

# Compile test_cases.fs to hex
add_custom_command(
        COMMAND           ${MAKE_BUILD_DIR}
        COMMAND           ${GFORTH} cross.fs basewords.fs test_cases.fs
        OUTPUT            ${CMAKE_SOURCE_DIR}/build/test_cases.hex
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/forth
        DEPENDS           forth/test_cases.fs
                          forth/basewords.fs
                          ${FORTH_SOURCES}
                          ${MACHINE_SOURCES})

# kick off our entire forth environment
add_custom_target(forth-env
        ALL
        DEPENDS           ${CMAKE_SOURCE_DIR}/build/nuc.hex)

# VM itself
add_library(vm_core
        vm.c
        vm_step.h
        vm.h
        vm_instruction.h
        vm_constants.h
        util/stack.c
        util/stack.h)
iF(DEBUG)
    target_compile_definitions(vm_core PUBLIC DEBUG)
endif()

add_library(vm_core_debug
        vm.c
        vm_step.h
        vm.h
        vm_instruction.h
        vm_opcodes.c
        vm_opcodes.h
        vm_constants.h
        vm_debug.c
        vm_debug.h
        util/stack.c
        util/stack.h)
target_compile_definitions(vm_core_debug
        PUBLIC
        DEBUG)

# VM tests
add_executable(hexaforth_test
        vm_opcodes.c
        vm_opcodes.h
        vm_instruction.h
        test/main.c
        test/vm_test.c
        test/vm_test.h
        test/compiler.c
        test/compiler.h)
target_link_libraries(hexaforth_test
        vm_core_debug)
target_compile_definitions(hexaforth_test
        PUBLIC
        TEST
        DEBUG)

# What reaches each word of the kernel (util/reach.c): nothing of it is to
# be there that nothing comes to.  It has the machine in it, and is built
# without the trace.
add_executable(kernel_test
        test/kernel_test.c
        vm_opcodes.c
        vm_debug.c
        util/stack.c
        util/image.c)
target_compile_options(kernel_test PRIVATE -UDEBUG -w)
target_link_libraries(kernel_test m)

# The search for the fewest instructions that do the work of a run, and of
# a turn of a loop (util/fewer.c, util/turns.c), held to runs whose fewest
# is known.  It has fuse.c in it, which has the machine.
add_executable(fewer_test
        test/fewer_test.c
        util/image.c)
target_compile_options(fewer_test PRIVATE -UDEBUG -w)
target_link_libraries(fewer_test m)

# What an image is made of: its pages of code, the source files its code is
# from, and the rest of its memory (util/layout.c).  The target `examine`
# says it of the nucleus's image.
add_executable(examine-image
        util/examine.c
        vm_opcodes.c
        vm_debug.c
        util/image.c)
target_compile_options(examine-image PRIVATE -UDEBUG -w)
target_link_libraries(examine-image m)
add_custom_target(examine
        COMMAND           ${CMAKE_BINARY_DIR}/examine-image build/nuc.hex build/nuc.tables build/nuc.sizes
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        DEPENDS           examine-image
                          build/nuc.hex)

# (They read build/test_cases.hex and the nucleus's image, from where the
# source is.)
add_custom_target(tests
        ALL
        COMMAND           ${CMAKE_BINARY_DIR}/hexaforth_test 2> ${CMAKE_BINARY_DIR}/hexaforth_test.debug
        COMMAND           ${CMAKE_BINARY_DIR}/kernel_test build/nuc.hex build/nuc.tables
        COMMAND           ${CMAKE_BINARY_DIR}/fewer_test
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        DEPENDS           hexaforth_test
                          kernel_test
                          fewer_test
                          build/test_cases.hex
                          build/nuc.hex)

# Main executable
add_executable(hexaforth
        main.c
        build/nuc.hex)
if(DEBUG)
    target_link_libraries(hexaforth
            vm_core_debug)
else()
    target_link_libraries(hexaforth
            vm_core)
endif()

# Debug executable
if(DEBUG)
add_executable(hexaforth-debug
        main.c
        build/nuc.hex)
target_link_libraries(hexaforth-debug
        vm_core_debug)
target_compile_definitions(hexaforth-debug
        PUBLIC
        DEBUG)
endif()

#
# The machines the system is tried on, and everything that is built for one
# and run on it.
#
# util/machines is the machines.  For each, everything made is in a
# directory of build/ that has its name, which is the bits of its cell:
# the machine's definition for cross.fs (basewords.fs), its nucleus
# (nuc.hex, and what cross.fs writes beside it), the disks its tests are
# given, and what the tests leave.  What is compiled for it is named for it
# too (hexaforth-32, prompt_test-32), and is where CMake builds.
#
# Nothing here is built unless it is asked for: the target `check` builds
# what the tests want and runs them (ctest), and each report is a target of
# its own.
#
set(BUILD ${CMAKE_SOURCE_DIR}/build)
enable_testing()

# A tool or a test: built without the trace, as kernel_test is.
function(tool NAME)
    add_executable(${NAME} EXCLUDE_FROM_ALL ${ARGN})
    target_compile_options(${NAME} PRIVATE -UDEBUG -w)
    target_link_libraries(${NAME} m)
endfunction()

# The targets that are to be built before the tests are run.
function(wanted)
    set_property(GLOBAL APPEND PROPERTY WANTED ${ARGN})
endfunction()

# The three that the target `tests` runs are tests like any other.
add_custom_target(test-cases DEPENDS ${BUILD}/test_cases.hex)
add_test(NAME instructions COMMAND hexaforth_test WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME kernel COMMAND kernel_test build/nuc.hex build/nuc.tables WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME fewer COMMAND fewer_test WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
wanted(hexaforth_test kernel_test fewer_test test-cases forth-env)

# What makes a disk, and the Mandrill, as the USC-SIPI image database has
# it: a TIFF that is not compressed, which is what the system reads.
tool(mkdisk util/mkdisk.c)
set(PICTURE ${BUILD}/mandrill.tiff)
add_custom_command(
        OUTPUT            ${PICTURE}
        COMMAND           ${MAKE_BUILD_DIR}
        COMMAND           curl -sSfL --remove-on-error -o ${PICTURE}
                            "https://sipi.usc.edu/database/download.php?vol=misc&img=4.2.03"
        VERBATIM)
add_custom_target(picture DEPENDS ${PICTURE})

# The seeds the tests that try things by chance are each run with.
set(SEEDS 3 4 CACHE STRING "the seeds the tests that try things by chance are run with")

# What the reference VM is made of, the nucleus on it typed at
# (test/nucleus.c), and the desktop on it (test/deskvm.c) with what looks at
# its screen.
set(VM_SOURCES vm.c vm_opcodes.c vm_debug.c util/stack.c)
set(TYPED_SOURCES test/nucleus.c ${VM_SOURCES} util/image.c)
set(DESK_SOURCES test/deskvm.c test/deskcheck.c test/tables.c test/dither.c bench/tiff.c)

# A machine: its name, and what the C compiler is told for it.  Its
# definition is made by the generator built for it, and its nucleus by
# cross.fs from that; and the nucleus again with the peephole off, which
# reads the calls the first build compiled in place (nuc.inline) and so is
# made after it, in a directory of its own.
#
# What is made for a machine is the target machine-NAME's to make, and the
# nucleus with the peephole off is plain-NAME's: whatever needs either
# depends on the target, so that each is made in one place.  made() says a
# file is the first one's.
function(made NAME)
    set_property(GLOBAL APPEND PROPERTY MADE_FOR_${NAME} ${ARGN})
endfunction()

function(machine NAME)
    set(AT ${BUILD}/${NAME})
    set(DEFINITIONS_${NAME} ${ARGN} CACHE INTERNAL "what the C compiler is told for the machine ${NAME}")
    set(CELL ${ARGN})
    list(FILTER CELL INCLUDE REGEX "^CELL_BITS=")
    set(CELL_OF_${NAME} ${CELL} CACHE INTERNAL "and of that, its cell")

    tool(basewords-fs-gen-${NAME} util/baseword_fs_gen.c vm_opcodes.c vm_debug.c)
    target_compile_definitions(basewords-fs-gen-${NAME} PRIVATE ${ARGN})
    add_custom_command(
            OUTPUT            ${AT}/basewords.fs
            COMMAND           ${CMAKE_COMMAND} -E make_directory ${AT}/plain
            COMMAND           basewords-fs-gen-${NAME} ${AT}/basewords.fs > ${AT}/basewords.log
            DEPENDS           basewords-fs-gen-${NAME})

    set(NUCLEUS ${AT}/nuc.hex ${AT}/nuc.lst ${AT}/nuc.sizes ${AT}/nuc.inline)
    set(PLAIN ${AT}/plain/nuc.hex)
    if("${ARGN}" MATCHES "DISP_W=")
        list(APPEND NUCLEUS ${AT}/nuc.tables)           # the names, which a machine with a display has
        list(APPEND PLAIN ${AT}/plain/nuc.tables)
    endif()
    add_custom_command(
            OUTPUT            ${NUCLEUS}
            COMMAND           ${CMAKE_COMMAND} -E env BUILD=${AT}/
                                ${GFORTH} ./cross.fs ${AT}/basewords.fs ./nuc.fs > ${AT}/cross.log 2>&1
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/forth
            COMMENT           "The nucleus for the machine ${NAME} (what cross.fs says is in ${AT}/cross.log)"
            DEPENDS           ${AT}/basewords.fs
                              ${FORTH_SOURCES}
                              ${MACHINE_SOURCES})
    made(${NAME} ${NUCLEUS})
    add_custom_command(
            OUTPUT            ${PLAIN}
            COMMAND           ${CMAKE_COMMAND} -E copy ${AT}/nuc.inline ${AT}/plain/nuc.inline
            COMMAND           ${CMAKE_COMMAND} -E env BUILD=${AT}/plain/ PEEPHOLE=off
                                ${GFORTH} ./cross.fs ${AT}/basewords.fs ./nuc.fs > ${AT}/plain/cross.log 2>&1
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/forth
            COMMENT           "The nucleus for the machine ${NAME}, with the peephole off (${AT}/plain/cross.log)"
            DEPENDS           machine-${NAME}
                              ${FORTH_SOURCES}
                              ${MACHINE_SOURCES})
    add_custom_target(plain-${NAME} DEPENDS ${PLAIN})

    machine_tool(hexaforth ${NAME} main.c ${VM_SOURCES})
endfunction()

# When all that is made for a machine has been said.
function(machine_made NAME)
    get_property(FILES GLOBAL PROPERTY MADE_FOR_${NAME})
    add_custom_target(machine-${NAME} DEPENDS ${FILES})
endfunction()

# A tool or a test built for a machine.
function(machine_tool TOOL NAME)
    tool(${TOOL}-${NAME} ${ARGN})
    target_compile_definitions(${TOOL}-${NAME} PRIVATE ${DEFINITIONS_${NAME}})
endfunction()

# A tool that has the machine's step in it (util/fuse.c), built for a
# machine's cell: fuse.c says what the rest of the machine it tries things
# on is.
function(stepping_tool TOOL NAME)
    tool(${TOOL}-${NAME} ${ARGN})
    target_compile_definitions(${TOOL}-${NAME} PRIVATE ${CELL_OF_${NAME}})
    target_include_directories(${TOOL}-${NAME} PRIVATE util)
endfunction()

# A test of a machine: what it is called, what is built for it, and what
# that is run with.  It is run from where the source is.
function(machine_test TEST NAME TOOL)
    add_test(NAME ${TEST} COMMAND ${TOOL}-${NAME} ${ARGN} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    wanted(${TOOL}-${NAME})
endfunction()

# The tests that type at the prompt of a machine's reference VM: the
# nucleus; comments, and a block of a disk read as source; regions a cell
# at a time against the one form of their pixels; the dither against a
# dither a pixel at a time; the TIFF reader; and for a machine with a
# display, the drawing words against a model of them, and the desktop, its
# windows worked with keys and a pointer and its screen looked at.  The
# same sources and the same tests on every machine: what is different is
# the cell.
function(machine_tests NAME)
    set(AT ${BUILD}/${NAME})
    set(NUC ${AT}/nuc.hex)
    wanted(machine-${NAME})

    machine_tool(prompt_test ${NAME} test/prompt_test.c ${TYPED_SOURCES})
    machine_test(prompt-${NAME} ${NAME} prompt_test ${NUC})

    # The block is a file that is there for this, laid down as a disk's
    # source is.
    machine_tool(source_test ${NAME} test/source_test.c ${TYPED_SOURCES})
    add_custom_command(
            OUTPUT            ${AT}/source.img
            COMMAND           ${CMAKE_COMMAND} -E make_directory ${AT}
            COMMAND           mkdisk ${PICTURE} ${AT}/source.img ${CMAKE_SOURCE_DIR}/test/source.fs > ${AT}/source-disk.log 2>&1
            DEPENDS           mkdisk picture test/source.fs)
    machine_test(source-${NAME} ${NAME} source_test ${NUC} ${AT}/source.img)
    made(${NAME} ${AT}/source.img)

    machine_tool(region_test ${NAME} test/region_test.c ${TYPED_SOURCES})
    machine_tool(dither_test ${NAME} test/dither_test.c test/dither.c ${TYPED_SOURCES})
    machine_tool(tiff_test ${NAME} test/tiff_test.c test/dither.c ${TYPED_SOURCES})
    foreach(SEED ${SEEDS})
        machine_test(regions-${NAME}-${SEED} ${NAME} region_test ${NUC} ${SEED})
        machine_test(dither-${NAME}-${SEED} ${NAME} dither_test ${NUC} ${SEED})
        machine_test(tiff-${NAME}-${SEED} ${NAME} tiff_test ${NUC} ${SEED})
    endforeach()

    if(NOT "${DEFINITIONS_${NAME}}" MATCHES "DISP_W=")
        return()
    endif()

    machine_tool(display_test ${NAME} test/display_test.c test/tables.c ${TYPED_SOURCES})
    foreach(SEED ${SEEDS})
        machine_test(display-${NAME}-${SEED} ${NAME} display_test ${NUC} forth ${SEED})
    endforeach()

    # The desktop, with a disk made as QEMU's is: block 1 for the editor,
    # the Mandrill, and the names that go with this image.  The test keeps
    # its screens, and its copy of the disk, in a directory of its own.
    machine_tool(desktop_test ${NAME} test/desktop_test.c ${DESK_SOURCES} ${TYPED_SOURCES})
    add_custom_command(
            OUTPUT            ${AT}/disk.img
            COMMAND           ${CMAKE_COMMAND} -E make_directory ${AT}/desktop
            COMMAND           mkdisk ${PICTURE} ${AT}/disk.img ${CMAKE_SOURCE_DIR}/baremetal/block1.fs ${AT}/nuc.tables
                                > ${AT}/disk.log 2>&1
            DEPENDS           mkdisk picture baremetal/block1.fs ${AT}/nuc.tables)
    machine_test(desktop-${NAME} ${NAME} desktop_test ${NUC} ${AT}/disk.img ${PICTURE} forth ${AT}/desktop names)
    made(${NAME} ${AT}/disk.img)
endfunction()

# After any change to a compiler or a peephole: the image against the image
# built with the peephole off (test/builds_test.c, which says how it
# compares them).
function(builds_test NAME)
    set(AT ${BUILD}/${NAME})
    machine_tool(builds_test ${NAME} test/builds_test.c util/image.c)
    machine_test(builds ${NAME} builds_test ${AT}/plain/nuc.hex ${AT}/plain/nuc.tables ${AT}/nuc.hex ${AT}/nuc.tables)
    wanted(plain-${NAME})
endfunction()

# util/machines, a line a machine: the bits of a cell, the log2 of memory's
# cells, and the display, across and down and the cell it starts at.  Each
# stops at a fetch or a store outside memory (STRICT).  The first of them
# is the one the two builds are compared on, and that the reports are of.
file(STRINGS util/machines MACHINE_LINES REGEX "^[^#]")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS util/machines)
set(MACHINES)
foreach(LINE ${MACHINE_LINES})
    separate_arguments(SAID UNIX_COMMAND "${LINE}")
    list(GET SAID 0 CELL)
    list(GET SAID 1 ADDR)
    set(DEFINITIONS CELL_BITS=${CELL} ADDR_BITS=${ADDR} STRICT)
    list(LENGTH SAID HOW_MUCH)
    if(HOW_MUCH GREATER 2)
        list(GET SAID 2 ACROSS)
        list(GET SAID 3 DOWN)
        list(GET SAID 4 STARTS)
        list(APPEND DEFINITIONS DISP_W=${ACROSS} DISP_H=${DOWN} DISP_BASE=${STARTS})
    endif()
    machine(${CELL} ${DEFINITIONS})
    machine_tests(${CELL})
    machine_made(${CELL})
    list(APPEND MACHINES ${CELL})
endforeach()
list(GET MACHINES 0 FIRST)
builds_test(${FIRST})

#
# The reports.  Each is a target that builds what it wants, runs it on the
# first machine, and prints what it found; everything it makes on the way
# is kept in a directory of build/ that has the target's name.
#
set(OF ${BUILD}/${FIRST})
set(NUC ${OF}/nuc.hex)
set(TABLES ${OF}/nuc.tables)
set(SIZES ${OF}/nuc.sizes)

# The reference VM that counts how often the instruction at each place is
# run (bench/counting.c), in place of vm.c: by itself, and as the VM the
# desktop is worked on.
set(COUNTED_SOURCES test/nucleus.c bench/counting.c vm_opcodes.c vm_debug.c util/stack.c util/image.c)
machine_tool(counting ${FIRST} bench/counting.c vm_opcodes.c vm_debug.c util/stack.c)
machine_tool(desktop_test_counted ${FIRST} test/desktop_test.c ${DESK_SOURCES} ${COUNTED_SOURCES})
target_compile_definitions(desktop_test_counted-${FIRST} PRIVATE COUNTING_A_TEST)
machine_tool(profile ${FIRST} bench/profile.c ${DESK_SOURCES} ${COUNTED_SOURCES})
target_compile_definitions(profile-${FIRST} PRIVATE COUNTING_A_TEST)

# `hits`: how often each rule of the nucleus's peephole is used while the
# desktop is worked.  join.fs keeps a count for each of its rules; the
# desktop's test is told to write the machine's memory when it is done, and
# the counts are read out of that (test/hits.c).  What the test itself
# types at the prompt is compiled by the nucleus too, and is counted with
# the rest.
set(HITS ${BUILD}/hits)
machine_tool(hits ${FIRST} test/hits.c vm_opcodes.c vm_debug.c)
add_custom_target(hits
        COMMAND           ${CMAKE_COMMAND} -E make_directory ${HITS}/desktop
        COMMAND           ${CMAKE_COMMAND} -E rm -f ${HITS}/memory.out
        COMMAND           ${CMAKE_COMMAND} -E env MEMORY=${HITS}/memory.out
                            $<TARGET_FILE:desktop_test-${FIRST}> ${NUC} ${OF}/disk.img ${PICTURE} forth ${HITS}/desktop names
                            > ${HITS}/desktop.log 2>&1
        COMMAND           hits-${FIRST} ${HITS}/memory.out forth/joins.fs
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT           "The desktop worked, and the rules it used (what its test says is in ${HITS}/desktop.log)")
add_dependencies(hits machine-${FIRST})

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
stepping_tool(costs ${FIRST} bench/costs.c util/image.c)
stepping_tool(survey ${FIRST} util/survey.c util/image.c)
stepping_tool(shorter ${FIRST} bench/shorter.c util/image.c)
add_custom_target(profile
        COMMAND           ${CMAKE_COMMAND} -E make_directory ${PROFILE}
        COMMAND           profile-${FIRST} ${NUC} ${TABLES} ${SIZES} ${OF}/disk.img ${PICTURE} forth ${PROFILE}
                            > ${PROFILE}/profile.txt 2> ${PROFILE}/profile.log
        COMMAND           ${CMAKE_COMMAND} -E cat ${PROFILE}/profile.txt
        COMMAND           costs-${FIRST} ${NUC} ${TABLES} ${SIZES} ${PROFILE}/desktop.counts ${PROFILE} ${PROFILE_FILES}
                            2> ${PROFILE}/costs.log
        COMMAND           survey-${FIRST} ${NUC} ${TABLES} ${PROFILE}/desktop.counts ${PROFILE}/survey.txt
                            2> ${PROFILE}/survey.log
        COMMAND           shorter-${FIRST} ${NUC} ${TABLES} ${PROFILE}/desktop.counts ${PROFILE}/shorter.txt
                            2> ${PROFILE}/shorter.log
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT           "The desktop's work, by place (${PROFILE}/profile.log says what went wrong, if anything did)")
add_dependencies(profile machine-${FIRST})

# `peephole`: what the nucleus's compiler is worth (forth/join.fs).  The
# nucleus compiles what is typed and what is read from blocks, and cross.fs
# compiles the image; the system's own source is the code there is to try
# the nucleus's compiler on.
#
#   the image built with the peephole and the image built without are cut
#     into the same straight-line runs (bench/runs.c): a run as it is
#     typed, and what cross.fs made of it
#   each run as typed is given to the nucleus's emit1 at the prompt of the
#     reference VM, an instruction at a time, and the nucleus says what it
#     made of it and which of its rules it used (bench/typed.c)
#   the work is done on the VM that counts: the machine coming up and
#     being stopped (bench/idle.fs), which is taken from the rest; the
#     desktop's test; and the drawing benchmark (bench/draw.fs)
#   and the runs are weighed by that (bench/weigh.c), which says at its top
#     what it prints
set(PEEPHOLE ${BUILD}/peephole)
machine_tool(runs ${FIRST} bench/runs.c vm_opcodes.c vm_debug.c util/stack.c util/image.c)
machine_tool(typed ${FIRST} bench/typed.c vm_opcodes.c vm_debug.c util/image.c)
tool(weigh bench/weigh.c)
add_custom_target(peephole
        COMMAND           ${CMAKE_COMMAND} -E make_directory ${PEEPHOLE}/desktop
        COMMAND           runs-${FIRST} ${OF}/plain/nuc.hex ${OF}/plain/nuc.tables ${NUC} ${TABLES}
                            > ${PEEPHOLE}/runs.txt 2> ${PEEPHOLE}/runs.log
        COMMAND           typed-${FIRST} type ${PEEPHOLE}/runs.txt ${NUC} ${TABLES}
                            > ${PEEPHOLE}/typed.fs 2> ${PEEPHOLE}/typed.log
        COMMAND           hexaforth-${FIRST} ${NUC} < ${PEEPHOLE}/typed.fs > ${PEEPHOLE}/typed.out 2>&1
        COMMAND           typed-${FIRST} read ${PEEPHOLE}/runs.txt ${PEEPHOLE}/typed.out
                            > ${PEEPHOLE}/typed.txt 2> ${PEEPHOLE}/typed.log
        COMMAND           ${CMAKE_COMMAND} -E copy ${OF}/disk.img ${PEEPHOLE}/disk.img
        COMMAND           ${CMAKE_COMMAND} -E rm -f ${PEEPHOLE}/idle.counts ${PEEPHOLE}/draw.counts ${PEEPHOLE}/desktop.counts
        COMMAND           ${CMAKE_COMMAND} -E env RUN_COUNTS=${PEEPHOLE}/idle.counts
                            $<TARGET_FILE:counting-${FIRST}> ${NUC} ${PEEPHOLE}/disk.img
                            < bench/idle.fs > ${PEEPHOLE}/idle.log 2>&1
        COMMAND           ${CMAKE_COMMAND} -E env RUN_COUNTS=${PEEPHOLE}/draw.counts
                            $<TARGET_FILE:counting-${FIRST}> ${NUC} ${PEEPHOLE}/disk.img
                            < bench/draw.fs > ${PEEPHOLE}/draw.log 2>&1
        COMMAND           ${CMAKE_COMMAND} -E env RUN_COUNTS=${PEEPHOLE}/desktop.counts
                            $<TARGET_FILE:desktop_test_counted-${FIRST}> ${NUC} ${PEEPHOLE}/disk.img ${PICTURE} forth
                            ${PEEPHOLE}/desktop names > ${PEEPHOLE}/desktop.log 2>&1
        COMMAND           weigh ${PEEPHOLE}/typed.txt forth/joins.fs ${SIZES} ${PEEPHOLE} desktop draw
                            > ${PEEPHOLE}/peephole.txt 2> ${PEEPHOLE}/weigh.log
        COMMAND           ${CMAKE_COMMAND} -E cat ${PEEPHOLE}/peephole.txt
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT           "The nucleus's compiler, weighed (each step's log is in ${PEEPHOLE})")
add_dependencies(peephole machine-${FIRST} plain-${FIRST})

# `phrases`: the phrase book, what to write to get each instruction
# (util/phrases.c, which says at its top what it finds and asks).  It is
# run where the Forth is, since it has cross.fs compile every phrase; the
# report is build/phrases.txt, and its first part is printed.  LONGEST=4 in
# the environment is phrases of up to four words, which takes much longer.
tool(phrase-book util/phrases.c util/image.c)
target_include_directories(phrase-book PRIVATE util)
add_custom_target(phrases
        COMMAND           ${MAKE_BUILD_DIR}
        COMMAND           ${WITH_HEADERS} $<TARGET_FILE:phrase-book> ${BUILD}/phrases.txt
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/forth)

# `tables`: the files that are made from the machine and from an image, and
# checked in, made again.  forth/folds.fs is what the nucleus's peephole is
# told of numbers, asked of the machine (util/joins.c).  forth/fused.fs is
# the words the decompiler shows an instruction as, made from a built image
# (util/fused.c); the table is in the image, so util/tables.cmake builds
# the image and makes the table of it until the table stops changing.  The
# image is the default machine's (build/nuc.hex).
tool(joins util/joins.c)
tool(fused util/fused.c util/image.c)
target_include_directories(fused PRIVATE util)
add_custom_target(tables
        COMMAND           joins forth/folds.fs
        COMMAND           ${WITH_HEADERS} ${CMAKE_COMMAND} -DFUSED=$<TARGET_FILE:fused> -P util/tables.cmake
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

#
# Every test there is, built and then run: a line for each, and all that
# each said kept (Testing/Temporary/LastTest.log, where CMake builds).
# `ctest -V -R NAME` there runs the ones whose names have that in them,
# and prints what they say.
#
get_property(WANTED GLOBAL PROPERTY WANTED)
add_custom_target(check
        COMMAND           ${CMAKE_CTEST_COMMAND} --output-on-failure
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        USES_TERMINAL)
add_dependencies(check ${WANTED})
