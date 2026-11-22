cmake_minimum_required(VERSION 3.18)

#
# What the project needs that is not in it.
#
# Each is said once, here: a program by the names it goes by, or a library
# by a header of its; what it is for; and the package that has it, where
# Homebrew installs things (macOS, and Linux if it is there) and where apt
# does (Debian and Ubuntu).  When CMake is run, each is looked for, and
# what is not there is asked of whichever of the two the system has, all
# of it at once: apt as root, through sudo if CMake is not root itself.
# So on a system that has CMake and one of the two, and nothing else,
#
#     cmake -S . -B build/cmake
#
# leaves everything here able to be built.  -DINSTALL_NEEDED=OFF only
# looks, and says what is not there.  This is before the project is named,
# since naming it wants the C compiler and make, which are two of them.
#
# What is still not there afterwards is said, with what it is for.  A
# system with neither Homebrew nor apt is told the same, and has to find
# each for itself.  macOS has a C compiler, make, git and curl with Xcode's
# command line tools, which Homebrew does not install without.
#
option(INSTALL_NEEDED "ask Homebrew or apt for what the project needs and is not here" ON)
find_program(BREW brew)
find_program(APT apt-get)
set(BREW_PREFIX)
if(BREW)
    execute_process(COMMAND ${BREW} --prefix
                    OUTPUT_VARIABLE BREW_PREFIX OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()

# A need: the variable that has where it is, and then
#
#   PROGRAM names...    a program, by any of those names
#   HEADER file         or a library, by a header of its
#   FOR text            what it is for
#   BREW formula        the package that has it, of Homebrew's
#   APT package         and of apt's
#   BUILT_HERE          where no package has it, it is built from its source,
#                       which the part of this file that wants it fetches
#   WITHOUT name        it is needed only where that other need is not here
#                       and no package has it: it is what the other is built
#                       from
function(needs NAME)
    cmake_parse_arguments(IT "BUILT_HERE" "HEADER;FOR;BREW;APT;WITHOUT" "PROGRAM" ${ARGN})
    set_property(GLOBAL APPEND PROPERTY NEEDED ${NAME})
    foreach(PART PROGRAM HEADER FOR BREW APT BUILT_HERE WITHOUT)
        set_property(GLOBAL PROPERTY NEEDED_${NAME}_${PART} "${IT_${PART}}")
    endforeach()
endfunction()

needs(C_COMPILER    PROGRAM cc gcc clang        APT gcc
      FOR "everything: the C is compiled with it, and gforth builds util/fuse.h with it")
needs(CXX_COMPILER  PROGRAM c++ g++ clang++     APT g++
      FOR "the Verilog: Verilator makes C++ of it")
needs(MAKE_PROGRAM  PROGRAM make gmake          APT make                        BREW make
      FOR "everything: CMake builds with it, and so does Verilator")
needs(GIT           PROGRAM git                 APT git                         BREW git
      FOR "bench-m68k and gates, which fetch the 68000 and the two other CPUs")
needs(CURL          PROGRAM curl                APT curl                        BREW curl
      FOR "the Mandrill, which the tests that have a disk are given")
needs(GFORTH        PROGRAM gforth              APT gforth                      BREW gforth
      FOR "every image: cross.fs is run by it")
needs(LIBTOOL       PROGRAM glibtool libtool    APT libtool-bin                 BREW libtool
      FOR "every image: gforth builds util/fuse.h through it")
needs(LTDL_HEADER   HEADER ltdl.h               APT libltdl-dev                 BREW libtool
      FOR "every image: what gforth builds of util/fuse.h is built with it")
needs(UNICORN_HEADER HEADER unicorn/unicorn.h   APT libunicorn-dev              BREW unicorn
      FOR "the tests kernel-hexaforth and kernel-hexaforth-fast: the kernels are run against vm.c on it")
needs(CLANG         PROGRAM clang               APT clang                       BREW llvm
      FOR "the AArch64 kernels, which it assembles")
needs(LLD           PROGRAM ld.lld              APT lld                         BREW lld
      FOR "the AArch64 kernels, which it links")
needs(OBJCOPY       PROGRAM llvm-objcopy        APT llvm                        BREW llvm
      FOR "the AArch64 kernels, as what QEMU's machine has in memory")
needs(NM            PROGRAM llvm-nm             APT llvm                        BREW llvm
      FOR "the AArch64 kernels: where a kernel's symbols are")
needs(QEMU_AARCH64  PROGRAM qemu-system-aarch64 APT qemu-system-arm             BREW qemu
      FOR "the desktop on a kernel: the qemu- tests, run, demo, demo-test and demo-time")
needs(VERILATOR     PROGRAM verilator           APT verilator                   BREW verilator
      FOR "the Verilog: the verilog- tests and the benchmarks")
needs(YOSYS         PROGRAM yosys               APT yosys                       BREW yosys
      FOR "gates and synth")
needs(NEXTPNR       PROGRAM nextpnr-ecp5        APT nextpnr-ecp5                BUILT_HERE
      FOR "synth")
needs(TRELLIS       PROGRAM ecppack             BREW prjtrellis                 WITHOUT NEXTPNR
      FOR "synth, where nextpnr-ecp5 is built here: the ECP5's database, which its own is made from")
needs(BOOST_HEADER  HEADER boost/version.hpp    BREW boost                      WITHOUT NEXTPNR
      FOR "synth, where nextpnr-ecp5 is built here: it is built with it")
needs(EIGEN_HEADER  HEADER eigen3/Eigen/Core    BREW eigen                      WITHOUT NEXTPNR
      FOR "synth, where nextpnr-ecp5 is built here: it is built with it")
needs(CLANGD        PROGRAM clangd              APT clangd                      BREW llvm
      FOR "the target clangd: what the language server says of the C")
needs(M68K_AS       PROGRAM m68k-elf-as m68k-linux-gnu-as           APT binutils-m68k-linux-gnu BREW m68k-elf-binutils
      FOR "bench-m68k: the 68000's assembler")
needs(M68K_LD       PROGRAM m68k-elf-ld m68k-linux-gnu-ld           APT binutils-m68k-linux-gnu BREW m68k-elf-binutils
      FOR "bench-m68k: the 68000's linker")
needs(M68K_OBJCOPY  PROGRAM m68k-elf-objcopy m68k-linux-gnu-objcopy APT binutils-m68k-linux-gnu BREW m68k-elf-binutils
      FOR "bench-m68k: the 68000's program as its bytes")

# Each is looked for where the system looks, and then where Homebrew has
# what it does not put there (llvm is one), so that a program the system
# has is the one that is used.
function(look_for NAME)
    get_property(PROGRAM GLOBAL PROPERTY NEEDED_${NAME}_PROGRAM)
    get_property(HEADER GLOBAL PROPERTY NEEDED_${NAME}_HEADER)
    get_property(FORMULA GLOBAL PROPERTY NEEDED_${NAME}_BREW)
    set(PLACES)
    if(BREW_PREFIX)
        set(PLACES ${BREW_PREFIX})
        if(FORMULA)
            list(APPEND PLACES ${BREW_PREFIX}/opt/${FORMULA})
        endif()
    endif()
    if(PROGRAM)
        list(TRANSFORM PLACES APPEND /bin)
        find_program(${NAME} NAMES ${PROGRAM} PATHS ${PLACES})
    else()                          # CMake does not know where a system's headers are until the project is named
        list(PREPEND PLACES /usr /usr/local)
        list(TRANSFORM PLACES APPEND /include)
        find_path(${NAME} ${HEADER} PATHS ${PLACES})
    endif()
endfunction()

# The package that has a need, of whichever of the two is here: none, if
# neither is, or the one that is has no package of it.
function(package_of RESULT NAME)
    set(PACKAGE)
    if(BREW)
        get_property(PACKAGE GLOBAL PROPERTY NEEDED_${NAME}_BREW)
    elseif(APT)
        get_property(PACKAGE GLOBAL PROPERTY NEEDED_${NAME}_APT)
    endif()
    set(${RESULT} "${PACKAGE}" PARENT_SCOPE)
endfunction()

# Is a need wanted here?  One that is needed only WITHOUT another is not,
# where that other is here or a package has it.
function(needed_here RESULT NAME)
    set(${RESULT} TRUE PARENT_SCOPE)
    get_property(OTHER GLOBAL PROPERTY NEEDED_${NAME}_WITHOUT)
    if(NOT OTHER)
        return()
    endif()
    package_of(PACKAGED ${OTHER})
    if(${OTHER} OR PACKAGED)
        set(${RESULT} FALSE PARENT_SCOPE)
    endif()
endfunction()

get_property(NEEDED GLOBAL PROPERTY NEEDED)
set(ASKED)
foreach(NAME ${NEEDED})
    look_for(${NAME})
    needed_here(HERE ${NAME})
    if(HERE AND NOT ${NAME})
        package_of(PACKAGE ${NAME})
        list(APPEND ASKED ${PACKAGE})
    endif()
endforeach()
list(REMOVE_DUPLICATES ASKED)
if(ASKED AND INSTALL_NEEDED)
    string(REPLACE ";" " " SAID "${ASKED}")
    if(BREW)
        message(STATUS "Not here, and asked of Homebrew: ${SAID}")
        execute_process(COMMAND ${BREW} install ${ASKED})
    else()
        message(STATUS "Not here, and asked of apt: ${SAID}")
        set(AS_ROOT)
        execute_process(COMMAND id -u OUTPUT_VARIABLE WHO OUTPUT_STRIP_TRAILING_WHITESPACE)
        if(NOT WHO STREQUAL "0")
            find_program(SUDO sudo)
            set(AS_ROOT ${SUDO})
        endif()
        set(INSTALLED ${AS_ROOT} ${CMAKE_COMMAND} -E env DEBIAN_FRONTEND=noninteractive ${APT} install -y ${ASKED})
        execute_process(COMMAND ${INSTALLED} RESULT_VARIABLE FAILED)
        if(FAILED)                  # as it does where apt has not yet been told what there is
            execute_process(COMMAND ${AS_ROOT} ${APT} update)
            execute_process(COMMAND ${INSTALLED})
        endif()
    endif()
    foreach(NAME ${NEEDED})
        look_for(${NAME})
    endforeach()
endif()
foreach(NAME ${NEEDED})
    needed_here(HERE ${NAME})
    if(HERE AND NOT ${NAME})
        get_property(PROGRAM GLOBAL PROPERTY NEEDED_${NAME}_PROGRAM)
        get_property(HEADER GLOBAL PROPERTY NEEDED_${NAME}_HEADER)
        get_property(FOR GLOBAL PROPERTY NEEDED_${NAME}_FOR)
        get_property(FORMULA GLOBAL PROPERTY NEEDED_${NAME}_BREW)
        get_property(PACKAGE GLOBAL PROPERTY NEEDED_${NAME}_APT)
        get_property(BUILT GLOBAL PROPERTY NEEDED_${NAME}_BUILT_HERE)
        if(HEADER)
            set(WHAT "The library that has ${HEADER}")
        else()
            list(GET PROGRAM 0 WHAT)
        endif()
        set(HAS "apt has no package of it")
        if(PACKAGE)
            set(HAS "apt's package of it is ${PACKAGE}")
        endif()
        if(FORMULA)
            string(APPEND HAS ", and Homebrew's is ${FORMULA}")
        else()
            string(APPEND HAS ", and Homebrew has none")
        endif()
        if(BREW AND NOT FORMULA)
            if(BUILT)                   # which is not a thing to warn of
                message(STATUS "${WHAT} is not here, and Homebrew has no package of it: it is built from its source when ${FOR} is first asked for")
                continue()
            endif()
            set(HOW "Homebrew has no package of it")
        elseif(NOT BREW AND NOT APT)
            set(HOW "Neither Homebrew nor apt is here to ask: ${HAS}")
        elseif(NOT INSTALL_NEEDED)
            set(HOW "It was not asked for, since INSTALL_NEEDED is off: ${HAS}")
        else()
            set(HOW "It was asked for and did not come")
        endif()
        message(WARNING "${WHAT} is not here.  It is for ${FOR}.  ${HOW}.")
    endif()
endforeach()

project(hexaforth C)

# How each C file is compiled is written out when CMake is run
# (compile_commands.json, where CMake builds), for a language server:
# clangd reads it, and so knows a file's definitions and headers.  clangd
# looks for it in build/ under each directory above a source file, so a
# link to it is put there, wherever CMake was told to build.  Only what is
# compiled as a target here is in it: not what gforth builds of
# util/fuse.h, the kernels or the Verilog.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BINARY_DIR STREQUAL "${CMAKE_SOURCE_DIR}/build")
    file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/build)
    file(CREATE_LINK ${CMAKE_BINARY_DIR}/compile_commands.json
                     ${CMAKE_SOURCE_DIR}/build/compile_commands.json SYMBOLIC)
endif()

set(CMAKE_C_STANDARD 99)

# Built as a release unless it is said otherwise: built with no flags, the
# tests do not link.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# What the compiler made on the way, the assembly among it, is kept beside
# each object: a source is compiled for each machine, often at once, and
# kept where it was compiled from they would be written over each other.
set(CMAKE_C_FLAGS_RELEASE "-save-temps=obj -O2 -fverbose-asm -fcommon")
set(CMAKE_C_FLAGS_DEBUG "-save-temps=obj -g -fverbose-asm -fcommon")

# What is compiled here has the trace of every instruction in it (DEBUG),
# but for a tool or a test, which is built without (tool).
add_compile_definitions(DEBUG)

# A tool or a test: built without the trace.  Every one is built by a build
# of no target, so that after one no tool is older than its source: a tool
# that is run by hand is then the tool the source says.  One that cannot be
# built everywhere, or not without fetching what it is made of, is said to
# be left out of that build where it is defined (left_out).
function(tool NAME)
    add_executable(${NAME} ${ARGN})
    target_compile_options(${NAME} PRIVATE -UDEBUG -w)
    target_link_libraries(${NAME} m)
endfunction()
function(left_out)
    set_target_properties(${ARGN} PROPERTIES EXCLUDE_FROM_ALL TRUE)
endfunction()

# cross.fs writes its output to build/, which is not checked in
set(MAKE_BUILD_DIR ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/build)

# An image is made from every Forth file: nuc.fs includes system.fs and
# through it the rest.  So an image is out of date when any of them
# changes, and a file that is added is found the next time anything is
# built.  test_cases.fs is made here, and only its own image needs it:
# that image is of cross.fs, the machine and the tests, and no other file.
file(GLOB FORTH_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/forth/*.fs)
list(FILTER FORTH_SOURCES EXCLUDE REGEX "/test_cases\\.fs$")

# cross.fs asks the machine which instructions do the work of which
# (util/fuse.h, which has vm.c in it), through gforth's C interface.  So an
# image is out of date when the machine changes too.  gforth builds that C
# with libtool; and where gforth is Homebrew's, the compiler has to be told
# where Homebrew's headers are, gforth's own among them.
set(MACHINE_SOURCES util/fuse.h util/named.h util/room.h vm.c vm_loop.h vm_step.h vm.h vm_opcodes.c
                    vm_opcodes.h vm_instruction.h vm_constants.h util/stack.c)
set(WITH_HEADERS)                   # what is put before a command that has gforth build C
if(BREW)
    set(BREW_CPATH ${BREW_PREFIX}/include)
    if(DEFINED ENV{CPATH})
        set(BREW_CPATH ${BREW_CPATH}:$ENV{CPATH})
    endif()
    set(WITH_HEADERS ${CMAKE_COMMAND} -E env CPATH=${BREW_CPATH})
endif()
set(RUN_GFORTH ${WITH_HEADERS} ${GFORTH})

# An image: cross.fs, run by gforth on a program, with what it writes in a
# directory and what it says in a log there.  Every image is made by this,
# and it is the one place that says how.
#
#   image(DIRECTORY
#         [SOURCE file]         the program, a file of forth/: nuc.fs if none
#                               is said
#         [WORDS file]          the machine cross.fs reads: forth/basewords.fs
#                               if none is said
#         [SET name=value...]   what cross.fs is told besides, in its
#                               environment (BOOT=on, ROM=virt-disk.fs)
#         [MAKES kind...]       the files it is made for, of those cross.fs
#                               writes, by what each ends in: hex if none is
#                               said
#         [FIRST command]       a command that is run before cross.fs
#         [FROM file...]        the source it is out of date after: every
#                               file of forth/ if none is said
#         [AFTER what...]       what else it is made after, files and targets
#         [LOG name]            its log: cross.log if none is said
#         [IS text])            what is said as it is made
#
# An image is out of date when the machine changes too (MACHINE_SOURCES).
# The files it is made for are left in IMAGE_FILES.
function(image DIRECTORY)
    cmake_parse_arguments(IT "" "SOURCE;WORDS;LOG;IS" "SET;MAKES;FIRST;FROM;AFTER" ${ARGN})
    if(NOT IT_SOURCE)
        set(IT_SOURCE nuc.fs)
    endif()
    if(NOT IT_WORDS)
        set(IT_WORDS ./basewords.fs)
    endif()
    if(NOT IT_MAKES)
        set(IT_MAKES hex)
    endif()
    if(NOT IT_FROM)
        set(IT_FROM ${FORTH_SOURCES})
    endif()
    if(NOT IT_LOG)
        set(IT_LOG cross.log)
    endif()
    get_filename_component(STEM ${IT_SOURCE} NAME_WE)
    set(FILES)
    foreach(KIND ${IT_MAKES})
        list(APPEND FILES ${DIRECTORY}/${STEM}.${KIND})
    endforeach()
    set(BEFORE)
    if(IT_FIRST)
        set(BEFORE COMMAND ${IT_FIRST})
    endif()
    set(WORDS_FILE ${IT_WORDS})
    if(IT_WORDS MATCHES "^\\./")                        # a file of forth/, said from there
        string(REGEX REPLACE "^\\./" "${CMAKE_SOURCE_DIR}/forth/" WORDS_FILE ${IT_WORDS})
    endif()
    add_custom_command(
            OUTPUT            ${FILES}
            COMMAND           ${CMAKE_COMMAND} -E make_directory ${DIRECTORY}
            ${BEFORE}
            COMMAND           ${CMAKE_COMMAND} -E env BUILD=${DIRECTORY}/ ${IT_SET}
                                ${RUN_GFORTH} ./cross.fs ${IT_WORDS} ./${IT_SOURCE} > ${DIRECTORY}/${IT_LOG} 2>&1
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/forth
            COMMENT           "${IT_IS} (what cross.fs says is in ${DIRECTORY}/${IT_LOG})"
            DEPENDS           ${IT_FROM} ${WORDS_FILE} ${MACHINE_SOURCES} ${IT_AFTER})
    set(IMAGE_FILES ${FILES} PARENT_SCOPE)
endfunction()

# Two files of Forth are made, and checked in: each is made by a command of
# its own, from what that command says, and is among the sources of every
# image (FORTH_SOURCES).
#
# forth/folds.fs is what the nucleus's peephole is told of numbers, asked
# of the machine (util/joins.c).  It reads no image.
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
set(FUSED_FS ${CMAKE_SOURCE_DIR}/forth/fused.fs)
set(UNTABLED ${CMAKE_SOURCE_DIR}/build/untabled)
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
# after this one (forth-env, in machine), so that the two files above are
# made in one place.
image(${CMAKE_SOURCE_DIR}/build MAKES hex tables IS "The nucleus of the default machine")

# basewords.fs opcode description for forth environment
add_custom_command(
        COMMAND           ${CMAKE_BINARY_DIR}/basewords-fs-gen
                            ${CMAKE_SOURCE_DIR}/forth/basewords.fs
        OUTPUT            ${CMAKE_SOURCE_DIR}/forth/basewords.fs
        DEPENDS           basewords-fs-gen)

add_executable(basewords-fs-gen
        util/baseword_fs_gen.c
        util/image.c
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

# Compile test_cases.fs to hex.  It is in build/ with the default machine's
# nucleus, so its log has a name of its own.
image(${CMAKE_SOURCE_DIR}/build
      SOURCE test_cases.fs
      FROM ${CMAKE_SOURCE_DIR}/forth/test_cases.fs ${CMAKE_SOURCE_DIR}/forth/cross.fs
      LOG test_cases.log
      IS "The tests, as cross.fs compiles them")

# kick off our entire forth environment
add_custom_target(forth-env
        ALL
        DEPENDS           ${CMAKE_SOURCE_DIR}/build/nuc.hex)

# The VM itself, with the trace
add_library(vm_core_debug
        vm.c
        vm_loop.h
        vm_step.h
        vm.h
        vm_instruction.h
        vm_opcodes.c
        vm_opcodes.h
        vm_constants.h
        vm_debug.c
        vm_debug.h
        util/stack.c
        util/stack.h
        util/image.c)
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

# What an image is made of: its pages of code, the source files its code is
# from, and the rest of its memory (util/layout.h).  The target `examine`
# says it of the nucleus's image.
tool(examine-image util/examine.c vm_opcodes.c vm_debug.c util/image.c)
add_custom_target(examine
        COMMAND           ${CMAKE_BINARY_DIR}/examine-image build/nuc.hex build/nuc.tables build/nuc.sizes
                            > build/examine.txt
        COMMAND           ${CMAKE_COMMAND} -E cat build/examine.txt
        COMMAND           keep ${GIT} build/kept examine build/nuc.hex build/examine.txt
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        DEPENDS           examine-image
                          build/nuc.hex)

# The reference VM with the trace of every instruction (DEBUG, above).  The
# reference VM itself is `hexaforth` (machine_tool, below): one program,
# which is of the machine whose image it is given.
add_executable(hexaforth-debug
        main.c
        build/nuc.hex)
target_link_libraries(hexaforth-debug
        vm_core_debug)
target_compile_definitions(hexaforth-debug
        PUBLIC
        DEBUG)

#
# The machines the system is tried on, and everything that is built for one
# and run on it.
#
# util/machines is the machines.  For each, everything made is in a
# directory of build/ that has its name, which is the bits of its cell:
# the machine's definition for cross.fs (basewords.fs), its nucleus
# (nuc.hex, and what cross.fs writes beside it), the disks its tests are
# given, and what the tests leave.  Nothing is compiled for a machine: a
# tool or a test is one program, where CMake builds, and is of the machine
# whose image it is given (machine_tool, stepping_tool).
#
# Nothing here is built unless it is asked for: a test builds what it
# needs before it is run (needs, below), the target `check` runs every
# test, and each report is a target of its own.
#
set(BUILD ${CMAKE_SOURCE_DIR}/build)
enable_testing()

# What a target measures is kept (util/keep.c): a target that measures
# writes what it found where it always does, and so over what it found the
# time before, which is then not there to be compared with.  Each such
# target ends by keeping what it wrote in build/kept/, under the image it
# was measured of as that image is, a number of its bytes, and the target's
# name: a change that makes another image is kept beside the one before
# and not over it.  build/kept/log.txt has a line for each time, in order,
# with the commit the tree was at, and is what is read to find the one
# before.  KEEP is put before a label, the image and the files.
set(KEPT ${BUILD}/kept)
tool(keep util/keep.c)
set(KEEP keep ${GIT} ${KEPT})

# How long a test may run before it is stopped and fails, so that a hung
# test (a machine running wild, a desktop that never comes up) does not
# hang check.  Tests on the host take seconds and QEMU's take minutes; each
# limit is at least ten times that.  A test that hits its limit has hung,
# or has got much slower, which is worth looking into either way.  What a
# test needs built may take as long as building does.
set(TEST_SECONDS 60)
set(QEMU_TEST_SECONDS 600)
set(BUILD_SECONDS 3600)

# What a test needs built: the targets that make what it runs and what it
# reads.  Every test says them, here and nowhere else, and is run only when
# they have been built as the source now has them.  Each target is built by
# a test of its own, built-TARGET, which the tests that need it wait for
# (CTest's fixtures): so however a test is run, by the target check or by
# `ctest -R` with its name, what it needs is built first, and if that does
# not build the test is not run, and is said to have failed.  It is never
# run on what was built before the source changed.  One target is built at
# a time.
function(needs TEST)
    foreach(WHAT ${ARGN})
        get_property(BUILT GLOBAL PROPERTY BUILT_FOR_TESTS)
        if(NOT WHAT IN_LIST BUILT)
            set_property(GLOBAL APPEND PROPERTY BUILT_FOR_TESTS ${WHAT})
            add_test(NAME built-${WHAT}
                     COMMAND ${CMAKE_COMMAND} -E env --unset=MAKEFLAGS --unset=MFLAGS --unset=MAKELEVEL
                             ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target ${WHAT})
            set_tests_properties(built-${WHAT} PROPERTIES
                                 FIXTURES_SETUP built-${WHAT} RESOURCE_LOCK the-build TIMEOUT ${BUILD_SECONDS})
        endif()
        set_property(TEST ${TEST} APPEND PROPERTY FIXTURES_REQUIRED built-${WHAT})
    endforeach()
endfunction()

# Which target makes a file: said where the file is made, and asked by a
# report of every file it names (report).
function(owned TARGET)
    foreach(FILE ${ARGN})
        string(MD5 KEY "${FILE}")
        set_property(GLOBAL PROPERTY OWNER_${KEY} ${TARGET})
    endforeach()
endfunction()

# The files cross.fs writes beside an image in a directory, which are all
# the one target's that makes the image.
function(image_owned TARGET DIRECTORY)
    foreach(KIND hex tables lst sizes inline public needs shadows)
        owned(${TARGET} ${DIRECTORY}/nuc.${KIND})
    endforeach()
endfunction()

# A report: a target that runs what it is told and says what was found,
# said as add_custom_target is.  What it depends on is not said: it is the
# target that makes each file its commands name, a machine's image or its
# disks or the pictures (owned), so that each is made, and made in one
# place, before the report reads it.  Disks said with commas between them
# are each looked at.  A tool it runs is built first, as CMake has it.
function(report NAME)
    add_custom_target(${NAME} ${ARGN})
    set(OWNERS)
    foreach(SAID ${ARGN})
        string(REPLACE "," ";" FILES "${SAID}")
        foreach(FILE ${FILES})
            string(MD5 KEY "${FILE}")
            get_property(OWNER GLOBAL PROPERTY OWNER_${KEY})
            if(OWNER)
                list(APPEND OWNERS ${OWNER})
            endif()
        endforeach()
    endforeach()
    list(REMOVE_DUPLICATES OWNERS)
    if(NOT OWNERS)
        message(FATAL_ERROR "the report ${NAME} names nothing that is made: it would be run on whatever was there")
    endif()
    add_dependencies(${NAME} ${OWNERS})
endfunction()

# The tests that need only the reference VM, with no QEMU and no Verilog,
# have a label, and the target `quick` runs those (at the end of this
# file).  A test is given it here.
set(ON_THE_VM vm)
function(on_the_vm)
    set_property(TEST ${ARGN} APPEND PROPERTY LABELS ${ON_THE_VM})
endfunction()

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

# What makes a disk, and the Mandrill, as the USC-SIPI image database has
# it: a TIFF that is not compressed, which is what the system reads.
tool(mkdisk util/mkdisk.c util/image.c)

# The applications: the source that is to be kept on the disk and compiled
# when it is wanted, so is written to the width of a block's line.  The
# target `wrap` fits them to it (util/wrap.c), writing the files again; what
# it changed is read in git's diff before it is kept.
# The source that goes on a disk as blocks (doc/boot.md), which `wrap` fits
# to them: every file of forth/ but cross.fs's own (cross.fs, and the
# machine it reads, basewords.fs), the kernel's (nuc.fs), the lists of
# what a system includes (system.fs, picture.fs), and what is made
# (fused.fs, folds.fs), whose makers lay it out themselves.
set(DISK_SOURCES ${FORTH_SOURCES})
list(FILTER DISK_SOURCES EXCLUDE REGEX "/(cross|basewords|nuc|system|picture|fused|folds)\\.fs$")
tool(wrap-source util/wrap.c)
add_custom_target(wrap
        COMMAND           wrap-source ${DISK_SOURCES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/forth
        DEPENDS           wrap-source
        VERBATIM)
# A picture of the USC-SIPI image database is fetched as the database has
# it, by its number there.
function(fetched FILE NUMBER)
    add_custom_command(
            OUTPUT            ${FILE}
            COMMAND           ${MAKE_BUILD_DIR}
            COMMAND           ${CURL} -sSfL --remove-on-error -o ${FILE}
                                "https://sipi.usc.edu/database/download.php?vol=misc&img=${NUMBER}"
            VERBATIM)
endfunction()
set(PICTURE ${BUILD}/mandrill.tiff)
fetched(${PICTURE} 4.2.03)
add_custom_target(picture DEPENDS ${PICTURE})
owned(picture ${PICTURE})

# A disk is no more than a floppy holds (DISK_FLOPPY, vm_constants.h), and
# the pictures are on one of their own: an index and the Mandrill under
# its label, which the viewer finds it by on whichever disk has it
# (mkdisk --disk; doc/disk.md, Several disks).  It is a machine's second
# disk, wherever a machine is given the picture.  pictures_disk makes such
# a disk of any picture, under the Mandrill's label, which is the one the
# viewer asks for.
function(pictures_disk DISK FROM)
    get_filename_component(AT ${DISK} DIRECTORY)
    get_filename_component(LOG ${DISK} NAME_WE)
    add_custom_command(
            OUTPUT            ${DISK}
            COMMAND           ${CMAKE_COMMAND} -E make_directory ${AT}
            COMMAND           mkdisk --disk ${DISK} mandrill.tiff=${FROM} > ${AT}/${LOG}.log 2>&1
            DEPENDS           mkdisk picture ${FROM})
endfunction()
set(PICTURES ${BUILD}/pictures.img)
pictures_disk(${PICTURES} ${PICTURE})
add_custom_target(pictures DEPENDS ${PICTURES})
owned(pictures ${PICTURES})

# The Mandrill as one-sample gray TIFFs, using the dither's own grays: at
# full size, and at half size.  bench/grays.c writes them, and the
# benchmarks have them on a disk of their own.
set(GRAYS ${BUILD}/mandrill-grays.tiff)
set(SMALL ${BUILD}/mandrill-small.tiff)

# A disk for the desktop, made as QEMU's is: block 1 for the editor, and an
# image's tables of names.  The picture is not on it: that is the
# pictures' disk (above).  mkdisk's output goes to a .log file of the same
# name beside it.
function(desk_disk DISK TABLES)
    get_filename_component(AT ${DISK} DIRECTORY)
    get_filename_component(LOG ${DISK} NAME_WE)
    add_custom_command(
            OUTPUT            ${DISK}
            COMMAND           ${CMAKE_COMMAND} -E make_directory ${AT}
            COMMAND           mkdisk ${DISK} ${CMAKE_SOURCE_DIR}/baremetal/block1.fs ${TABLES}
                                > ${AT}/${LOG}.log 2>&1
            DEPENDS           mkdisk baremetal/block1.fs ${TABLES})
endfunction()

# Two more of that database, smoother than the Mandrill, for what is
# measured of pictures (tiles-report): Peppers and the F-16.  Nothing else
# reads them.
set(PEPPERS ${BUILD}/peppers.tiff)
set(AIRPLANE ${BUILD}/airplane.tiff)
fetched(${PEPPERS} 4.2.07)
fetched(${AIRPLANE} 4.2.05)

# The seeds the tests that try things by chance are each run with.
set(SEEDS 3 4 CACHE STRING "the seeds the tests that try things by chance are run with")

# What the reference VM is made of, the nucleus on it typed at
# (test/nucleus.c), and the desktop on it (test/deskvm.c) with what looks at
# its screen.
set(VM_SOURCES vm.c vm_opcodes.c vm_debug.c util/stack.c)
set(TYPED_SOURCES test/nucleus.c vm.c)
set(DESK_SOURCES test/deskvm.c test/deskcheck.c test/tables.c test/dither.c bench/tiff.c)

# A machine is two things.  It is sizes, which the generator is told and
# makes its definition of (basewords.fs): no tool is built for them, since
# cross.fs says them beside every image it makes (nuc.machine), and a tool
# is the machine of the image it is given.  And it is a system: the images
# cross.fs makes for it and the disks that go with them.  A machine of
# util/machines is both under one name.  QEMU's has the sizes of one of
# those and a system of its own, since its ROM has another disk's driver.
#
# What is made for a system is the target machine-NAME's to make.
# Whatever else is made from one of those files depends on the file, so
# that it is made again when the file is, and on the target, so that the
# file is made in one place and not by two targets at once.  made() says a
# file is that target's.
function(made NAME)
    set_property(GLOBAL APPEND PROPERTY MADE_FOR_${NAME} ${ARGN})
    owned(machine-${NAME} ${ARGN})
endfunction()

# A system: its name, which is its directory of build/ and what its
# targets are named for, and the machine whose sizes it has.
#
#   system(NAME OF
#          [SET name=value...]  what cross.fs is told for every image of it
#                               (ROM=virt-disk.fs)
#          [AFTER what...])     what its nucleus is made after
#
# Its nucleus is made after the default machine's (forth-env), which is
# where the two made files of Forth are made.  A system whose machine has
# a display has the names that go with its image (nuc.tables), a disk for
# its desktop, and the image it boots from a disk with (BOOT=on): the
# system without what forth/apps.fs lists, which the boot compiles from
# the disk (doc/boot.md).  boot-system-NAME makes that in a directory of
# its own, with its two disks, after the nucleus, since both write what
# fuse.h keeps in build/.
function(system NAME OF)
    cmake_parse_arguments(IT "" "" "SET;AFTER" ${ARGN})
    set(AT ${BUILD}/${NAME})
    set(WORDS ${BUILD}/${OF}/basewords.fs)
    set(NAMES)
    if("${SIZES_OF_${OF}}" MATCHES "DISP_W=")
        set(NAMES tables)                               # the names, which a machine with a display has
    endif()
    image(${AT}
          WORDS ${WORDS}
          SET ${IT_SET}
          MAKES hex lst sizes inline ${NAMES}
          AFTER forth-env ${IT_AFTER}
          IS "The nucleus for the machine ${NAME}")
    set(NUCLEUS ${IMAGE_FILES})
    made(${NAME} ${NUCLEUS})
    image_owned(machine-${NAME} ${AT})
    if(NOT NAMES)
        return()
    endif()

    # The desktop's disk: block 1 for the editor, and the names that go
    # with this image.
    desk_disk(${AT}/disk.img ${AT}/nuc.tables)
    made(${NAME} ${AT}/disk.img)

    image(${AT}/boot
          WORDS ${WORDS}
          SET ${IT_SET} BOOT=on
          MAKES hex tables
          AFTER machine-${NAME} ${NUCLEUS}
          IS "The image the machine ${NAME} boots from a disk with")
    # Its disk: block 1 is forth/apps.fs, and after it each file it
    # includes, which the boot compiles from its source (mkdisk follows
    # the includes, so apps.fs is the one list).
    add_custom_command(
            OUTPUT            ${AT}/boot/apps.img ${AT}/boot/apps.ipl
            COMMAND           mkdisk --boot ${AT}/boot/nuc.hex ${AT}/boot/apps.img ${AT}/boot/apps.ipl
                                ${CMAKE_SOURCE_DIR}/forth/apps.fs > ${AT}/boot/apps-disk.log 2>&1
            DEPENDS           mkdisk ${AT}/boot/nuc.hex ${FORTH_SOURCES})
    # And the disk it starts its desktop from: the same, with the
    # desktop's own things at their blocks (mkdisk --boot-desk), and the
    # names that go with that image.
    add_custom_command(
            OUTPUT            ${AT}/boot/desk.img ${AT}/boot/desk.ipl
            COMMAND           mkdisk --boot-desk ${AT}/boot/nuc.hex ${AT}/boot/desk.img ${AT}/boot/desk.ipl
                                ${AT}/boot/nuc.tables
                                ${CMAKE_SOURCE_DIR}/forth/apps.fs > ${AT}/boot/desk-disk.log 2>&1
            DEPENDS           mkdisk ${AT}/boot/nuc.hex ${FORTH_SOURCES})
    add_custom_target(boot-system-${NAME} DEPENDS ${AT}/boot/nuc.hex ${AT}/boot/apps.img ${AT}/boot/desk.img)
    image_owned(boot-system-${NAME} ${AT}/boot)
    owned(boot-system-${NAME} ${AT}/boot/apps.img ${AT}/boot/apps.ipl ${AT}/boot/desk.img ${AT}/boot/desk.ipl)
endfunction()

# A system's nucleus again with the peephole off (PEEPHOLE=off), which
# plain-NAME makes in a directory of its own.  It reads the calls the first
# build compiled in place (nuc.inline), so it is made after it.  It is made
# of the machine the two builds are compared on, and the reports are of.
function(plain NAME)
    set(AT ${BUILD}/${NAME})
    image(${AT}/plain
          WORDS ${AT}/basewords.fs
          SET PEEPHOLE=off
          MAKES hex tables
          FIRST ${CMAKE_COMMAND} -E copy ${AT}/nuc.inline ${AT}/plain/nuc.inline
          AFTER machine-${NAME} ${AT}/nuc.hex ${AT}/nuc.inline
          IS "The nucleus for the machine ${NAME}, with the peephole off")
    add_custom_target(plain-${NAME} DEPENDS ${IMAGE_FILES})
    image_owned(plain-${NAME} ${AT}/plain)
endfunction()

# A second system: the picture alone (forth/picture.fs), which
# picture-system-NAME makes in a directory of its own, after the nucleus.
# It is for the machine of 64K cells, which with the whole desktop has not
# the memory to dither the picture at its own size (bench_on).
function(picture_system NAME)
    set(AT ${BUILD}/${NAME})
    image(${AT}/picture
          WORDS ${AT}/basewords.fs
          SET SYSTEM=picture.fs
          AFTER machine-${NAME} ${AT}/nuc.hex
          IS "The system with the picture alone, for the machine ${NAME}")
    add_custom_target(picture-system-${NAME} DEPENDS ${IMAGE_FILES})
    image_owned(picture-system-${NAME} ${AT}/picture)
endfunction()

function(machine NAME)
    set(AT ${BUILD}/${NAME})
    set(SIZES_OF_${NAME} ${ARGN} CACHE INTERNAL "the sizes of the machine ${NAME}, each as the name vm_constants.h has for it and what it is")

    # Its definition for cross.fs, which the generator makes of the machine
    # as it is told it: the bits of a cell, the bits of an address and the
    # display's width, height and first cell, as far as they are said.
    set(SAID)
    foreach(SIZE CELL_BITS ADDR_BITS DISP_W DISP_H DISP_BASE)
        if("${ARGN}" MATCHES "(^|;)${SIZE}=([^;]*)")
            list(APPEND SAID ${CMAKE_MATCH_2})
        endif()
    endforeach()
    add_custom_command(
            OUTPUT            ${AT}/basewords.fs
            COMMAND           ${CMAKE_COMMAND} -E make_directory ${AT}
            COMMAND           basewords-fs-gen ${AT}/basewords.fs ${SAID} > ${AT}/basewords.log
            DEPENDS           basewords-fs-gen)
    system(${NAME} ${NAME})
endfunction()

# When all that is made for a machine has been said.
function(machine_made NAME)
    get_property(FILES GLOBAL PROPERTY MADE_FOR_${NAME})
    add_custom_target(machine-${NAME} DEPENDS ${FILES})
endfunction()

# A tool or a test that has the machine in it, or reads a machine's image.
# It is one program for every machine: the machine is the one whose image it
# is given, which the image says beside it (util/image.h), and no size of a
# machine is compiled into it.  Its machine stops at a fetch or a store
# outside memory (STRICT).  What every such tool has (COMMON_SOURCES: the
# words, the dictionary, a stack printed, an image read) is compiled once,
# and a tool says the sources that are its own.
set(COMMON_SOURCES vm_opcodes.c vm_debug.c util/stack.c util/image.c)
add_library(common STATIC ${COMMON_SOURCES})
target_compile_options(common PRIVATE -UDEBUG -w)
function(machine_tool TOOL)
    set(OWN ${ARGN})
    list(REMOVE_ITEM OWN ${COMMON_SOURCES})
    tool(${TOOL} ${OWN})
    target_compile_definitions(${TOOL} PRIVATE STRICT)
    target_link_libraries(${TOOL} common)
endfunction()

# A tool that has the machine's step in it (util/fuse.h).  It too is one
# program for every machine: its cell is that of the image it is given, and
# fuse.h says what the rest of the machine it tries things on is.
function(stepping_tool TOOL)
    tool(${TOOL} ${ARGN})
    target_include_directories(${TOOL} PRIVATE util)
endfunction()

# The reference VM (main.c): hexaforth IMAGE.hex [DISK.img [MEMORY.out]].
machine_tool(hexaforth main.c vm.c)
# An image's code, word by word, as the decompiler shows it, with every
# instruction's place, number and fields (util/disasm_hex.c, which says at
# its top what it shows): disasm_hex IMAGE.hex [NAME... | FROM TO].  It has
# the machine in it, for where a branch goes.
machine_tool(disasm_hex util/disasm_hex.c test/nucleus.c)
# The reference VM, counting how often the instruction at each place is run
# (bench/counting.c): counting IMAGE.hex [DISK.img [MEMORY.out]], and the
# counts written where RUN_COUNTS says.
machine_tool(counting bench/counting.c)
# What work counted that way costs (bench/costs.c); and its loops, each
# with the fewest instructions found that do a turn of it (bench/shorter.c).
stepping_tool(costs bench/costs.c util/image.c)
stepping_tool(shorter bench/shorter.c util/image.c)
target_compile_definitions(shorter PRIVATE SEARCH_STATES=${SEARCH_STATES})
# Two measurements of the same work, by word (bench/compared.c): compared
# WAS WAS.work IS IS.work, of what is built and of what was kept
# (build/kept/, and build/kept/log.txt says what there is).
stepping_tool(compared bench/compared.c util/image.c)

# A test of a machine: what it is called, what is built for it, and what
# that is run with.  It is run from where the source is.  It needs what is
# built for it, and what is made for the machine: its image, its disks.  A
# test that reads more than that says so itself.
function(machine_test TEST NAME TOOL)
    add_test(NAME ${TEST} COMMAND ${TOOL} ${ARGN} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    needs(${TEST} ${TOOL} machine-${NAME})
    on_the_vm(${TEST})
endfunction()

# The tests that type at the prompt of a machine's reference VM: the
# nucleus; comments, and a block of a disk read as source; regions a cell
# at a time against the one form of their pixels; the dither against a
# dither a pixel at a time; the TIFF reader; and for a machine with a
# display, the drawing words against a model of them, and the desktop, its
# windows worked with keys and a pointer and its screen looked at.  The
# same programs and the same tests on every machine: what is different is
# the image.  The two that start a machine from its ROM stop at a store
# into it, where a machine whose kernel is a ROM would go wrong (ROM_BYTES).
machine_tool(prompt_test test/prompt_test.c ${TYPED_SOURCES})
machine_tool(source_test test/source_test.c ${TYPED_SOURCES})
machine_tool(boot_test test/boot_test.c test/tables.c ${TYPED_SOURCES})
target_compile_definitions(boot_test PRIVATE ROM_BYTES=KROM_BYTES)
machine_tool(region_test test/region_test.c ${TYPED_SOURCES})
machine_tool(dither_test test/dither_test.c test/dither.c ${TYPED_SOURCES})
machine_tool(tiff_test test/tiff_test.c test/dither.c ${TYPED_SOURCES})
machine_tool(display_test test/display_test.c test/tables.c ${TYPED_SOURCES})
machine_tool(gray_test test/gray_test.c test/dither.c ${TYPED_SOURCES})
machine_tool(desktop_test test/desktop_test.c ${DESK_SOURCES} ${TYPED_SOURCES})
target_compile_definitions(desktop_test PRIVATE ROM_BYTES=KROM_BYTES)
# What a machine compiled at its boot, beside the same words as cross.fs
# lays them (bench/booted.c): the target booted-NAME.
machine_tool(booted_sizes bench/booted.c)
function(machine_tests NAME)
    set(AT ${BUILD}/${NAME})
    set(NUC ${AT}/nuc.hex)

    machine_test(prompt-${NAME} ${NAME} prompt_test ${NUC})

    # The block is a file that is there for this, laid down as a disk's
    # source is.
    add_custom_command(
            OUTPUT            ${AT}/source.img
            COMMAND           ${CMAKE_COMMAND} -E make_directory ${AT}
            COMMAND           mkdisk ${AT}/source.img ${CMAKE_SOURCE_DIR}/test/source.fs > ${AT}/source-disk.log 2>&1
            DEPENDS           mkdisk test/source.fs)
    machine_test(source-${NAME} ${NAME} source_test ${NUC} ${AT}/source.img)
    made(${NAME} ${AT}/source.img)

    # The machine started from its ROM alone, and a disk mkdisk made of the
    # same image, with test/boot.fs as its block 1 and test/boot-more.fs
    # after it (doc/boot.md); or with no disk and what mkdisk wrote for the
    # console; or with a disk whose block 1 is test/boot-error.fs.
    add_custom_command(
            OUTPUT            ${AT}/boot.img ${AT}/boot.ipl ${AT}/boot-error.img ${AT}/boot-error.ipl
            COMMAND           ${CMAKE_COMMAND} -E make_directory ${AT}
            COMMAND           mkdisk --boot ${NUC} ${AT}/boot.img ${AT}/boot.ipl
                                ${CMAKE_SOURCE_DIR}/test/boot.fs ${CMAKE_SOURCE_DIR}/test/boot-more.fs
                                > ${AT}/boot-disk.log 2>&1
            COMMAND           mkdisk --boot ${NUC} ${AT}/boot-error.img ${AT}/boot-error.ipl
                                ${CMAKE_SOURCE_DIR}/test/boot-error.fs >> ${AT}/boot-disk.log 2>&1
            DEPENDS           mkdisk ${NUC} test/boot.fs test/boot-more.fs test/boot-error.fs)
    # A second disk, for the checks that give a machine two: an index and
    # the files of test/second/, one of them under a label the first disk
    # has too.
    add_custom_command(
            OUTPUT            ${AT}/second.img
            COMMAND           ${CMAKE_COMMAND} -E make_directory ${AT}
            COMMAND           mkdisk --disk ${AT}/second.img
                                ${CMAKE_SOURCE_DIR}/test/second/second.fs ${CMAKE_SOURCE_DIR}/test/second/boot-more.fs
                                > ${AT}/second-disk.log 2>&1
            DEPENDS           mkdisk test/second/second.fs test/second/boot-more.fs)
    made(${NAME} ${AT}/second.img)
    # And the disk of the image the machine boots from (boot-system-NAME).
    machine_test(boot-${NAME} ${NAME} boot_test ${NUC} ${AT}/boot.img ${AT}/boot.ipl ${AT}/boot-error.img forth
                 ${AT}/boot/apps.img ${AT}/second.img)
    needs(boot-${NAME} boot-system-${NAME})
    made(${NAME} ${AT}/boot.img ${AT}/boot.ipl ${AT}/boot-error.img)

    foreach(SEED ${SEEDS})
        machine_test(regions-${NAME}-${SEED} ${NAME} region_test ${NUC} ${SEED})
        machine_test(dither-${NAME}-${SEED} ${NAME} dither_test ${NUC} ${SEED})
        machine_test(tiff-${NAME}-${SEED} ${NAME} tiff_test ${NUC} ${SEED})
    endforeach()

    if(NOT "${SIZES_OF_${NAME}}" MATCHES "DISP_W=")
        return()
    endif()

    foreach(SEED ${SEEDS})
        machine_test(display-${NAME}-${SEED} ${NAME} display_test ${NUC} forth ${SEED})
        machine_test(gray-${NAME}-${SEED} ${NAME} gray_test ${NUC} ${SEED})
    endforeach()

    # The desktop, with the system's disk for it (system); and the pictures'
    # disk as its second, which has the Mandrill.  The test keeps its
    # screens, and its copy of the first disk, in a directory of its own.
    file(MAKE_DIRECTORY ${AT}/desktop)
    machine_test(desktop-${NAME} ${NAME} desktop_test ${NUC} ${AT}/disk.img ${PICTURES} ${PICTURE} forth
                 ${AT}/desktop names)
    needs(desktop-${NAME} picture pictures)

    # The same checks on the machine started from its ROM alone, with a disk
    # it can start from that is the desktop's too (boot-system-NAME): the
    # desktop they are run on is the one its boot compiled from the disk's
    # source (doc/boot.md).
    file(MAKE_DIRECTORY ${AT}/booted)
    machine_test(desktop-booted-${NAME} ${NAME} desktop_test ${AT}/boot/nuc.hex ${AT}/boot/desk.img ${PICTURES}
                 ${PICTURE} forth ${AT}/booted names rom)
    needs(desktop-booted-${NAME} boot-system-${NAME} picture pictures)

    # What the machine compiled at that boot (booted_sizes).
    report(booted-${NAME}
            COMMAND           ${CMAKE_COMMAND} -E env BOOTED=${AT}/booted/memory
                                $<TARGET_FILE:desktop_test> ${AT}/boot/nuc.hex ${AT}/boot/desk.img ${PICTURES}
                                ${PICTURE} forth ${AT}/booted names rom > ${AT}/booted/checks.txt
            COMMAND           booted_sizes ${AT}/boot/nuc.hex ${AT}/boot/nuc.tables ${AT}/booted/memory
                                ${AT}/booted/disk.img ${AT}/nuc.hex ${AT}/nuc.tables > ${AT}/booted/sizes.txt
            COMMAND           ${CMAKE_COMMAND} -E echo "what the machine ${NAME} compiled at its boot: ${AT}/booted/sizes.txt"
            COMMAND           ${KEEP} booted ${AT}/boot/nuc.hex ${AT}/booted/sizes.txt
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endfunction()

# After any change to a compiler or a peephole: the image against the image
# built with the peephole off (test/builds_test.c, which says how it
# compares them).
stepping_tool(builds_test test/builds_test.c util/image.c)
function(builds_test NAME)
    set(AT ${BUILD}/${NAME})
    add_test(NAME builds
             COMMAND builds_test ${AT}/plain/nuc.hex ${AT}/plain/nuc.tables ${AT}/nuc.hex ${AT}/nuc.tables
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    needs(builds builds_test machine-${NAME} plain-${NAME})
    on_the_vm(builds)
endfunction()

# What a system's image is made of (util/examine.c, as the target `examine`
# says it of the image where the source is): the target examine-NAME, of the
# system NAME, by the examiner built for the machine OF whose sizes it has.
# The kernel is to fit in its ROM on every machine, and this is what says
# how long it is on each.
function(examined NAME OF)
    set(AT ${BUILD}/${NAME})
    report(examine-${NAME}
            COMMAND           examine-image ${AT}/nuc.hex ${AT}/nuc.tables ${AT}/nuc.sizes > ${AT}/examine.txt
            COMMAND           ${CMAKE_COMMAND} -E cat ${AT}/examine.txt
            COMMAND           ${KEEP} examine ${AT}/nuc.hex ${AT}/examine.txt
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endfunction()

# The whole of a machine's image as it is laid out, to be read for a better
# way of laying it: listing-NAME writes build/NAME/listing.txt
# (util/disasm_hex.c, --layout, which says what is in it).  The desktop is
# worked first on the VM that counts (bench/profile.c, as the target
# `profile` works it), so that each call to a word on another page says how
# often it was run; what that left is in build/NAME/listing/.
#
# The reference VM that counts how often the instruction at each place is
# run (bench/counting.c), in place of vm.c, is the VM the desktop is worked
# on (profiler); by itself it is `counting`.
set(COUNTED_SOURCES test/nucleus.c bench/counting.c)
machine_tool(profiler bench/profile.c ${DESK_SOURCES} ${COUNTED_SOURCES})
target_compile_definitions(profiler PRIVATE COUNTING_A_TEST)
function(listed NAME)
    set(AT ${BUILD}/${NAME})
    set(L ${AT}/listing)
    report(listing-${NAME}
            COMMAND           ${CMAKE_COMMAND} -E make_directory ${L}
            COMMAND           profiler ${AT}/nuc.hex ${AT}/nuc.tables ${AT}/nuc.sizes ${AT}/disk.img ${PICTURES} ${PICTURE}
                                forth ${L} > ${L}/profile.txt 2> ${L}/profile.log
            COMMAND           disasm_hex ${AT}/nuc.hex --no-color --layout --counts ${L}/desktop.counts
                                > ${AT}/listing.txt 2> ${L}/listing.log
            COMMAND           ${CMAKE_COMMAND} -E echo "the image of the machine ${NAME}, laid out: ${AT}/listing.txt"
            COMMAND           ${KEEP} listing ${AT}/nuc.hex ${AT}/listing.txt ${L}/desktop.counts ${L}/profile.txt
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT           "The machine ${NAME}'s image as it is laid out (what went wrong, if anything did, is in ${L})")
endfunction()

# Where a machine's memory goes: memory-map-NAME prints the map (util/map.c)
# of its image as cross.fs built it; of the machine started from that with
# its disk, at its prompt; of the machine that boots from a disk
# (boot-system-NAME), once its boot has compiled what the disk has; and of
# the desktop, when its checks are done.  The last three are run on the
# reference VM that watches the heap (bench/heap.c: `heap` by itself, and
# as the VM in the desktop's test, desktop_test_heap), and after the map of
# each is what the heap was given out to while it ran.  Each map, what the
# watch said and the memory they are of are kept in build/NAME/map/.
set(WATCHED_SOURCES test/nucleus.c bench/heap.c)
machine_tool(map util/map.c)
machine_tool(heap bench/heap.c)
machine_tool(desktop_test_heap test/desktop_test.c ${DESK_SOURCES} ${WATCHED_SOURCES})
target_compile_definitions(desktop_test_heap PRIVATE WATCHING_A_TEST)
function(mapped NAME)
    set(AT ${BUILD}/${NAME})
    set(MP ${AT}/map)
    report(memory-map-${NAME}
            COMMAND           ${CMAKE_COMMAND} -E make_directory ${MP}/desktop
            COMMAND           map ${AT}/nuc.hex ${AT}/nuc.tables > ${MP}/image.txt
            COMMAND           ${CMAKE_COMMAND} -E copy ${AT}/disk.img ${MP}/disk.img
            COMMAND           ${CMAKE_COMMAND} -E env HEAP=${MP}/prompt.heap HEAP_NAMES=${AT}/nuc.tables
                                $<TARGET_FILE:heap> ${AT}/nuc.hex ${MP}/disk.img ${MP}/prompt.mem
                                < ${CMAKE_SOURCE_DIR}/bench/idle.fs > ${MP}/prompt.out 2>&1
            COMMAND           map ${AT}/nuc.hex ${AT}/nuc.tables ${MP}/prompt.mem > ${MP}/prompt.txt
            COMMAND           ${CMAKE_COMMAND} -E copy ${AT}/boot/apps.img ${MP}/apps.img
            COMMAND           ${CMAKE_COMMAND} -E env HEAP=${MP}/boot.heap HEAP_NAMES=${AT}/boot/nuc.tables
                                $<TARGET_FILE:heap> ${AT}/boot/nuc.hex ${MP}/apps.img ${MP}/boot.mem
                                < ${CMAKE_SOURCE_DIR}/bench/idle.fs > ${MP}/boot.out 2>&1
            COMMAND           map ${AT}/boot/nuc.hex ${AT}/boot/nuc.tables ${MP}/boot.mem > ${MP}/boot.txt
            COMMAND           ${CMAKE_COMMAND} -E env MEMORY=${MP}/desktop.mem HEAP=${MP}/desktop.heap
                                HEAP_NAMES=${AT}/nuc.tables
                                $<TARGET_FILE:desktop_test_heap> ${AT}/nuc.hex ${AT}/disk.img ${PICTURES}
                                ${PICTURE} forth ${MP}/desktop names > ${MP}/desktop.out 2>&1
            COMMAND           map ${AT}/nuc.hex ${AT}/nuc.tables ${MP}/desktop.mem > ${MP}/desktop.txt
            COMMAND           ${CMAKE_COMMAND} -E cat ${MP}/image.txt ${MP}/prompt.txt ${MP}/prompt.heap
                                ${MP}/boot.txt ${MP}/boot.heap ${MP}/desktop.txt ${MP}/desktop.heap
            COMMAND           ${KEEP} memory-map ${AT}/nuc.hex ${MP}/image.txt ${MP}/prompt.txt ${MP}/prompt.heap
                                ${MP}/desktop.txt ${MP}/desktop.heap
            COMMAND           ${KEEP} memory-map ${AT}/boot/nuc.hex ${MP}/boot.txt ${MP}/boot.heap
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            VERBATIM)

    # The same as a table (map --table): what each part of memory has used
    # and has left, of the system as cross.fs built it and of the machine
    # that boots without the applications and has compiled them from its
    # disk.  That machine is run to its prompt here, and stopped if it runs
    # on (vm_stopped.h).  memory-table-NAME makes build/NAME/map/table.txt,
    # which quick and check show.
    report(memory-table-${NAME}
            COMMAND           ${CMAKE_COMMAND} -E make_directory ${MP}
            COMMAND           ${CMAKE_COMMAND} -E copy ${AT}/boot/apps.img ${MP}/table.img
            COMMAND           ${CMAKE_COMMAND} -E env STOP_BUDGET=2000000000
                                $<TARGET_FILE:hexaforth> ${AT}/boot/nuc.hex ${MP}/table.img ${MP}/table.mem
                                < ${CMAKE_SOURCE_DIR}/bench/idle.fs > ${MP}/table.out 2>&1
            COMMAND           map --table ${AT}/nuc.hex ${AT}/nuc.tables ${AT}/boot/nuc.hex ${AT}/boot/nuc.tables
                                ${MP}/table.mem > ${MP}/table.txt
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            VERBATIM)
    set_property(GLOBAL APPEND PROPERTY MEMORY_TABLES ${MP}/table.txt)
    set_property(GLOBAL APPEND PROPERTY MEMORY_TABLED memory-table-${NAME})
endfunction()

# `compiling-64` and the like: what it costs a machine to boot from its
# disk and compile from source there what its image is without.  The
# reference VM that counts (bench/counting.c) is started with the boot's
# image in memory, the disk the desktop boots from and the pictures' disk,
# and runs to its prompt; then what the work costs is said (bench/costs.c):
# how many instructions and cycles, each word with all it calls, and the
# loops.  What the machine said, the work by place and the reports are in
# build/NAME/compiling/, where `disasm_hex ... --counts` reads the work to
# say how often each instruction of a word was run.  And the reference VM
# that watches the dictionary searched (bench/finds.c), for finding-NAME.
machine_tool(finds bench/finds.c)
function(compiled NAME)
    set(AT ${BUILD}/${NAME})
    set(CP ${AT}/compiling)
    report(compiling-${NAME}
            COMMAND           ${CMAKE_COMMAND} -E make_directory ${CP}
            COMMAND           ${CMAKE_COMMAND} -E env RUN_WORK=${CP}/work
                                $<TARGET_FILE:counting> ${AT}/boot/nuc.hex ${AT}/boot/desk.img,${PICTURES}
                                < ${CMAKE_SOURCE_DIR}/bench/idle.fs > ${CP}/boot.out 2>&1
            COMMAND           costs ${AT}/boot/nuc.hex ${AT}/boot/nuc.tables ${AT}/boot/nuc.sizes ${CP}/work ${CP}
                                > ${CP}/costs.txt 2> ${CP}/costs.log
            COMMAND           ${CMAKE_COMMAND} -E cat ${CP}/boot.out ${CP}/costs.txt
            COMMAND           ${KEEP} compiling ${AT}/boot/nuc.hex ${CP}/work ${CP}/boot.out ${CP}/costs.txt
                                ${CP}/paths.txt ${CP}/loops.txt ${CP}/vars.txt
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            VERBATIM)

    # `finding-64` and the like: what that boot asks the dictionary for.
    # The same machine is run on the reference VM that watches sfind
    # (bench/finds.c): how often it is asked, how many headers it looks at,
    # and each name by what was looked at for it.  It is in
    # build/NAME/compiling/finds.txt.
    report(finding-${NAME}
            COMMAND           ${CMAKE_COMMAND} -E make_directory ${CP}
            COMMAND           ${CMAKE_COMMAND} -E env FINDS=${CP}/finds.txt FINDS_NAMES=${AT}/boot/nuc.tables
                                $<TARGET_FILE:finds> ${AT}/boot/nuc.hex ${AT}/boot/desk.img,${PICTURES}
                                < ${CMAKE_SOURCE_DIR}/bench/idle.fs > ${CP}/finds.out 2>&1
            COMMAND           ${CMAKE_COMMAND} -E cat ${CP}/finds.txt
            COMMAND           ${KEEP} finding ${AT}/boot/nuc.hex ${CP}/finds.txt
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            VERBATIM)
endfunction()

# What a machine's display shows, from its memory as the reference VM
# writes it when it has stopped (util/screen.c): `screen`, which the target
# `seen` builds, with what makes a PNG of one (util/png.c).
machine_tool(screen util/screen.c)

# util/machines, a line a machine: the bits of a cell, the log2 of memory's
# cells, and the display, across and down and the cell it starts at.  The
# first of them is the one the two builds are compared on, and that the
# reports are of.
file(STRINGS util/machines MACHINE_LINES REGEX "^[^#]")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS util/machines)
set(MACHINES)
foreach(LINE ${MACHINE_LINES})
    separate_arguments(SAID UNIX_COMMAND "${LINE}")
    list(GET SAID 0 CELL)
    list(GET SAID 1 ADDR)
    set(ITS_SIZES CELL_BITS=${CELL} ADDR_BITS=${ADDR})
    list(LENGTH SAID HOW_MUCH)
    if(HOW_MUCH GREATER 2)
        list(GET SAID 2 ACROSS)
        list(GET SAID 3 DOWN)
        list(GET SAID 4 STARTS)
        list(APPEND ITS_SIZES DISP_W=${ACROSS} DISP_H=${DOWN} DISP_BASE=${STARTS})
    endif()
    machine(${CELL} ${ITS_SIZES})
    machine_tests(${CELL})
    machine_made(${CELL})
    if(HOW_MUCH GREATER 2)
        examined(${CELL} ${CELL})                       # the names it reads are a machine's that has a display
        listed(${CELL})
        mapped(${CELL})
        compiled(${CELL})
        if(ADDR EQUAL 16)                               # 64K cells
            picture_system(${CELL})
        endif()
    endif()
    list(APPEND MACHINES ${CELL})
endforeach()
list(GET MACHINES 0 FIRST)
plain(${FIRST})
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

# The desktop's test on the VM that counts (COUNTED_SOURCES, with listed,
# where what works the desktop on it, profiler, is too).
machine_tool(desktop_test_counted test/desktop_test.c ${DESK_SOURCES} ${COUNTED_SOURCES})
target_compile_definitions(desktop_test_counted PRIVATE COUNTING_A_TEST)

# `screenshot`: the desktop with every window open and HexaPaint in front,
# as the README shows it (doc/desktop.png, which is checked in).  The
# desktop's test is run with a directory of its own for its screens, and
# the screen it leaves when HexaPaint has been drawn in, put behind another
# window and brought back to the front is made a PNG (util/png.c).  The
# target makes it again when the desktop looks otherwise.
set(SHOT ${BUILD}/screenshot)
tool(png util/png.c)
add_custom_target(seen DEPENDS screen png)

# `hello`: a window made by what is typed at the prompt (doc/hello.fs,
# which DESKTOP.md is about), on the reference VM of the first machine: what
# it printed, and its display as a picture (build/hello/hello.png).
set(HELLO ${BUILD}/hello)
report(hello
        COMMAND           ${CMAKE_COMMAND} -E make_directory ${HELLO}
        COMMAND           hexaforth ${NUC} ${OF}/disk.img,${PICTURES} ${HELLO}/memory.out
                            < doc/hello.fs > ${HELLO}/hello.txt 2>&1
        COMMAND           ${CMAKE_COMMAND} -E cat ${HELLO}/hello.txt
        COMMAND           screen ${NUC} ${HELLO}/memory.out ${HELLO}/hello.pbm
        COMMAND           png ${HELLO}/hello.pbm ${HELLO}/hello.png
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
report(screenshot
        COMMAND           ${CMAKE_COMMAND} -E make_directory ${SHOT}/desktop
        COMMAND           $<TARGET_FILE:desktop_test> ${NUC} ${OF}/disk.img ${PICTURES} ${PICTURE} forth
                            ${SHOT}/desktop names > ${SHOT}/desktop.log 2>&1
        COMMAND           png ${SHOT}/desktop/16-again.pbm doc/desktop.png
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
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
        COMMAND           ${CMAKE_COMMAND} -E make_directory ${PROFILE}
        COMMAND           ${PROFILED} ${PROFILE}
                            > ${PROFILE}/profile.txt 2> ${PROFILE}/profile.log
        COMMAND           ${CMAKE_COMMAND} -E cat ${PROFILE}/profile.txt
        COMMAND           costs ${NUC} ${TABLES} ${SIZES} ${PROFILE}/desktop.counts ${PROFILE} ${PROFILE_FILES}
                            2> ${PROFILE}/costs.log
        COMMAND           survey ${NUC} ${TABLES} ${PROFILE}/desktop.counts ${PROFILE}/survey.txt
                            2> ${PROFILE}/survey.log
        COMMAND           shorter ${NUC} ${TABLES} ${PROFILE}/desktop.counts ${PROFILE}/shorter.txt
                            2> ${PROFILE}/shorter.log
        COMMAND           ${KEEP} profile ${NUC} ${PROFILE}/profile.txt ${PROFILE}/desktop.counts ${PROFILE}/paths.txt
                            ${PROFILE}/loops.txt ${PROFILE}/vars.txt ${PROFILE}/survey.txt ${PROFILE}/shorter.txt
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
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
        COMMAND           ${CMAKE_COMMAND} -E make_directory ${USES}
        COMMAND           ${PROFILED} ${USES} > ${USES}/profile.txt 2> ${USES}/profile.log
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT           "The desktop's work, counted (${USES}/profile.log says what went wrong, if anything did)"
        DEPENDS           profiler machine-${FIRST} ${NUC} ${OF}/disk.img picture ${PICTURES})
report(subtracts
        COMMAND           uses ${OF}/plain/nuc.hex ${OF}/plain/nuc.tables ${NUC} ${TABLES} ${USES}/desktop.counts
                            - 1- negate > ${USES}/subtracts.txt
        COMMAND           ${CMAKE_COMMAND} -E cat ${USES}/subtracts.txt
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
        COMMAND           ${CMAKE_COMMAND} -E make_directory ${PEEPHOLE}/desktop
        COMMAND           runs ${OF}/plain/nuc.hex ${OF}/plain/nuc.tables ${NUC} ${TABLES}
                            > ${PEEPHOLE}/runs.txt 2> ${PEEPHOLE}/runs.log
        COMMAND           typed type ${PEEPHOLE}/runs.txt ${NUC} ${TABLES}
                            > ${PEEPHOLE}/typed.fs 2> ${PEEPHOLE}/typed.log
        COMMAND           hexaforth ${NUC} < ${PEEPHOLE}/typed.fs > ${PEEPHOLE}/typed.out 2>&1
        COMMAND           typed read ${PEEPHOLE}/runs.txt ${PEEPHOLE}/typed.out
                            > ${PEEPHOLE}/typed.txt 2> ${PEEPHOLE}/typed.log
        COMMAND           ${CMAKE_COMMAND} -E copy ${OF}/disk.img ${PEEPHOLE}/disk.img
        COMMAND           ${CMAKE_COMMAND} -E rm -f ${PEEPHOLE}/idle.counts ${PEEPHOLE}/draw.counts ${PEEPHOLE}/desktop.counts
        COMMAND           ${CMAKE_COMMAND} -E env RUN_COUNTS=${PEEPHOLE}/idle.counts
                            $<TARGET_FILE:counting> ${NUC} ${PEEPHOLE}/disk.img
                            < bench/idle.fs > ${PEEPHOLE}/idle.log 2>&1
        COMMAND           ${CMAKE_COMMAND} -E env RUN_COUNTS=${PEEPHOLE}/draw.counts RUN_WORK=${PEEPHOLE}/draw.work
                            $<TARGET_FILE:counting> ${NUC} ${PEEPHOLE}/disk.img
                            < bench/draw.fs > ${PEEPHOLE}/draw.log 2>&1
        COMMAND           ${CMAKE_COMMAND} -E env RUN_COUNTS=${PEEPHOLE}/desktop.counts
                            $<TARGET_FILE:desktop_test_counted> ${NUC} ${PEEPHOLE}/disk.img ${PICTURES}
                            ${PICTURE} forth ${PEEPHOLE}/desktop names > ${PEEPHOLE}/desktop.log 2>&1
        COMMAND           weigh ${PEEPHOLE}/typed.txt ${SIZES} ${PEEPHOLE} desktop draw
                            > ${PEEPHOLE}/peephole.txt 2> ${PEEPHOLE}/weigh.log
        COMMAND           pair-rules seen ${NUC} ${PEEPHOLE}/typed.txt > ${PEEPHOLE}/seen.txt 2> ${PEEPHOLE}/seen.log
        COMMAND           typed shown ${PEEPHOLE}/typed.txt forth/fused.fs
                            > ${PEEPHOLE}/shown.txt 2> ${PEEPHOLE}/shown.log
        COMMAND           ${CMAKE_COMMAND} -E cat ${PEEPHOLE}/peephole.txt ${PEEPHOLE}/seen.txt ${PEEPHOLE}/shown.txt
        COMMAND           ${KEEP} peephole ${NUC} ${PEEPHOLE}/peephole.txt ${PEEPHOLE}/seen.txt ${PEEPHOLE}/shown.txt
                            ${PEEPHOLE}/words.txt ${PEEPHOLE}/typed.txt ${PEEPHOLE}/idle.counts ${PEEPHOLE}/draw.counts
                            ${PEEPHOLE}/draw.work ${PEEPHOLE}/desktop.counts
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT           "The nucleus's compiler, weighed (each step's log is in ${PEEPHOLE})")

# `pairs`: what the nucleus works out two instructions to be together
# (forth/compose.fs), held to the machine's step for every pair it can
# meet and every three that have a name (util/pairs.c, which says at its
# top what it types and what it prints).  It fails if the nucleus was wrong
# of any.  What was typed and what the nucleus said are in build/pairs/.
set(PAIRS ${BUILD}/pairs)
stepping_tool(pair-rules util/pairs.c util/image.c)
report(pairs
        COMMAND           ${CMAKE_COMMAND} -E make_directory ${PAIRS}
        COMMAND           pair-rules type ${NUC} ${TABLES} > ${PAIRS}/typed.fs
        COMMAND           hexaforth ${NUC} < ${PAIRS}/typed.fs > ${PAIRS}/typed.out 2>&1
        COMMAND           pair-rules read ${NUC} ${PAIRS}/typed.out
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT           "What the nucleus works two instructions out to be, held to the step (${PAIRS})")

# `phrases`: the phrase book, what to write to get each instruction
# (util/phrases.c, which says at its top what it finds and asks).  It is
# run where the Forth is, since it has cross.fs compile every phrase; the
# report is build/phrases.txt, and its first part is printed.  LONGEST=4 in
# the environment is phrases of up to four words, which takes much longer.
stepping_tool(phrase-book util/phrases.c util/image.c)
target_compile_definitions(phrase-book PRIVATE GFORTH="${GFORTH}")
add_custom_target(phrases
        COMMAND           ${MAKE_BUILD_DIR}
        COMMAND           ${WITH_HEADERS} $<TARGET_FILE:phrase-book> ${BUILD}/phrases.txt
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/forth)

# What makes forth/folds.fs and forth/fused.fs, which the build of the
# default machine's nucleus runs (above, at build/nuc.hex).
stepping_tool(joins util/joins.c)
stepping_tool(fused util/fused.c util/image.c)
target_compile_definitions(fused PRIVATE GFORTH="${GFORTH}")

#
# The AArch64 kernels, for QEMU's virt machine: baremetal/hexaforth.S, the
# small one, and baremetal/hexaforth-fast.S, decoded by table, which behave
# the same.  A register of the AArch64 is a cell, so they are built for the
# machine of util/machines whose cell is 64 bits: its sizes are what the
# assembler is told, and its nucleus is the image a kernel has in it.
# Everything made is in build/kernel/.
#
#   check       runs each kernel against vm.c, and the desktop on QEMU with
#               each, with the others
#   run         boots KERNEL in QEMU, at its serial console (Ctrl-A X to
#               leave)
#   demo        boots it into the desktop, with a disk, a keyboard and a
#               pointer.  It wants a QEMU with a display; the serial
#               console types into the front window too, and ^Q leaves the
#               desktop for the serial prompt
#   demo-test   drives that desktop with no display and checks its screen,
#               as check does, on the kernel that is said here
#   demo-time   says when the desktop is ready and when its picture is made,
#               with the Mandrill on its disk, its grays, and its grays at
#               half its size
#   size        how big the kernel is
#
# and what those five are of is said when CMake is run, and kept until it
# is said otherwise (cmake -DMIPS=5 -DKERNEL=hexaforth-fast build/cmake):
#
#   KERNEL      which kernel: hexaforth, or hexaforth-fast
#   STRICT      the kernel halts on a fetch or a store outside memory
#   SCALE       each pixel that many across and down, for a screen whose
#               own pixels are small; demo-test then checks as well that
#               every pixel came out as a whole block
#   MIPS        that many million instructions a second and no more,
#               however fast the host is, evenly, so the machine is as slow
#               as one of that speed and as smooth.  It may have a fraction
#   DISK        the disk gives and takes that many thousand bytes a second
#               and no more, as a floppy did: about 40 for an 8-inch one at
#               double density, 23 for a 5 1/4-inch one, half of each at
#               single density.  It is QEMU that holds the drive to it, and
#               it is the rate and no more: no head moves and nothing has
#               to come round.  demo and demo-time take it
#
# Unless they are said, the machine is the one the project is there to
# show: the small kernel, each pixel drawn twice across and down, a million
# instructions a second, and an 8-inch floppy.  MIPS and DISK said as
# nothing hold the machine to no speed (cmake -DMIPS= -DDISK= -DSCALE=1
# build/cmake is the machine as fast as the host makes it).  A value said
# once is kept: cmake -USCALE -UMIPS -UDISK build/cmake has these defaults
# again.  The tests the target `check` runs are not of these: they say
# their own.
#
set(KERNELS_OF 64)
set(KERNEL hexaforth CACHE STRING "which kernel run, demo, demo-test, demo-time and size are of: hexaforth or hexaforth-fast")
option(STRICT "that kernel halts on a fetch or a store outside memory" OFF)
set(SCALE 2 CACHE STRING "how many across and down the bridge draws each pixel of the display")
set(MIPS 1 CACHE STRING "the million instructions a second that kernel is held to; none, if it is said as nothing")
set(DISK 40 CACHE STRING "the thousand bytes a second the disk is held to; none, if it is said as nothing")

if(KERNELS_OF IN_LIST MACHINES)

set(KB ${BUILD}/kernel)
set(OF_KERNEL ${BUILD}/virt)
set(LOAD 0x40080000)                # where QEMU's virt machine has a kernel start
set(KERNEL_SIZES ${SIZES_OF_${KERNELS_OF}})
list(TRANSFORM KERNEL_SIZES PREPEND -D)
set(CROSS_CLANG ${CLANG} --target=aarch64-none-elf -c ${KERNEL_SIZES})

# The machine the kernels are: QEMU's virt, which has the sizes of the
# machine of ${KERNELS_OF} bits and a disk of its own, a virtio block device.  A
# machine's ROM has the driver of the disk it starts from, so this one's
# system is not that machine's: it is made with ROM=virt-disk.fs, in
# build/virt/, as every system is (system).  machine-virt makes its image
# and the disk the desktop is given with it, whose names are that image's,
# and boot-system-virt the image it boots from a disk with and that disk:
# VIRT is the two.  It is made after the machine of ${KERNELS_OF} bits, since all of
# them write what fuse.h keeps in build/.
system(virt ${KERNELS_OF}
       SET ROM=virt-disk.fs
       AFTER machine-${KERNELS_OF} ${BUILD}/${KERNELS_OF}/nuc.hex)
machine_made(virt)
set(VIRT machine-virt boot-system-virt)

# What it is made of: the target examine-virt.  Its ROM has the virtio
# disk's driver in it and is the fullest there is, so this is where how long
# its kernel is, and what its ROM has left, is asked.
examined(virt ${KERNELS_OF})

# A number that may have a fraction, times a whole number, as a whole
# number: CMake's own arithmetic is of whole numbers.
function(times RESULT NUMBER BY)
    if(NOT "${NUMBER}" MATCHES "^([0-9]*)\\.?([0-9]*)$")
        message(FATAL_ERROR "${NUMBER} is not a number")
    endif()
    set(UNITS "${CMAKE_MATCH_1}")
    set(PARTS "${CMAKE_MATCH_2}")
    string(LENGTH "${PARTS}" DIGITS)
    set(OF 1)
    while(DIGITS GREATER 0)
        math(EXPR OF "${OF} * 10")
        math(EXPR DIGITS "${DIGITS} - 1")
    endwhile()
    foreach(WHICH UNITS PARTS)                          # a number that starts with a nothing is not read as tens
        string(REGEX REPLACE "^0+" "" ${WHICH} "${${WHICH}}")
        if("${${WHICH}}" STREQUAL "")
            set(${WHICH} 0)
        endif()
    endforeach()
    math(EXPR IS "${UNITS} * ${BY} + ${PARTS} * ${BY} / ${OF}")
    set(${RESULT} ${IS} PARENT_SCOPE)
endfunction()

# The image a kernel has in it, as bytes; or its ROM alone, which is the
# first 4KB of the image the machine boots from a disk with: what is after
# a kernel is the machine's memory, so a kernel that has only the ROM in
# it starts with nothing else there (doc/boot.md).  And the bridge that
# shows the display on QEMU (baremetal/ramfb.S), which a kernel with a
# display is linked after, since the machine's memory runs on from the end
# of the kernel.  A bridge that draws each pixel more than once across and
# down is another bridge, named for how many.
tool(hex2bin util/hex2bin.c util/image.c)
add_custom_command(
        OUTPUT            ${KB}/image.bin ${KB}/rom.bin
        COMMAND           ${CMAKE_COMMAND} -E make_directory ${KB}
        COMMAND           hex2bin ${OF_KERNEL}/nuc.hex ${KB}/image.bin
        COMMAND           hex2bin --rom ${OF_KERNEL}/boot/nuc.hex ${KB}/rom.bin
        DEPENDS           hex2bin ${VIRT} ${OF_KERNEL}/nuc.hex ${OF_KERNEL}/boot/nuc.hex)
function(bridge SCALED)
    set(BRIDGE ${KB}/ramfb.o)
    set(TOLD)
    if(NOT SCALED EQUAL 1)
        set(BRIDGE ${KB}/ramfb-x${SCALED}.o)
        set(TOLD -DSCALE=${SCALED})
    endif()
    set(BRIDGE ${BRIDGE} PARENT_SCOPE)
    get_property(BRIDGES GLOBAL PROPERTY BRIDGES)
    if(BRIDGE IN_LIST BRIDGES)
        return()
    endif()
    set_property(GLOBAL APPEND PROPERTY BRIDGES ${BRIDGE})
    add_custom_command(
            OUTPUT            ${BRIDGE}
            COMMAND           ${CMAKE_COMMAND} -E make_directory ${KB}
            COMMAND           ${CROSS_CLANG} ${TOLD} ${CMAKE_SOURCE_DIR}/baremetal/ramfb.S -o ${BRIDGE}
            DEPENDS           baremetal/ramfb.S)
endfunction()
bridge(1)
bridge(${SCALE})
get_property(BRIDGES GLOBAL PROPERTY BRIDGES)
add_custom_target(kernel-parts DEPENDS ${KB}/image.bin ${KB}/rom.bin ${BRIDGES})

# A kernel: which of the two, and how it is built.
#
#   DISPLAY     with the bridge, which it calls
#   PROFILE     the same, counting how often each instruction is executed.
#               The counts are kept 4MB past the stacks, which is clear of
#               the pixels only when they are not scaled, so this is always
#               the plain bridge.  (Nothing reads them now: the target
#               `profile` counts on the reference VM.)
#   STRICT      halting on a fetch or a store outside memory
#   MIPS n      held to that many million instructions a second
#   SCALE n     with the bridge that draws each pixel that many times
#   ROM         with only the kernel's ROM in it and not the image: the
#               machine reads the rest from its disk
#
# Each is another file, named for what it is; KERNEL_FILE is its name,
# which the target KERNEL_TARGET makes: NAME.elf, NAME.bin, which is what
# QEMU's machine has in memory, and NAME.symbols, where its symbols are.
function(kernel WHICH)
    cmake_parse_arguments(IS "DISPLAY;PROFILE;STRICT;ROM" "MIPS;SCALE" "" ${ARGN})
    set(NAME ${WHICH})
    set(TOLD)
    set(BRIDGE)
    set(IMAGE ${KB}/image.bin)
    if(IS_ROM)
        string(APPEND NAME -rom)
        set(IMAGE ${KB}/rom.bin)
    endif()
    if(IS_PROFILE)
        string(APPEND NAME -profile)
        list(APPEND TOLD -DBRIDGE -DPROFILE)
        bridge(1)
    elseif(IS_DISPLAY)
        string(APPEND NAME -display)
        list(APPEND TOLD -DBRIDGE)
        if(NOT IS_SCALE)
            set(IS_SCALE 1)
        endif()
        bridge(${IS_SCALE})
    endif()
    if(IS_STRICT)
        string(APPEND NAME -strict)
        list(APPEND TOLD -DSTRICT)
    endif()
    if(IS_MIPS)
        string(APPEND NAME -m${IS_MIPS})
        times(IPS ${IS_MIPS} 1000000)
        list(APPEND TOLD -DIPS=${IPS})
    endif()
    if(IS_DISPLAY AND NOT IS_SCALE EQUAL 1)
        string(APPEND NAME -x${IS_SCALE})
    endif()
    set(KERNEL_FILE ${KB}/${NAME} PARENT_SCOPE)
    set(KERNEL_TARGET kernel-${NAME} PARENT_SCOPE)
    if(TARGET kernel-${NAME})
        return()
    endif()
    set(FILE ${KB}/${NAME})
    add_custom_command(
            OUTPUT            ${FILE}.o
            COMMAND           ${CROSS_CLANG} "-DIMAGE=\"${IMAGE}\"" ${TOLD}
                                ${CMAKE_SOURCE_DIR}/baremetal/${WHICH}.S -o ${FILE}.o
            DEPENDS           baremetal/${WHICH}.S kernel-parts ${IMAGE}
            VERBATIM)
    add_custom_command(
            OUTPUT            ${FILE}.elf ${FILE}.bin ${FILE}.symbols
            COMMAND           ${LLD} -Ttext=${LOAD} -e _start -o ${FILE}.elf ${BRIDGE} ${FILE}.o
            COMMAND           ${OBJCOPY} -O binary ${FILE}.elf ${FILE}.bin
            COMMAND           ${NM} ${FILE}.elf > ${FILE}.symbols
            DEPENDS           ${FILE}.o kernel-parts ${BRIDGE})
    add_custom_target(kernel-${NAME} DEPENDS ${FILE}.elf ${FILE}.bin ${FILE}.symbols)
    add_dependencies(kernel-${NAME} kernel-parts)
endfunction()

# What is built for the kernels' machine and is not the machine's own VM
# stopping where it is strict: as the kernels are built, unless they are
# told to be.
function(kernel_tool TOOL)
    tool(${TOOL} ${ARGN})
    target_compile_definitions(${TOOL} PRIVATE ${SIZES_OF_${KERNELS_OF}})
endfunction()

# A kernel against vm.c, which is the machine: the cases difftest_dump
# writes, each with what vm.c left of it, are run on the kernel under
# Unicorn (baremetal/difftest.c).
kernel_tool(difftest baremetal/difftest.c)
find_library(UNICORN unicorn PATHS ${BREW_PREFIX}/lib)
if(UNICORN_HEADER AND UNICORN)      # without it difftest does not build, and says what it has not
    target_include_directories(difftest PRIVATE ${UNICORN_HEADER})
    target_link_libraries(difftest ${UNICORN})
else()
    left_out(difftest)
endif()
add_custom_command(
        OUTPUT            ${KB}/cases.bin
        COMMAND           ${CMAKE_COMMAND} -E make_directory ${KB}
        COMMAND           difftest_dump ${KB}/cases.bin
        DEPENDS           difftest_dump)

# What drives the desktop on QEMU and looks at its screen (baremetal/qemu.c:
# the checks are the reference VM's too, test/deskcheck.c), what only
# watches it, and what says how big a kernel is.
set(QEMU_SOURCES baremetal/qemu.c test/deskcheck.c test/tables.c test/dither.c bench/tiff.c vm_opcodes.c vm_debug.c)
kernel_tool(guitest baremetal/guitest.c ${QEMU_SOURCES})
kernel_tool(desktime baremetal/desktime.c ${QEMU_SOURCES})
target_compile_definitions(guitest PRIVATE QEMU_AARCH64="${QEMU_AARCH64}")
target_compile_definitions(desktime PRIVATE QEMU_AARCH64="${QEMU_AARCH64}")
tool(kernel_size baremetal/size.c)

# The tests: each kernel against vm.c; the desktop on QEMU with the small
# one, which halts if it leaves memory; and the desktop with each started
# from its ROM alone.  The fast kernel's desktop is checked only so: it is
# the same checks after a boot and a compile as well, and the kernels'
# devices are the same code.  A desktop's screens, and its copy of the
# disk, are kept in a directory named for the kernel.
#
# A kernel has the machine's image in it, so a test of one needs the kernel
# built, which is built again when the image is; kernel-tests is the cases,
# and the directories the screens are kept in.  The desktop on QEMU is
# given the machine's disk as well, which has the names that go with that
# image: a kernel and a disk of two builds do not know each other's names.
function(kernel_test WHICH)
    kernel(${WHICH})
    add_test(NAME kernel-${WHICH}
             COMMAND difftest ${KERNEL_FILE}.bin ${KB}/cases.bin ${KERNEL_FILE}.symbols)
    needs(kernel-${WHICH} kernel-${WHICH} difftest kernel-tests)
endfunction()
function(qemu_test WHICH)
    kernel(${WHICH} DISPLAY ${ARGN})
    get_filename_component(NAME ${KERNEL_FILE} NAME)
    set(ON ${OF_KERNEL}/disk.img)
    set(BOOTS)
    if("ROM" IN_LIST ARGN)              # it starts from the disk, and compiles its desktop from there:
        set(ON ${OF_KERNEL}/boot/desk.img)
        set(BOOTS boot)                 # it is to say what it compiles, and its screen to show it
    endif()
    add_test(NAME qemu-${NAME}
             COMMAND ${CMAKE_COMMAND} -E env --unset=DISK SCALE=1
                     $<TARGET_FILE:guitest> ${KERNEL_FILE}.elf ${ON} ${PICTURES} ${PICTURE} forth
                     ${KB}/screens-${NAME} ${BOOTS}
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(qemu-${NAME} PROPERTIES TIMEOUT ${QEMU_TEST_SECONDS})
    set_property(GLOBAL APPEND PROPERTY SCREENS ${KB}/screens-${NAME})
    needs(qemu-${NAME} kernel-${NAME} guitest kernel-tests ${VIRT} picture pictures)
endfunction()
kernel_test(hexaforth)
kernel_test(hexaforth-fast)
qemu_test(hexaforth STRICT)
qemu_test(hexaforth STRICT ROM)
qemu_test(hexaforth-fast ROM)
get_property(SCREENS GLOBAL PROPERTY SCREENS)
add_custom_target(kernel-tests
        COMMAND           ${CMAKE_COMMAND} -E make_directory ${SCREENS}
        DEPENDS           ${KB}/cases.bin)

# The kernel that is said when CMake is run, and what is done with it.
set(SAID)
if(STRICT)
    list(APPEND SAID STRICT)
endif()
if(MIPS)
    list(APPEND SAID MIPS ${MIPS})
endif()
set(SLOW)
if(DISK)
    times(RATE ${DISK} 1024)
    set(SLOW ",throttling.bps-read=${RATE},throttling.bps-write=${RATE}")
endif()
set(QEMU ${QEMU_AARCH64} -M virt -cpu cortex-a53 -nic none)

kernel(${KERNEL} ${SAID})
add_custom_target(run
        COMMAND           ${QEMU} -nographic -kernel ${KERNEL_FILE}.elf
        USES_TERMINAL)
add_dependencies(run ${KERNEL_TARGET})
add_custom_target(size
        COMMAND           kernel_size ${KERNEL_FILE}.symbols ${KERNEL_FILE}.bin > ${KERNEL_FILE}.size.txt
        COMMAND           ${CMAKE_COMMAND} -E cat ${KERNEL_FILE}.size.txt
        COMMAND           ${KEEP} size ${KERNEL_FILE}.bin ${KERNEL_FILE}.size.txt)
add_dependencies(size ${KERNEL_TARGET})

# The desktop on QEMU with a display: the target NAME boots the kernel last
# said (kernel) with a disk, the pictures' disk, a keyboard and a pointer.
# The disk is its own, a copy of FROM: it is made of that when that is made
# again, and is otherwise as the desktop left it.  The machine's disks are
# its block devices in the order of their places (forth/virt-disk.fs), so
# each is said its place: the disk it is given or starts from in the first,
# and the pictures' disk, which nothing writes to, in the next.
set(SLOW_READ)
if(DISK)
    set(SLOW_READ ",throttling.bps-read=${RATE}")
endif()
function(desktop_shown NAME FROM)                       # and what makes FROM
    add_custom_command(
            OUTPUT            ${KB}/${NAME}.img
            COMMAND           ${CMAKE_COMMAND} -E copy ${FROM} ${KB}/${NAME}.img
            DEPENDS           ${ARGN} ${FROM})
    add_custom_target(${NAME}
            COMMAND           ${QEMU} -m 256 -serial mon:stdio -device ramfb -global virtio-mmio.force-legacy=false
                                -drive "file=${KB}/${NAME}.img,if=none,format=raw,id=hd0${SLOW}"
                                -device virtio-blk-device,drive=hd0,bus=virtio-mmio-bus.0
                                -drive "file=${PICTURES},if=none,format=raw,id=hd1,readonly=on${SLOW_READ}"
                                -device virtio-blk-device,drive=hd1,bus=virtio-mmio-bus.1
                                -device virtio-keyboard-device -device virtio-tablet-device
                                -kernel ${KERNEL_FILE}.elf
            DEPENDS           ${KB}/${NAME}.img
            USES_TERMINAL
            VERBATIM)
    add_dependencies(${NAME} ${KERNEL_TARGET} ${ARGN} pictures)
endfunction()
kernel(${KERNEL} DISPLAY SCALE ${SCALE} ${SAID})
desktop_shown(demo ${OF_KERNEL}/disk.img machine-virt)

# demo-boot is the same machine with nothing in its memory but the kernel's
# ROM: it reads its image from the disk, compiles the applications and the
# desktop from their source there, saying on the screen what it is at, and
# opens the desktop (doc/boot.md).
kernel(${KERNEL} DISPLAY SCALE ${SCALE} ROM ${SAID})
desktop_shown(demo-boot ${OF_KERNEL}/boot/desk.img ${VIRT})

set(STRICTLY)
if(STRICT)
    set(STRICTLY STRICT)
endif()
kernel(${KERNEL} DISPLAY SCALE ${SCALE} ${STRICTLY})
add_custom_target(demo-test
        COMMAND           ${CMAKE_COMMAND} -E make_directory ${KB}/screens
        COMMAND           ${CMAKE_COMMAND} -E env --unset=DISK SCALE=${SCALE}
                            $<TARGET_FILE:guitest> ${KERNEL_FILE}.elf ${OF_KERNEL}/disk.img ${PICTURES} ${PICTURE} forth
                            ${KB}/screens
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_dependencies(demo-test ${KERNEL_TARGET} machine-virt pictures)

set(HELD)
if(MIPS)
    set(HELD MIPS ${MIPS})
endif()
kernel(${KERNEL} DISPLAY ${HELD})
# demo-time runs three times, with a different picture on the disk each
# time: the Mandrill, its grays, and its grays at half size.  The gray
# files are smaller to read, and dither to the same bitmaps as the Mandrill
# at full and half size.  Each run's files are in build/kernel/timed-PIC.
set(TIMES)
set(TIMED_DISKS)
foreach(PIC colour gray small)
    set(PICTURES_OF ${PICTURES})
    set(FROM ${PICTURE})
    if(PIC STREQUAL "gray")
        set(FROM ${GRAYS})
    elseif(PIC STREQUAL "small")
        set(FROM ${SMALL})
    endif()
    if(NOT PIC STREQUAL "colour")           # a pictures' disk with that one on it, under the label the viewer asks for
        set(PICTURES_OF ${KB}/pictures-${PIC}.img)
        pictures_disk(${PICTURES_OF} ${FROM})
    endif()
    list(APPEND TIMES
         COMMAND ${CMAKE_COMMAND} -E make_directory ${KB}/timed-${PIC}
         COMMAND ${CMAKE_COMMAND} -E echo "${PIC}: ${FROM}"
         COMMAND ${CMAKE_COMMAND} -E env DISK=${DISK}
                 $<TARGET_FILE:desktime> ${KERNEL_FILE}.elf ${KERNEL_FILE}.symbols ${OF_KERNEL}/disk.img
                 ${PICTURES_OF} ${FROM} forth ${OF_KERNEL}/nuc.tables ${KB}/timed-${PIC})
    list(APPEND TIMED_DISKS ${PICTURES_OF})
endforeach()
add_custom_target(demo-time ${TIMES}
        DEPENDS           ${TIMED_DISKS}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_dependencies(demo-time ${KERNEL_TARGET} machine-virt pictures)

# The kernel that counts: nothing reads its counts, and it is still built.
kernel(hexaforth PROFILE)

endif()

#
# The Verilog: rtl/hexaforth.v, the CPU; rtl/hf_mem.v, the memory; and
# rtl/soc.v, the two together.  Verilator makes a program of each with its
# test bench: tb (rtl/tb.cpp), the CPU with a plain memory; tb_soc, the
# whole system with the nucleus in its memory; and tb_mem, the memory by
# itself.  Everything made is in build/rtl/, with a directory for each way
# the CPU is built (c64, c32-lat2, c64-lat2-krom).
#
# A way it is built is the bits of its cell; LAT, the cycles after it is
# asked for that what the memory read is back, 1 or 2; and whether the
# kernel is in a ROM.  Its memory is what RTL_MEMORY says, or as much of
# that as a cell of 16 bits reaches.  The program
# counter is 16 bits, which is all an execution token holds.
#
# The nucleus the tests run is the one for the machine vm_constants.h
# gives a cell of that size when nothing else is said (default-32), and
# what the Verilog does is compared with what vm.c does of that machine.
#
#   check       on each way: the cases difftest_dump writes, each with what
#               vm.c left of it, run on the CPU; and a session typed at the
#               nucleus on vm.c, on the CPU and on the whole system, which
#               are to send the same (test/session_test.c).  And the
#               memory, against a model of it
#   synth       synthesises the system for an ECP5 FPGA, and says how big
#               it is and how fast its clock can go.  It wants yosys and
#               nextpnr-ecp5; SYNTH_CELL, SYNTH_ADDR (the log2 of memory's
#               cells: as much as a CPU is tested with if it is not said),
#               SYNTH_LAT and SYNTH_KROM
#               say what of
#
set(RB ${BUILD}/rtl)
set(RTL ${CMAKE_SOURCE_DIR}/rtl)
# The log2 of the bytes of memory a CPU is tested with: 256KB, which the
# machines of 64 and 32 bits are to fit in.  Their display starts at byte
# $20000 and what is given out is after it, and the machine of 64 bits is
# given some as it starts (the screen's port).  With 128KB that was a store
# outside memory, to a cell that happened to be free.
set(RTL_MEMORY 18)
set(RTL_PC 16)
set(VERILATED ${VERILATOR} -Wall -Wno-UNUSED -Wno-DECLFILENAME -Wno-PINCONNECTEMPTY -Wno-WIDTH -O3
                        --x-assign fast --x-initial fast)

# The log2 of the cells that so much memory is, of a cell of some size, and
# no more than an address, which is a cell, can reach.
function(cells_of RESULT BYTES_LOG2 CELL)
    math(EXPR BYTES "${CELL} / 8")
    while(BYTES GREATER 1)
        math(EXPR BYTES "${BYTES} / 2")
        math(EXPR BYTES_LOG2 "${BYTES_LOG2} - 1")
    endwhile()
    if(BYTES_LOG2 GREATER CELL)
        set(BYTES_LOG2 ${CELL})
    endif()
    set(${RESULT} ${BYTES_LOG2} PARENT_SCOPE)
endfunction()

# What makes an image into what the Verilog's memory is loaded from, and
# the kernel's ROM (util/memimage.c): memimage IMAGE.hex OUTDIR ADDR_BITS,
# the cell being the image's machine's and the memory as much as is said.
tool(memimage util/memimage.c util/image.c)

# The cases the kernels and the Verilog are run against, each with what
# vm.c left of it (baremetal/difftest_dump.c): difftest_dump CASES.bin
# [CELL], of a machine of that cell.
tool(difftest_dump baremetal/difftest_dump.c ${VM_SOURCES} util/image.c test/compiler.c test/vm_test.c)

# The CPU with a plain memory and its test bench, in a directory (tb): of
# a cell, the log2 of its memory's cells, and LAT.
function(verilog_cpu B CELL ADDR LAT)
    add_custom_command(
            OUTPUT            ${B}/tb
            COMMAND           ${CMAKE_COMMAND} -E make_directory ${B}
            COMMAND           ${VERILATED} -CFLAGS "-DW=${CELL} -DAB=${ADDR} -DLAT=${LAT}"
                                --cc hexaforth.v -GW=${CELL} -GPCW=${RTL_PC} -GLAT=${LAT}
                                --exe ${RTL}/tb.cpp --Mdir ${B}/cpu -o ../tb --build -j 4 > ${B}/tb.log 2>&1
            WORKING_DIRECTORY ${RTL}
            COMMENT           "The Verilog CPU in ${B} (what Verilator says is in its tb.log)"
            DEPENDS           rtl/hexaforth.v rtl/tb.cpp
            VERBATIM)
endfunction()

# What is the same for every way a CPU of one cell is built: the machine,
# the cases, and what vm.c sent when the session was typed at it.
function(verilog_cell CELL)
    set(M default-${CELL})
    machine(${M} CELL_BITS=${CELL})
    machine_made(${M})
    add_custom_command(
            OUTPUT            ${RB}/cases-${CELL}.bin
            COMMAND           ${CMAKE_COMMAND} -E make_directory ${RB}
            COMMAND           difftest_dump ${RB}/cases-${CELL}.bin ${CELL}
            DEPENDS           difftest_dump)
    add_custom_command(
            OUTPUT            ${RB}/session-${CELL}.out
            COMMAND           ${CMAKE_COMMAND} -E make_directory ${RB}
            COMMAND           hexaforth ${BUILD}/${M}/nuc.hex < ${RTL}/session.fs > ${RB}/session-${CELL}.out
            DEPENDS           hexaforth machine-${M} ${BUILD}/${M}/nuc.hex rtl/session.fs)
    add_custom_target(verilog-${CELL} DEPENDS ${RB}/cases-${CELL}.bin ${RB}/session-${CELL}.out)
    add_dependencies(verilog-${CELL} machine-${M})
endfunction()

# A way the CPU is built, and its tests.
function(verilog CELL LAT)                              # and KROM, if the kernel is in a ROM
    set(NAME c${CELL})
    if(NOT LAT EQUAL 1)
        string(APPEND NAME -lat${LAT})
    endif()
    set(KROM)
    if("${ARGN}" STREQUAL "KROM")
        string(APPEND NAME -krom)
        set(KROM +define+HF_KROM -I${RB}/${NAME})
    endif()
    set(B ${RB}/${NAME})
    set(M default-${CELL})
    set(NUC ${BUILD}/${M}/nuc.hex)
    cells_of(ADDR ${RTL_MEMORY} ${CELL})
    set(TOLD -CFLAGS "-DW=${CELL} -DAB=${ADDR} -DLAT=${LAT}")

    verilog_cpu(${B} ${CELL} ${ADDR} ${LAT})
    add_custom_command(
            OUTPUT            ${B}/tb_soc ${B}/mem.hex
            COMMAND           ${CMAKE_COMMAND} -E make_directory ${B}
            COMMAND           memimage ${NUC} ${B} ${ADDR}
            COMMAND           ${VERILATED} ${TOLD} --cc soc.v hexaforth.v hf_mem.v --top-module soc
                                -GW=${CELL} -GAB=${ADDR} -GLAT=${LAT} "-GINIT=\"${B}/mem.hex\"" ${KROM}
                                --exe ${RTL}/tb_soc.cpp --Mdir ${B}/soc -o ../tb_soc --build -j 4 > ${B}/tb_soc.log 2>&1
            WORKING_DIRECTORY ${RTL}
            COMMENT           "The Verilog system ${NAME} (what Verilator says is in ${B}/tb_soc.log)"
            DEPENDS           rtl/soc.v rtl/hexaforth.v rtl/hf_mem.v rtl/tb_soc.cpp
                              memimage verilog-${CELL} ${NUC}
            VERBATIM)
    add_custom_command(
            OUTPUT            ${B}/cpu.out ${B}/cpu.err
            COMMAND           ${B}/tb run ${NUC} < ${RTL}/session.fs > ${B}/cpu.out 2> ${B}/cpu.err
            DEPENDS           ${B}/tb verilog-${CELL} ${NUC} rtl/session.fs)
    # The system reads its memory from mem.hex when it is run, so what it
    # sent is of that image as much as of the program: Verilator leaves a
    # program it has no need to make again as old as it was.
    add_custom_command(
            OUTPUT            ${B}/soc.out ${B}/soc.err
            COMMAND           ${B}/tb_soc < ${RTL}/session.fs > ${B}/soc.out 2> ${B}/soc.err
            DEPENDS           ${B}/tb_soc ${B}/mem.hex rtl/session.fs)
    add_custom_target(verilog-${NAME} DEPENDS ${B}/tb ${B}/tb_soc ${B}/cpu.out ${B}/soc.out)
    add_dependencies(verilog-${NAME} verilog-${CELL})

    # What a way is tested with is all the target verilog-NAME's: the CPU
    # and the system, the cases, and what each sent of the session.
    add_test(NAME verilog-cases-${NAME} COMMAND ${B}/tb cases ${RB}/cases-${CELL}.bin)
    needs(verilog-cases-${NAME} verilog-${NAME})
    add_test(NAME verilog-session-${NAME}
             COMMAND session_test ${RB}/session-${CELL}.out ${B}/cpu.out ${B}/soc.out ${B}/cpu.err)
    needs(verilog-session-${NAME} verilog-${NAME} session_test)
endfunction()

tool(session_test test/session_test.c)
foreach(CELL 64 32 16)
    verilog_cell(${CELL})
    verilog(${CELL} 1)
    verilog(${CELL} 2)
endforeach()
verilog(64 2 KROM)

# The memory by itself, against a model of it (rtl/tb_mem.cpp).
cells_of(ADDR ${RTL_MEMORY} 64)
add_custom_command(
        OUTPUT            ${RB}/c64/tb_mem
        COMMAND           ${CMAKE_COMMAND} -E make_directory ${RB}/c64
        COMMAND           ${VERILATED} -CFLAGS "-DW=64 -DAB=${ADDR} -DLAT=1" --cc hf_mem.v
                            --exe ${RTL}/tb_mem.cpp --Mdir ${RB}/c64/mem -o ../tb_mem --build -j 4 > ${RB}/c64/tb_mem.log 2>&1
        WORKING_DIRECTORY ${RTL}
        COMMENT           "The Verilog memory (what Verilator says is in ${RB}/c64/tb_mem.log)"
        DEPENDS           rtl/hf_mem.v rtl/tb_mem.cpp
        VERBATIM)
add_custom_target(verilog-memory DEPENDS ${RB}/c64/tb_mem)
add_test(NAME verilog-memory COMMAND ${RB}/c64/tb_mem)
needs(verilog-memory verilog-memory)

# Synthesis.
set(SYNTH_CELL 64 CACHE STRING "the bits of a cell of the system `synth` synthesises")
set(SYNTH_ADDR "" CACHE STRING "the log2 of the cells of its memory: as much as a CPU is tested with, if it is not said")
set(SYNTH_LAT 1 CACHE STRING "the cycles after it is asked for that what its memory read is back")
option(SYNTH_KROM "its kernel is in a ROM" OFF)
set(SYNTH_AB ${SYNTH_ADDR})
if(NOT SYNTH_AB)
    cells_of(SYNTH_AB ${RTL_MEMORY} ${SYNTH_CELL})
endif()
set(S ${RB}/synth-c${SYNTH_CELL}-ab${SYNTH_AB})
if(NOT SYNTH_LAT EQUAL 1)
    string(APPEND S -lat${SYNTH_LAT})
endif()
set(SYNTH_ROM)
set(SYNTH_READ read_verilog)
set(SYNTH_IS 0)
if(SYNTH_KROM)
    string(APPEND S -krom)
    set(SYNTH_ROM COMMAND memimage ${BUILD}/default-${SYNTH_CELL}/nuc.hex ${S} ${SYNTH_AB})
    set(SYNTH_READ read_verilog -DHF_KROM -I${S})
    set(SYNTH_IS 1)
endif()
string(REPLACE ";" " " SYNTH_READ "${SYNTH_READ}")

# nextpnr-ecp5, where it is not here and no package has it, as Homebrew has
# none: nextpnr's source, fetched as it was at the release Homebrew builds
# its nextpnr-ice40 from (0.11.1), and built for the one ECP5 that `synth`
# places for.  It is built when `synth` is first asked for, in
# build/nextpnr/, and that takes a while; what it says is kept there.
#
# The chip's database is made from Project Trellis's, by a Python program
# that has Trellis's own module in it (pytrellis).  That module is built
# for one Python and no other, so the Python is found by asking each that
# is beside Trellis whether it can have it.
set(NEXTPNR_BUILT)
if(NOT NEXTPNR AND TRELLIS AND BOOST_HEADER AND EIGEN_HEADER)
    get_filename_component(TRELLIS_AT ${TRELLIS} DIRECTORY)
    get_filename_component(TRELLIS_AT ${TRELLIS_AT} DIRECTORY)
    file(GLOB PYTHONS ${TRELLIS_AT}/bin/python3.*)
    list(FILTER PYTHONS INCLUDE REGEX "/python3\\.[0-9]+$")
    list(SORT PYTHONS COMPARE NATURAL ORDER DESCENDING)
    set(TRELLIS_PYTHON)
    foreach(PYTHON ${PYTHONS})
        execute_process(COMMAND ${PYTHON} -c "import pytrellis"
                        WORKING_DIRECTORY ${TRELLIS_AT}/lib/trellis
                        RESULT_VARIABLE FAILED OUTPUT_QUIET ERROR_QUIET)
        if(NOT FAILED)
            set(TRELLIS_PYTHON ${PYTHON})
            break()
        endif()
    endforeach()
    if(TRELLIS_PYTHON)
        include(ExternalProject)
        set(NB ${BUILD}/nextpnr)
        ExternalProject_Add(nextpnr
                GIT_REPOSITORY    https://github.com/YosysHQ/nextpnr
                GIT_TAG           62e659ed6748e9f60b2a4aa25dd4fe79a9605881
                GIT_SUBMODULES    ""
                SOURCE_DIR        ${NB}/source
                BINARY_DIR        ${NB}/made
                CMAKE_ARGS        -DARCH=ecp5 -DECP5_DEVICES=85k -DBUILD_PYTHON=OFF
                                  -DTRELLIS_INSTALL_PREFIX=${TRELLIS_AT} -DCMAKE_PREFIX_PATH=${TRELLIS_AT}
                                  -DPython3_EXECUTABLE=${TRELLIS_PYTHON}
                BUILD_COMMAND     ${CMAKE_COMMAND} --build <BINARY_DIR> --target nextpnr-ecp5 -j 4
                INSTALL_COMMAND   ""
                LOG_DIR           ${NB}
                LOG_CONFIGURE     TRUE
                LOG_BUILD         TRUE
                LOG_OUTPUT_ON_FAILURE TRUE
                EXCLUDE_FROM_ALL  TRUE)
        set(NEXTPNR ${NB}/made/nextpnr-ecp5)
        set(NEXTPNR_BUILT nextpnr)
    else()
        message(WARNING "nextpnr-ecp5 cannot be built here: no Python in ${TRELLIS_AT}/bin has Project Trellis's module, which is in ${TRELLIS_AT}/lib/trellis.")
    endif()
endif()

tool(synth_said rtl/synth.c)
add_custom_target(synth
        COMMAND           ${CMAKE_COMMAND} -E make_directory ${S}
        ${SYNTH_ROM}
        COMMAND           ${YOSYS} -q -l ${S}/yosys.log -p
                            "${SYNTH_READ} soc.v hexaforth.v hf_mem.v; chparam -set W ${SYNTH_CELL} -set AB ${SYNTH_AB} -set LAT ${SYNTH_LAT} soc; synth_ecp5 -top soc -json ${S}/soc.json"
        COMMAND           ${NEXTPNR} --85k --package CABGA381 --json ${S}/soc.json --lpf-allow-unconstrained
                            --timing-allow-fail -q -l ${S}/nextpnr.log || ${CMAKE_COMMAND} -E true
        COMMAND           synth_said ${S}/nextpnr.log
                            "W=${SYNTH_CELL} LAT=${SYNTH_LAT} KROM=${SYNTH_IS} memory 2^${SYNTH_AB} cells:"
                            > ${S}/result.txt
        COMMAND           ${CMAKE_COMMAND} -E cat ${S}/result.txt
        COMMAND           ${KEEP} synth ${RTL}/hexaforth.v ${S}/result.txt ${S}/nextpnr.log
        WORKING_DIRECTORY ${RTL}
        VERBATIM)
add_dependencies(synth machine-default-${SYNTH_CELL} ${NEXTPNR_BUILT})

#
# The benchmarks: what the machine costs to do its work, in clocks.
#
# Runtime is clocks, and the Verilog CPU is what has them (rtl/tb.cpp), so
# the work is done there: on each machine of util/machines, with as much
# memory as it has, and with what the memory read back a cycle after it is
# asked for and two (LAT).  A piece of work is a file that is typed at the
# nucleus's prompt, the same file on every machine:
#
#   bench-draw      bench/draw.fs: text, fills and scrolls.  What it leaves
#                   on the display is compared with what the reference VM
#                   leaves there when the same is typed at it
#   bench-dither    bench/dither.fs: the Mandrill read from a disk a block
#                   at a time, as the system reads it, and dithered, from
#                   its three samples and from its grays, at its own size
#                   and at half of it, and from its half-size grays at their
#                   own size and at twice it.  Each bitmap is compared with the
#                   dither a pixel at a time (test/dither.c).  A machine
#                   that has not the memory for one says so, and that is
#                   what is printed
#   bench-gray      bench/gray.fs: keeping the Mandrill as gray in sixteen
#                   levels, one sample per 4 by 4 pixels (forth/gray.fs);
#                   loading the blue noise mask from the disk; drawing the
#                   whole picture through it at half size; and copying the
#                   bitmap to the display (copy-bits): a figure for each.
#                   The bitmap, and the display where it was copied, are
#                   compared with the same done a pixel at a time
#                   (test/gray.h)
#
# and two things are there to compare the machine with:
#
#   bench-m68k      the same dither on a 68000, written for it
#                   (bench/m68k/), with the picture already in its memory,
#                   and its bitmap compared with the same dither a pixel at
#                   a time.  It wants the 68000's assembler (m68k-elf-as),
#                   and fetches Musashi, which is the 68000
#   gates           how big the CPU is beside a Z80 and a 68000
#
#   bench           the first four
#
# A figure is printed by what checked the work it is of (bench/check.c),
# and nothing is printed for work that was not checked.  Everything is
# kept in build/bench/; a line a result is printed, and the target fails
# if a check did.  No figure is kept in the repository: what a change costs
# is what this prints before it and after.
#
set(BB ${BUILD}/bench)
set(BENCH ${CMAKE_SOURCE_DIR}/bench)

# The benchmarks' disks, each no more than a floppy: the system's, which
# the machine has as its first; the pictures' disk, which has the Mandrill;
# and a disk of the Mandrill's grays and its grays at half size, as
# one-sample TIFFs (bench/grays.c writes the two files).  A machine is given
# the three, and bench/pictures.fs, typed at it, finds each picture by its
# label.  It says a label with parse-name, which the nucleus has: the
# system with the picture alone has no s" (macros.fs).
tool(picture-grays bench/grays.c bench/tiff.c test/dither.c)
add_custom_command(
        OUTPUT            ${GRAYS} ${SMALL}
        COMMAND           ${CMAKE_COMMAND} -E make_directory ${BB}
        COMMAND           picture-grays ${PICTURE} ${GRAYS} ${SMALL} > ${BB}/grays.log 2>&1
        DEPENDS           picture-grays picture ${PICTURE})
add_custom_command(
        OUTPUT            ${BB}/system.img
        COMMAND           ${CMAKE_COMMAND} -E make_directory ${BB}
        COMMAND           mkdisk ${BB}/system.img > ${BB}/system-disk.log 2>&1
        DEPENDS           mkdisk)
add_custom_command(
        OUTPUT            ${BB}/grays.img
        COMMAND           mkdisk --disk ${BB}/grays.img grays.tiff=${GRAYS} small.tiff=${SMALL}
                            > ${BB}/grays-disk.log 2>&1
        DEPENDS           mkdisk ${GRAYS} ${SMALL})
set(BENCH_DISK_FILES ${BB}/system.img ${PICTURES} ${BB}/grays.img)
set(BENCH_DISKS "${BB}/system.img,${PICTURES},${BB}/grays.img")    # as a machine is told its disks

# How often each cell of the dithered picture occurs, cut into cells of 16,
# 32 and 64 pixels (bench/patterns.c): the target cell-patterns prints it
# for the picture at its own size and at half, and keeps it in
# build/bench/patterns-own.txt and patterns-half.txt.
tool(patterns bench/patterns.c bench/tiff.c test/dither.c)
add_custom_target(cell-patterns
        COMMAND           ${CMAKE_COMMAND} -E make_directory ${BB}
        COMMAND           patterns ${PICTURE} own > ${BB}/patterns-own.txt
        COMMAND           patterns ${PICTURE} half > ${BB}/patterns-half.txt
        COMMAND           ${CMAKE_COMMAND} -E cat ${BB}/patterns-own.txt ${BB}/patterns-half.txt
        DEPENDS           patterns picture ${PICTURE})

# Three ways a picture could be stored are tried on the Mandrill, Peppers
# and the F-16, each by a tool of bench/ named as its target is: the target
# NAME-report prints what the tool says of each picture, at each size where
# it takes one, and each is left as pictures in build/bench/NAME/, in a
# directory named for the picture and the size.
#
#   tiles-report    as a codebook of tiles and a tile's number for each
#                   place, drawn as a font is (bench/tiles.c): how small,
#                   and how near the dither, at its own size and at half
#   masks-report    kept as gray, in few bits and at less than its size,
#                   and drawn black and white through a threshold mask
#                   (bench/masks.c): how small, at the same two sizes
#   quads-report    one byte per 2 by 2 block, expanded back to full size
#                   and dithered (bench/quads.c): each way's size, and how
#                   far it is from the picture's grays
function(pictures_report NAME)                          # and the sizes its tool takes, if it takes one
    tool(${NAME} bench/${NAME}.c bench/tiff.c test/dither.c)
    set(EACH ${ARGN})
    if(NOT EACH)
        set(EACH whole)
    endif()
    set(SAID)
    foreach(SOURCE ${PICTURE} ${PEPPERS} ${AIRPLANE})
        get_filename_component(PIC ${SOURCE} NAME_WE)
        foreach(SIZE ${EACH})
            set(AT ${BB}/${NAME}/${PIC})
            set(OF)
            if(ARGN)
                set(AT ${AT}-${SIZE})
                set(OF ${SIZE})
            endif()
            add_custom_command(
                    OUTPUT            ${AT}.txt
                    COMMAND           ${CMAKE_COMMAND} -E make_directory ${AT}
                    COMMAND           ${NAME} ${SOURCE} ${OF} ${AT} > ${AT}.txt
                    DEPENDS           ${NAME} ${SOURCE})
            list(APPEND SAID ${AT}.txt)
        endforeach()
    endforeach()
    add_custom_target(${NAME}-report
            COMMAND           ${CMAKE_COMMAND} -E cat ${SAID}
            DEPENDS           ${SAID})
endfunction()
pictures_report(tiles own half)
pictures_report(masks own half)
pictures_report(quads)

# The dithers that are timed, each as PICTURE:SIZE:CHECKED.  PICTURE is
# one of the pictures in blocks.fs, SIZE is the size it is dithered at, and
# CHECKED is which dither of the Mandrill the bitmap must match.  The small
# grays are the Mandrill at half size: dithered at their own size they
# match the Mandrill dithered at half, and dithered at twice their size,
# with each gray as a 2 by 2, they match "doubled" (see bench/check.c).
# asks-PIC-SIZE.fs is what is typed after bench/dither.fs to run one: it
# dithers, prints where the bitmap is, and stops.  dither-costs counts the
# Mandrill and its grays at half size (COSTED_PICTURES).
set(COSTED_PICTURES gray colour)
set(DITHERS gray:own:own gray:half:half colour:own:own colour:half:half small:own:half small:twice:doubled)
foreach(DITHER ${DITHERS})
    string(REPLACE ":" ";" DITHER ${DITHER})
    list(GET DITHER 0 PIC)
    list(GET DITHER 1 SIZE)
    file(WRITE ${BB}/asks-${PIC}-${SIZE}.fs "mark ${PIC}-picture ${SIZE} mark\nb @ . b 1+ @ .\nbye\n")
endforeach()

# What is typed after bench/gray.fs: keep the colour picture as gray, one
# sample per 1 << GRAY_KEEP pixels a side; load the GRAY_MASK by GRAY_MASK
# blue noise mask; draw the whole picture through it, each sample 1 <<
# GRAY_DRAW pixels a side; copy the bitmap to the display at GRAY_AT pixels
# across at the top, as a window shows it (its content is a pixel in from
# the frame, so rarely on a cell boundary); print where the bitmap is; and
# stop.  This is the viewer's work on the 16-bit machine, which lacks the
# memory to draw the gray at full size.  check is given the same numbers.
set(GRAY_KEEP 2)
set(GRAY_DRAW 1)
set(GRAY_MASK 64)
set(GRAY_AT 1)
file(WRITE ${BB}/asks-gray.fs
     "display-init\n"
     "mark colour-picture ${GRAY_KEEP} keep mark 0 mask-blue${GRAY_MASK} m ! mark ${GRAY_DRAW} draw mark "
     "${GRAY_DRAW} ${GRAY_AT} show mark\n"
     "b @ . b 1+ @ .\nbye\n")

# The work on a machine: its nucleus, the reference VM, the Verilog CPU
# with as much memory, and what checks a bitmap of its kind (bench_check).
machine_tool(bench_check bench/check.c bench/tiff.c test/dither.c)
function(bench_on NAME)
    string(REGEX MATCH "CELL_BITS=([0-9]+)" HAS "${SIZES_OF_${NAME}}")
    set(CELL ${CMAKE_MATCH_1})
    string(REGEX MATCH "ADDR_BITS=([0-9]+)" HAS "${SIZES_OF_${NAME}}")
    set(ADDR ${CMAKE_MATCH_1})
    set(NUC ${BUILD}/${NAME}/nuc.hex)
    foreach(LAT 1 2)
        set(T ${BB}/tb-${NAME}-${LAT})
        verilog_cpu(${T} ${CELL} ${ADDR} ${LAT})
        set_property(GLOBAL APPEND PROPERTY BENCH_PARTS ${T}/tb)
        set(ON "${CELL} bits LAT=${LAT}")
        set(WANTS bench-parts ${T}/tb ${BENCH_DISK_FILES} machine-${NAME} ${NUC} bench_check)

        set(J ${BB}/draw-${NAME}-${LAT})
        add_custom_command(
                OUTPUT            ${J}.txt
                COMMAND           ${T}/tb run ${NUC} ${BENCH_DISKS} ${J}.mem < ${BENCH}/draw.fs > ${J}.out 2> ${J}.err
                COMMAND           hexaforth ${NUC} ${BENCH_DISKS} ${J}.vm < ${BENCH}/draw.fs > ${J}.vm.out 2>&1
                COMMAND           bench_check ${NUC} display ${J}.vm ${J}.mem ${J}.err draw "${ON}" text fills scrolls
                                    > ${J}.txt 2> ${J}.check
                COMMAND           ${CMAKE_COMMAND} -E rm -f ${J}.mem ${J}.vm
                DEPENDS           ${WANTS} hexaforth bench/draw.fs
                VERBATIM)
        set_property(GLOBAL APPEND PROPERTY DRAW_FIGURES ${J}.txt)
        set_property(GLOBAL APPEND PROPERTY DRAW_KEPT
                     COMMAND ${KEEP} draw-${NAME}-${LAT} ${NUC} ${J}.txt ${BENCH}/draw.fs)

        foreach(DITHER ${DITHERS})
            string(REPLACE ":" ";" DITHER ${DITHER})
            list(GET DITHER 0 PIC)
            list(GET DITHER 1 SIZE)
            list(GET DITHER 2 MADE)
            set(J ${BB}/dither-${PIC}-${SIZE}-${NAME}-${LAT})
            # The machine of 64K cells, with the whole desktop, has not the
            # memory for a bitmap of the Mandrill's size: that is dithered
            # on its system with the picture alone (forth/picture.fs), and
            # its line says so.
            set(SYSTEM_IMAGE ${NUC})
            set(SAYS "${ON}")
            set(SYSTEM_WANTS)
            if(NOT MADE STREQUAL "half" AND ADDR EQUAL 16)
                set(SYSTEM_IMAGE ${BUILD}/${NAME}/picture/nuc.hex)
                set(SAYS "${ON}, picture.fs")
                set(SYSTEM_WANTS picture-system-${NAME} ${SYSTEM_IMAGE})
            endif()
            add_custom_command(
                    OUTPUT            ${J}.txt
                    COMMAND           ${CMAKE_COMMAND} -E cat ${BENCH}/tb-disk.fs ${BENCH}/pictures.fs ${BENCH}/dither.fs
                                        ${BB}/asks-${PIC}-${SIZE}.fs > ${J}.fs
                    COMMAND           ${T}/tb run ${SYSTEM_IMAGE} ${BENCH_DISKS} ${J}.mem < ${J}.fs > ${J}.out 2> ${J}.err
                    COMMAND           bench_check ${SYSTEM_IMAGE} picture ${PICTURE} ${MADE} ${J}.mem ${J}.out ${J}.err
                                        "dither ${PIC} ${SIZE}" "${SAYS}" > ${J}.txt 2> ${J}.check
                    COMMAND           ${CMAKE_COMMAND} -E rm -f ${J}.mem
                    DEPENDS           ${WANTS} ${SYSTEM_WANTS} bench/pictures.fs bench/tb-disk.fs bench/dither.fs
                                        picture ${PICTURE}
                    VERBATIM)
            set_property(GLOBAL APPEND PROPERTY DITHER_FIGURES ${J}.txt)
            set_property(GLOBAL APPEND PROPERTY DITHER_KEPT
                         COMMAND ${KEEP} dither-${PIC}-${SIZE}-${NAME}-${LAT} ${SYSTEM_IMAGE} ${J}.txt ${J}.fs)
        endforeach()

        set(J ${BB}/gray-${NAME}-${LAT})
        add_custom_command(
                OUTPUT            ${J}.txt
                COMMAND           ${CMAKE_COMMAND} -E cat ${BENCH}/tb-disk.fs ${BENCH}/pictures.fs ${BENCH}/gray.fs
                                    ${BB}/asks-gray.fs > ${J}.fs
                COMMAND           ${T}/tb run ${NUC} ${BENCH_DISKS} ${J}.mem < ${J}.fs > ${J}.out 2> ${J}.err
                COMMAND           bench_check ${NUC} gray ${PICTURE} ${GRAY_KEEP} ${GRAY_DRAW} ${GRAY_MASK} ${GRAY_AT}
                                    ${J}.mem ${J}.out ${J}.err "gray colour" "${ON}" keep mask draw copy
                                    > ${J}.txt 2> ${J}.check
                COMMAND           ${CMAKE_COMMAND} -E rm -f ${J}.mem
                DEPENDS           ${WANTS} bench/pictures.fs bench/tb-disk.fs bench/gray.fs picture ${PICTURE}
                VERBATIM)
        set_property(GLOBAL APPEND PROPERTY GRAY_FIGURES ${J}.txt)
        set_property(GLOBAL APPEND PROPERTY GRAY_KEPT
                     COMMAND ${KEEP} gray-${NAME}-${LAT} ${NUC} ${J}.txt ${J}.fs)
    endforeach()

    # Where the dither's clocks go (dither-costs-NAME): the same work, at
    # half the picture's size, typed at the machine's counting VM with the
    # VM's own disk, and then what each word costs with all it calls, its
    # loops and its variables (bench/costs.c, in clocks by bench/cycles.h),
    # and the fewest instructions found that do a turn of each loop
    # (bench/shorter.c, in shorter.txt).  What is kept is in
    # build/bench/costs-NAME-PIC/, what the machine printed in typed.out
    # there.
    set(WORKS)
    set(COSTED)
    foreach(PIC ${COSTED_PICTURES})
        set(K ${BB}/costs-${NAME}-${PIC})
        add_custom_command(
                OUTPUT            ${K}/work
                COMMAND           ${CMAKE_COMMAND} -E make_directory ${K}
                COMMAND           ${CMAKE_COMMAND} -E cat ${BENCH}/pictures.fs ${BENCH}/dither.fs
                                    ${BB}/asks-${PIC}-half.fs > ${K}/typed.fs
                COMMAND           ${CMAKE_COMMAND} -E env RUN_WORK=${K}/work
                                    $<TARGET_FILE:counting> ${NUC} ${BENCH_DISKS} < ${K}/typed.fs > ${K}/typed.out 2>&1
                DEPENDS           counting machine-${NAME} ${NUC} ${BENCH_DISK_FILES} bench/pictures.fs
                                  bench/dither.fs picture ${PICTURE}
                VERBATIM)
        list(APPEND WORKS ${K}/work)
        list(APPEND COSTED COMMAND ${CMAKE_COMMAND} -E echo "dither ${PIC} half, cells of ${CELL} bits:"
                           COMMAND costs ${NUC} ${BUILD}/${NAME}/nuc.tables ${BUILD}/${NAME}/nuc.sizes ${K}/work ${K}
                           COMMAND shorter ${NUC} ${BUILD}/${NAME}/nuc.tables ${K}/work ${K}/shorter.txt
                           COMMAND ${KEEP} dither-costs-${PIC} ${NUC} ${K}/work ${K}/typed.fs ${K}/paths.txt
                                     ${K}/loops.txt ${K}/vars.txt ${K}/shorter.txt)
    endforeach()
    add_custom_target(dither-costs-${NAME} ${COSTED} DEPENDS ${WORKS} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_dependencies(dither-costs-${NAME} costs shorter keep)

    # The same for the gray (gray-costs-NAME): bench/gray.fs's work typed at
    # the counting VM, then the cost of each word, loop and variable, in
    # build/bench/gray-costs-NAME/.
    set(K ${BB}/gray-costs-${NAME})
    add_custom_command(
            OUTPUT            ${K}/work
            COMMAND           ${CMAKE_COMMAND} -E make_directory ${K}
            COMMAND           ${CMAKE_COMMAND} -E cat ${BENCH}/pictures.fs ${BENCH}/gray.fs
                                ${BB}/asks-gray.fs > ${K}/typed.fs
            COMMAND           ${CMAKE_COMMAND} -E env RUN_WORK=${K}/work
                                $<TARGET_FILE:counting> ${NUC} ${BENCH_DISKS} < ${K}/typed.fs > ${K}/typed.out 2>&1
            DEPENDS           counting machine-${NAME} ${NUC} ${BENCH_DISK_FILES} bench/pictures.fs
                              bench/gray.fs picture ${PICTURE}
            VERBATIM)
    add_custom_target(gray-costs-${NAME}
            COMMAND           ${CMAKE_COMMAND} -E echo "the gray, cells of ${CELL} bits:"
            COMMAND           costs ${NUC} ${BUILD}/${NAME}/nuc.tables ${BUILD}/${NAME}/nuc.sizes ${K}/work ${K}
            COMMAND           ${KEEP} gray-costs ${NUC} ${K}/work ${K}/typed.fs ${K}/paths.txt ${K}/loops.txt ${K}/vars.txt
            DEPENDS           ${K}/work
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_dependencies(gray-costs-${NAME} costs keep)
endfunction()
foreach(NAME ${MACHINES})
    bench_on(${NAME})
endforeach()
get_property(BENCH_PARTS GLOBAL PROPERTY BENCH_PARTS)
add_custom_target(bench-parts DEPENDS ${BENCH_DISK_FILES} ${BENCH_PARTS})

# The 68000.  Musashi is fetched as it was when this was written; the
# program is where it is put in the 68000's memory.
set(MB ${BB}/m68k)
set(M68K_AT 0x1000)
set(M68K_FIGURES)
if(M68K_AS)
    include(ExternalProject)
    set(MUSASHI ${BB}/Musashi)
    ExternalProject_Add(musashi
            GIT_REPOSITORY    https://github.com/kstenerud/Musashi
            GIT_TAG           313ebf1bd9f4d0d93341eb5ce21fd8a119e9dbdd
            SOURCE_DIR        ${MUSASHI}
            CONFIGURE_COMMAND ""
            BUILD_COMMAND     ""
            INSTALL_COMMAND   ""
            EXCLUDE_FROM_ALL  TRUE)
    set_source_files_properties(${MUSASHI}/m68kmake.c ${MUSASHI}/m68kcpu.c ${MUSASHI}/softfloat/softfloat.c
                                PROPERTIES GENERATED TRUE)
    tool(m68kmake ${MUSASHI}/m68kmake.c)
    add_dependencies(m68kmake musashi)
    add_custom_command(
            OUTPUT            ${MB}/m68kops.c ${MB}/m68kops.h
            COMMAND           ${CMAKE_COMMAND} -E make_directory ${MB}
            COMMAND           m68kmake ${MB} ${MUSASHI}/m68k_in.c > ${MB}/m68kmake.log 2>&1
            DEPENDS           m68kmake)
    tool(run68k bench/m68k/run68k.c bench/tiff.c test/dither.c
                ${MUSASHI}/m68kcpu.c ${MB}/m68kops.c ${MUSASHI}/softfloat/softfloat.c)
    target_include_directories(run68k PRIVATE ${MUSASHI} ${MB})
    add_dependencies(run68k musashi)
    left_out(m68kmake run68k)                           # they are made of what is fetched

    set(M68K_ASSEMBLED ${M68K_AS} -m68000 --register-prefix-optional)
    foreach(PART atk gray)
        add_custom_command(
                OUTPUT            ${MB}/${PART}.o
                COMMAND           ${CMAKE_COMMAND} -E make_directory ${MB}
                COMMAND           ${M68K_ASSEMBLED} -o ${MB}/${PART}.o ${BENCH}/m68k/${PART}.s
                DEPENDS           bench/m68k/${PART}.s)
    endforeach()
    # What the 68000 dithers: the Mandrill and its grays, each at its own
    # size and at half.
    foreach(PIC gray colour)
        foreach(SIZE own half)
            set(J ${MB}/${PIC}-${SIZE})
            set(SAMPLES 3)
            if(PIC STREQUAL gray)
                set(SAMPLES 1)
            endif()
            set(HALF 0)
            if(SIZE STREQUAL half)
                set(HALF 1)
            endif()
            add_custom_command(
                    OUTPUT            ${J}.txt
                    COMMAND           ${M68K_ASSEMBLED} --defsym SAMPLES=${SAMPLES} --defsym HALF=${HALF}
                                        -o ${J}.o ${BENCH}/m68k/glue.s
                    COMMAND           ${M68K_LD} -Ttext=${M68K_AT} -e _start -o ${J}.elf ${J}.o ${MB}/atk.o ${MB}/gray.o
                    COMMAND           ${M68K_OBJCOPY} -O binary ${J}.elf ${J}.bin
                    COMMAND           run68k ${J}.bin ${M68K_AT} ${PICTURE} ${PIC} ${SIZE} > ${J}.txt 2> ${J}.log
                    DEPENDS           run68k bench/m68k/glue.s ${MB}/atk.o ${MB}/gray.o picture ${PICTURE})
            list(APPEND M68K_FIGURES ${J}.txt)
        endforeach()
    endforeach()
else()
    file(WRITE ${BB}/m68k.txt "m68k                   68000             not run: there is no m68k-elf-as\n")
    list(APPEND M68K_FIGURES ${BB}/m68k.txt)
endif()

# What was found, as figures: each kind is one target's to make, and the
# targets that print are of those.  A target that prints figures keeps
# them, each under the image it is of (KEEP): every time, since a figure
# that is already kept is not kept again.  The 68000's are of no image of
# this machine's, and change only when its program does.
get_property(DRAW_FIGURES GLOBAL PROPERTY DRAW_FIGURES)
get_property(DITHER_FIGURES GLOBAL PROPERTY DITHER_FIGURES)
get_property(GRAY_FIGURES GLOBAL PROPERTY GRAY_FIGURES)
get_property(DRAW_KEPT GLOBAL PROPERTY DRAW_KEPT)
get_property(DITHER_KEPT GLOBAL PROPERTY DITHER_KEPT)
get_property(GRAY_KEPT GLOBAL PROPERTY GRAY_KEPT)
set(M68K_KEPT)
add_custom_target(draw-figures DEPENDS ${DRAW_FIGURES})
add_custom_target(dither-figures DEPENDS ${DITHER_FIGURES})
add_custom_target(gray-figures DEPENDS ${GRAY_FIGURES})
add_custom_target(m68k-figures DEPENDS ${M68K_FIGURES})
add_dependencies(draw-figures bench-parts)
add_dependencies(dither-figures bench-parts)
add_dependencies(gray-figures bench-parts)
function(figures TARGET)                                # and the kinds it prints
    set(PRINTED)
    set(KEPT_SO)
    foreach(KIND ${ARGN})
        if(NOT ${KIND}_FIGURES)                         # a kind that has nothing to print was not said: it is not left out
            message(FATAL_ERROR "the target ${TARGET} has no figure of the kind ${KIND} to print")
        endif()
        list(APPEND PRINTED ${${KIND}_FIGURES})
        list(APPEND KEPT_SO ${${KIND}_KEPT})
    endforeach()
    add_custom_target(${TARGET} COMMAND bench_check table ${PRINTED} ${KEPT_SO})
    foreach(KIND ${ARGN})
        string(TOLOWER ${KIND} OF)
        add_dependencies(${TARGET} ${OF}-figures)
    endforeach()
endfunction()
figures(bench-draw DRAW)
figures(bench-dither DITHER)
figures(bench-gray GRAY)
figures(bench-m68k M68K)
figures(bench DRAW DITHER GRAY M68K)

#
# `gates`: how big the CPU is, beside a Z80 and a 68000.
#
# Each is put through one flow of Yosys, to NAND gates of two inputs,
# inverters and D flip-flops, and what it comes to is given as gates
# (bench/gates.c).  A memory that is a register file is made flip-flops,
# and one that is a ROM or a stack is left a memory and said, not counted.
# The CPU is rtl/hexaforth.v with cells of 16 bits, of 32 and of 64, and
# each again without its multiplier, which the other two have none of.
#
# The other two are not kept here.  They are fetched, as they were when
# this was written: TV80 for the Z80, and fx68k for the 68000, whose
# microcode is the ROMs that are left out.  fx68k's structures are made
# packed for Yosys to read (bench/packed.cmake), which changes nothing they
# are.
#
# It wants yosys.  Everything is kept in build/bench/gates/.
#
set(GB ${BB}/gates)
if(YOSYS)
    include(ExternalProject)
    tool(gates_said bench/gates.c)

    # One flow for every CPU: what it is called, where its Verilog is, its
    # top module, what reads it (a line each), the memories to make
    # flip-flops, and whether its multipliers are left out.
    function(gates LABEL)
        cmake_parse_arguments(OF "NO_MULTIPLIER" "IN;TOP;MEMORIES" "READ;AFTER" ${ARGN})
        string(REPLACE ";" "\n" READ "${OF_READ}")
        set(MAP "techmap")
        if(OF_NO_MULTIPLIER)
            set(MAP "techmap t:$mul %n")
        endif()
        set(MEMORIES)
        if(OF_MEMORIES)
            set(MEMORIES "memory_map ${OF_MEMORIES}")
        endif()
        file(WRITE ${GB}/${LABEL}.ys
"${READ}
hierarchy -top ${OF_TOP}
proc -norom
synth -top ${OF_TOP} -flatten -noalumacc -run :fine
${MEMORIES}
opt -fast -full
${MAP}
opt -fast
abc -g NAND
opt -fast
dfflegalize -cell $_DFF_P_ 01 -cell $_DFF_PN0_ 01 -cell $_DFF_PN1_ 01 -cell $_DFF_PP0_ 01 -cell $_DFF_PP1_ 01
abc -g NAND
opt_clean
tee -o ${GB}/${LABEL}.stat stat
")
        add_custom_command(
                OUTPUT            ${GB}/${LABEL}.stat
                COMMAND           ${YOSYS} -q -l ${GB}/${LABEL}.log -s ${GB}/${LABEL}.ys > ${GB}/${LABEL}.out 2>&1
                WORKING_DIRECTORY ${OF_IN}
                COMMENT           "Gates: ${LABEL} (what Yosys says is in ${GB}/${LABEL}.log)"
                DEPENDS           ${GB}/${LABEL}.ys ${OF_AFTER})
        set_property(GLOBAL APPEND PROPERTY GATES ${LABEL})
    endfunction()

    foreach(CELL 16 32 64)
        set(READ "read_verilog hexaforth.v" "chparam -set W ${CELL} -set LAT 1 hexaforth")
        gates(hexaforth-${CELL} IN ${RTL} TOP hexaforth READ ${READ} AFTER rtl/hexaforth.v)
        gates(hexaforth-${CELL}-no-multiplier IN ${RTL} TOP hexaforth READ ${READ} AFTER rtl/hexaforth.v NO_MULTIPLIER)
    endforeach()

    ExternalProject_Add(tv80
            GIT_REPOSITORY    https://github.com/hutch31/tv80
            GIT_TAG           66a131c38d05ef58b3d8c4f1507a72e6e4aa5d65
            SOURCE_DIR        ${GB}/tv80
            CONFIGURE_COMMAND ""
            BUILD_COMMAND     ""
            INSTALL_COMMAND   ""
            EXCLUDE_FROM_ALL  TRUE)
    gates(z80-tv80 IN ${GB}/tv80/rtl/core TOP tv80s
          READ "read_verilog tv80s.v tv80_core.v tv80_alu.v tv80_mcode.v tv80_reg.v"
          MEMORIES "t:$mem_v2" AFTER tv80)

    ExternalProject_Add(fx68k
            GIT_REPOSITORY    https://github.com/ijor/fx68k
            GIT_TAG           0602ee4627b10f301298f2673d826cdd6baa9327
            SOURCE_DIR        ${GB}/fx68k
            CONFIGURE_COMMAND ""
            BUILD_COMMAND     ""
            INSTALL_COMMAND   ""
            EXCLUDE_FROM_ALL  TRUE)
    set(PACKED ${GB}/fx68k-packed)
    add_custom_command(
            OUTPUT            ${PACKED}/fx68k.sv
            COMMAND           ${CMAKE_COMMAND} -E make_directory ${PACKED}
            COMMAND           ${CMAKE_COMMAND} -E copy ${GB}/fx68k/fx68kAlu.sv ${GB}/fx68k/uaddrPla.sv
                                ${GB}/fx68k/microrom.mem ${GB}/fx68k/nanorom.mem ${PACKED}
            COMMAND           ${CMAKE_COMMAND} -DFROM=${GB}/fx68k/fx68k.sv -DTO=${PACKED}/fx68k.sv -P ${BENCH}/packed.cmake
            DEPENDS           fx68k bench/packed.cmake)
    gates(m68000-fx68k IN ${PACKED} TOP fx68k
          READ "read_verilog -sv fx68k.sv fx68kAlu.sv uaddrPla.sv"
          MEMORIES "fx68k/excUnit.regs68L fx68k/excUnit.regs68H" AFTER ${PACKED}/fx68k.sv)

    get_property(GATES GLOBAL PROPERTY GATES)
    set(COUNTED)
    set(SAID)
    foreach(LABEL ${GATES})
        list(APPEND COUNTED ${GB}/${LABEL}.stat)
        list(APPEND SAID COMMAND gates_said ${LABEL} ${GB}/${LABEL}.stat)
    endforeach()
    add_custom_target(gates ${SAID} COMMAND ${KEEP} gates ${RTL}/hexaforth.v ${COUNTED} DEPENDS ${COUNTED})
    add_dependencies(gates tv80 fx68k)
else()
    add_custom_target(gates COMMAND ${CMAKE_COMMAND} -E echo "gates: not run: there is no yosys")
endif()

#
# The files the tree's text says by name, each of which is to be there: the
# target `said` (util/said.c).
#
# A file is named by what includes it, by this file, in comments and in
# CLAUDE.md, and when its name changes nothing that is built finds the
# places that only say it.  The kinds of file that are looked for are the
# tree's source; build/ is made, and worktrees/ is other trees.
#
# What is said and is not a file of the tree is said here, by what it is:
# another's, which the system has or which is fetched; what is made when
# something is built; a name that stands for whatever file a reader has; a
# file that is gone, where doc/boot.md says what was; and a word of the
# Forth that ends as a file's name does.
#
set(SAID_ANOTHERS ltdl.h unicorn/unicorn.h libcc.fs
                  m68k.h m68kcpu.c m68kops.c m68kops.h m68kmake.c m68k_in.c softfloat/softfloat.c
                  tv80s.v tv80_core.v tv80_alu.v tv80_mcode.v tv80_reg.v)
set(SAID_MADE     test_cases.fs cmake_install.cmake typed.fs asks-gray.fs asks-PIC-SIZE.fs half.fs phrasetest.fs
                  Vhexaforth.h Vhexaforth___024root.h Vhf_mem.h Vsoc.h)
set(SAID_ANY      name.fs program.fs yourprogram.fs mine.fs)
set(SAID_GONE     vmdisk.fs virtio-blk.fs)
set(SAID_WORDS    cw.h)
tool(said_files util/said.c)
add_custom_target(said
        COMMAND           said_files ${GIT} c h cpp fs S v cmake md
                            --not worktrees
                            --besides ${SAID_ANOTHERS} ${SAID_MADE} ${SAID_ANY} ${SAID_GONE} ${SAID_WORDS}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        DEPENDS           said_files
        VERBATIM)

#
# The figures the tree's comments and documents say: the target `figures`
# (util/said.c).  How big a thing is, how many there are of a thing and
# what a thing costs is what a target prints, and is not written down
# beside it to be kept in step.  This prints each line of a comment or a
# document that has a number and then one of these words, to be read: it
# cannot tell what the machine is from how much there was of something,
# and fails nothing.
#
set(FIGURES_OF % percent bytes KB MB instructions cycles clocks rules words lines blocks files
               tests checks times million states loops cells headers names entries variables
               seconds turns pairs calls characters pixels windows glyphs definitions places)
add_custom_target(figures
        COMMAND           said_files ${GIT} --figures ${FIGURES_OF} --not worktrees
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        DEPENDS           said_files
        VERBATIM)

#
# What clangd says of the C: the target `clangd`.
#
# A language server says what it finds wrong with a file each time the file
# is changed, so what it says of the tree as it is would be said again
# after every change.  This asks it of every file of C that is compiled as
# a target here, a file at a time, as it is configured (.clangd) and as
# CMake says the file is compiled (compile_commands.json, above); keeps
# what it printed of each in build/clangd/; and prints what it found wrong
# and of how many files (util/clangd.c), failing if it found anything.
#
# It is asked for its warnings as well as its errors, which it leaves out
# unless told.  It is not asked to try each of its features at every word
# of the file (--check-locations), which it does unless told: one of those
# is each change it can offer to make to the source, and its offer to turn
# round the two sides of an operator fails on an operator that is in a
# macro (DISK_DRIVE_OF, vm_constants.h).  Nothing here asks it for such a
# change, and a server does not try one until it is asked.
#
# It is asked of every header too, in each directory that has a file of C:
# a header is not compiled by itself, but clangd reads it by itself in a
# session, so it is to include what it needs.  What is fetched or made in
# build/ (the 68000, bench-m68k) is not asked of: it is another's source,
# and is not changed here.
#
tool(clangd_said util/clangd.c)
get_property(EVERY_TARGET DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
set(C_FILES)
foreach(TARGET ${EVERY_TARGET})
    get_target_property(KIND ${TARGET} TYPE)
    if(NOT KIND MATCHES "^(EXECUTABLE|STATIC_LIBRARY|SHARED_LIBRARY|MODULE_LIBRARY|OBJECT_LIBRARY)$")
        continue()
    endif()
    get_target_property(SOURCES ${TARGET} SOURCES)
    foreach(SOURCE ${SOURCES})
        if(SOURCE MATCHES "\\.c$")
            get_filename_component(SOURCE ${SOURCE} ABSOLUTE)
            string(FIND "${SOURCE}" "${BUILD}/" MADE)
            if(NOT MADE EQUAL 0)
                list(APPEND C_FILES ${SOURCE})
            endif()
        endif()
    endforeach()
endforeach()
list(REMOVE_DUPLICATES C_FILES)
set(C_DIRECTORIES)
foreach(FILE ${C_FILES})
    get_filename_component(WHERE ${FILE} DIRECTORY)
    list(APPEND C_DIRECTORIES ${WHERE})
endforeach()
list(REMOVE_DUPLICATES C_DIRECTORIES)
foreach(WHERE ${C_DIRECTORIES})
    file(GLOB HEADERS CONFIGURE_DEPENDS ${WHERE}/*.h)
    list(APPEND C_FILES ${HEADERS})
endforeach()
list(SORT C_FILES)
if(CLANGD)
    set(CS ${BUILD}/clangd)
    set(ASKED)
    set(SAID)
    foreach(FILE ${C_FILES})
        file(RELATIVE_PATH LOG ${CMAKE_SOURCE_DIR} ${FILE})
        string(REPLACE "/" "-" LOG ${LOG})
        set(LOG ${CS}/${LOG}.log)
        list(APPEND ASKED COMMAND ${CLANGD} --check=${FILE} --check-warnings --check-locations=0
                                    > ${LOG} 2>&1 || ${CMAKE_COMMAND} -E true)
        list(APPEND SAID ${LOG})
    endforeach()
    add_custom_target(clangd
            COMMAND           ${CMAKE_COMMAND} -E make_directory ${CS}
            ${ASKED}
            COMMAND           clangd_said ${SAID}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            DEPENDS           clangd_said
            VERBATIM)
else()
    add_custom_target(clangd COMMAND ${CMAKE_COMMAND} -E echo "clangd: not run: there is no clangd")
endif()

#
# Every test there is, run: a line for each, and all that each said kept
# (Testing/Temporary/LastTest.log, where CMake builds).  `ctest -V -R NAME`
# there runs the ones whose names have that in them, and prints what they
# say.  Either way a test's needs are built before it is run (needs,
# above): a line for each of those too, built-TARGET, and a test whose
# needs do not build is not run.
#
# The tests run in parallel, CHECK_JOBS at a time: 4 unless CMake is told
# otherwise (cmake -DCHECK_JOBS=8 build/cmake), so that the machine is left
# room for other work while they run.  That is safe because no test writes
# a file another test uses: each writes to its own directory (QEMU's tests
# to a fresh temporary one), what the Verilog and the kernels are compared
# with is made when they are built, and one thing is built at a time.  A
# new test that writes files needs a directory of its own too.
#
set(CHECK_JOBS 4 CACHE STRING "how many tests the target check runs at once")
get_property(EVERY_TEST DIRECTORY PROPERTY TESTS)
foreach(TEST ${EVERY_TEST})                             # each that has not been given longer
    get_test_property(${TEST} TIMEOUT GIVEN)
    if(NOT GIVEN)
        set_tests_properties(${TEST} PROPERTIES TIMEOUT ${TEST_SECONDS})
    endif()
    # A test that does not say what it needs would be run on whatever
    # happened to be built: there is none.
    get_test_property(${TEST} FIXTURES_REQUIRED NEEDED)
    get_test_property(${TEST} FIXTURES_SETUP BUILDS)
    if(NOT NEEDED AND NOT BUILDS)
        message(FATAL_ERROR "the test ${TEST} does not say what it needs built (needs)")
    endif()
endforeach()
# Before its tests, each of the two says the names the system's source
# defines again, on every machine (util/again.c, of what cross.fs wrote
# beside each image), and what each machine's memory has used and has left
# (util/map.c, --table: memory-table-NAME): so each has those made first.
# Neither is an error.  They are said where whoever is at work reads them,
# and before the tests, so that they are there when a test fails.
set(AGAIN_SAID)
set(AGAIN_OF)
foreach(NAME ${MACHINES})
    list(APPEND AGAIN_SAID ${NAME} ${BUILD}/${NAME}/nuc.shadows)
    list(APPEND AGAIN_OF machine-${NAME})
endforeach()
get_property(MEMORY_TABLES GLOBAL PROPERTY MEMORY_TABLES)
get_property(MEMORY_TABLED GLOBAL PROPERTY MEMORY_TABLED)
add_custom_target(check
        COMMAND           again ${AGAIN_SAID}
        COMMAND           ${CMAKE_COMMAND} -E cat ${MEMORY_TABLES}
        COMMAND           ${CMAKE_CTEST_COMMAND} --output-on-failure -j ${CHECK_JOBS}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        USES_TERMINAL)
add_dependencies(check ${AGAIN_OF} ${MEMORY_TABLED})

# quick is the tests that need only the reference VM (ON_THE_VM): every
# machine's prompt, boot, desktop and drawing, and the two builds compared.
# It is what is run while the source is being changed, and it is what
# building with no target named does: it is built after everything else
# such a build makes, since its tests build what they need themselves, one
# target at a time.
add_custom_target(quick ALL
        COMMAND           again ${AGAIN_SAID}
        COMMAND           ${CMAKE_COMMAND} -E cat ${MEMORY_TABLES}
        COMMAND           ${CMAKE_CTEST_COMMAND} --output-on-failure -j ${CHECK_JOBS} -L ${ON_THE_VM}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        USES_TERMINAL)
add_dependencies(quick ${AGAIN_OF} ${MEMORY_TABLED})
get_property(EVERY_TARGET DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(EACH ${EVERY_TARGET})
    get_target_property(LEFT_OUT ${EACH} EXCLUDE_FROM_ALL)
    if(NOT LEFT_OUT AND NOT "${EACH}" STREQUAL "quick")
        add_dependencies(quick ${EACH})
    endif()
endforeach()
