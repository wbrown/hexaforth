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
      FOR "everything: the C is compiled with it, and gforth builds util/fuse.c with it")
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
      FOR "every image: gforth builds util/fuse.c through it")
needs(LTDL_HEADER   HEADER ltdl.h               APT libltdl-dev                 BREW libtool
      FOR "every image: what gforth builds of util/fuse.c is built with it")
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
# with libtool; and where gforth is Homebrew's, the compiler has to be told
# where Homebrew's headers are, gforth's own among them.
set(MACHINE_SOURCES util/fuse.c util/named.c util/room.c vm.c vm_step.h vm.h vm_opcodes.c
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

# hexaforth nucleus image from swapforth
add_custom_command(
        COMMAND           ${MAKE_BUILD_DIR}
        COMMAND           ${RUN_GFORTH} cross.fs basewords.fs nuc.fs
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
        COMMAND           ${RUN_GFORTH} cross.fs basewords.fs test_cases.fs
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
# is known.  It has fuse.c in it, which has the machine.  How long a search
# may take, in what it looks at, is said here and nowhere else: what is
# looked for, how deep and how long, is the run's or the turn's to say.
set(SEARCH_STATES 1000000 CACHE STRING "the states a search for fewer instructions looks at before it is given up (util/fewer.c)")
add_executable(fewer_test
        test/fewer_test.c
        util/image.c)
target_compile_options(fewer_test PRIVATE -UDEBUG -w)
target_compile_definitions(fewer_test PRIVATE SEARCH_STATES=${SEARCH_STATES})
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
        COMMAND           ${CURL} -sSfL --remove-on-error -o ${PICTURE}
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
# nucleus with the peephole off is plain-NAME's.  Whatever else is made
# from one of those files depends on the file, so that it is made again
# when the file is, and on the target, so that the file is made in one
# place and not by two targets at once.  made() says a file is the first
# target's.
function(made NAME)
    set_property(GLOBAL APPEND PROPERTY MADE_FOR_${NAME} ${ARGN})
endfunction()

function(machine NAME)
    set(AT ${BUILD}/${NAME})
    set(DEFINITIONS_${NAME} ${ARGN} CACHE INTERNAL "what the C compiler is told for the machine ${NAME}")
    set(CELL ${ARGN})
    list(FILTER CELL INCLUDE REGEX "^CELL_BITS=")
    set(CELL_OF_${NAME} ${CELL} CACHE INTERNAL "and of that, its cell")
    set(SIZES ${ARGN})
    list(REMOVE_ITEM SIZES STRICT)
    set(SIZES_OF_${NAME} ${SIZES} CACHE INTERNAL "and all of it that is the machine's sizes")

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
                                ${RUN_GFORTH} ./cross.fs ${AT}/basewords.fs ./nuc.fs > ${AT}/cross.log 2>&1
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
                                ${RUN_GFORTH} ./cross.fs ${AT}/basewords.fs ./nuc.fs > ${AT}/plain/cross.log 2>&1
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/forth
            COMMENT           "The nucleus for the machine ${NAME}, with the peephole off (${AT}/plain/cross.log)"
            DEPENDS           machine-${NAME} ${NUCLEUS})
    add_custom_target(plain-${NAME} DEPENDS ${PLAIN})

    machine_tool(hexaforth ${NAME} main.c ${VM_SOURCES})
    # An image's code, word by word, as the decompiler shows it, with every
    # instruction's place, number and fields (util/disasm_hex.c, which says
    # at its top what it shows): disasm_hex-NAME IMAGE.hex [NAME... | FROM TO].
    # It has the machine in it, for where a branch goes.
    machine_tool(disasm_hex ${NAME} util/disasm_hex.c vm_opcodes.c vm_debug.c util/stack.c util/image.c)
    # The reference VM of the machine, counting how often the instruction at
    # each place is run (bench/counting.c): counting-NAME IMAGE.hex
    # [DISK.img [MEMORY.out]], and the counts written where RUN_COUNTS says.
    machine_tool(counting ${NAME} bench/counting.c vm_opcodes.c vm_debug.c util/stack.c)
    # What work counted that way costs (bench/costs.c): costs-NAME; and its
    # loops, each with the fewest instructions found that do a turn of it
    # (bench/shorter.c): shorter-NAME.
    stepping_tool(costs ${NAME} bench/costs.c util/image.c)
    stepping_tool(shorter ${NAME} bench/shorter.c util/image.c)
    target_compile_definitions(shorter-${NAME} PRIVATE SEARCH_STATES=${SEARCH_STATES})
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
            DEPENDS           mkdisk picture ${PICTURE} test/source.fs)
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
            DEPENDS           mkdisk picture ${PICTURE} baremetal/block1.fs ${AT}/nuc.tables)
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

# What a machine's image is made of (util/examine.c, as the target `examine`
# says it of the image where the source is): the target examine-NAME.  The
# kernel is to fit in its ROM on every machine, and this is what says how
# long it is on each.
function(examined NAME)
    set(AT ${BUILD}/${NAME})
    machine_tool(examine-image ${NAME} util/examine.c vm_opcodes.c vm_debug.c util/image.c)
    add_custom_target(examine-${NAME}
            COMMAND           examine-image-${NAME} ${AT}/nuc.hex ${AT}/nuc.tables ${AT}/nuc.sizes
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_dependencies(examine-${NAME} machine-${NAME})
endfunction()

# What a machine's display shows, from its memory as the reference VM
# writes it when it has stopped (util/screen.c): screen-NAME, which the
# target `seen` builds for every machine, with what makes a PNG of one
# (util/png.c).
function(shown NAME)
    machine_tool(screen ${NAME} util/screen.c)
    set_property(GLOBAL APPEND PROPERTY SEEN screen-${NAME})
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
    if(HOW_MUCH GREATER 2)
        examined(${CELL})                               # the names it reads are a machine's that has a display
        shown(${CELL})
    endif()
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
# run (bench/counting.c), in place of vm.c, as the VM the desktop is worked
# on; by itself it is counting-NAME, which every machine has (machine).
set(COUNTED_SOURCES test/nucleus.c bench/counting.c vm_opcodes.c vm_debug.c util/stack.c util/image.c)
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

# `screenshot`: the desktop with every window open and HexaPaint in front,
# as the README shows it (doc/desktop.png, which is checked in).  The
# desktop's test is run with a directory of its own for its screens, and
# the screen it leaves when HexaPaint has been drawn in, put behind another
# window and brought back to the front is made a PNG (util/png.c).  The
# target makes it again when the desktop looks otherwise.
set(SHOT ${BUILD}/screenshot)
tool(png util/png.c)
get_property(SEEN GLOBAL PROPERTY SEEN)
add_custom_target(seen DEPENDS ${SEEN} png)

# `hello`: a window made by what is typed at the prompt (doc/hello.fs,
# which DESKTOP.md is about), on the reference VM of the first machine: what
# it printed, and its display as a picture (build/hello/hello.png).
set(HELLO ${BUILD}/hello)
add_custom_target(hello
        COMMAND           ${CMAKE_COMMAND} -E make_directory ${HELLO}
        COMMAND           hexaforth-${FIRST} ${NUC} ${OF}/disk.img ${HELLO}/memory.out < doc/hello.fs > ${HELLO}/hello.txt 2>&1
        COMMAND           ${CMAKE_COMMAND} -E cat ${HELLO}/hello.txt
        COMMAND           screen-${FIRST} ${HELLO}/memory.out ${HELLO}/hello.pbm
        COMMAND           png ${HELLO}/hello.pbm ${HELLO}/hello.png
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_dependencies(hello machine-${FIRST})
add_custom_target(screenshot
        COMMAND           ${CMAKE_COMMAND} -E make_directory ${SHOT}/desktop
        COMMAND           $<TARGET_FILE:desktop_test-${FIRST}> ${NUC} ${OF}/disk.img ${PICTURE} forth ${SHOT}/desktop names
                            > ${SHOT}/desktop.log 2>&1
        COMMAND           png ${SHOT}/desktop/16-again.pbm doc/desktop.png
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT           "The desktop worked, and its screen with HexaPaint in front (what its test says is in ${SHOT}/desktop.log)")
add_dependencies(screenshot machine-${FIRST})

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
stepping_tool(survey ${FIRST} util/survey.c util/image.c)
set(PROFILED profile-${FIRST} ${NUC} ${TABLES} ${SIZES} ${OF}/disk.img ${PICTURE} forth)    # and where it leaves the work
add_custom_target(profile
        COMMAND           ${CMAKE_COMMAND} -E make_directory ${PROFILE}
        COMMAND           ${PROFILED} ${PROFILE}
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
stepping_tool(uses ${FIRST} bench/uses.c util/image.c)
add_custom_command(
        OUTPUT            ${USES}/desktop.counts
        COMMAND           ${CMAKE_COMMAND} -E make_directory ${USES}
        COMMAND           ${PROFILED} ${USES} > ${USES}/profile.txt 2> ${USES}/profile.log
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT           "The desktop's work, counted (${USES}/profile.log says what went wrong, if anything did)"
        DEPENDS           profile-${FIRST} machine-${FIRST} ${NUC} ${OF}/disk.img)
add_custom_target(subtracts
        COMMAND           uses-${FIRST} ${OF}/plain/nuc.hex ${OF}/plain/nuc.tables ${NUC} ${TABLES} ${USES}/desktop.counts
                            - 1- negate > ${USES}/subtracts.txt
        COMMAND           ${CMAKE_COMMAND} -E cat ${USES}/subtracts.txt
        DEPENDS           ${USES}/desktop.counts)
add_dependencies(subtracts machine-${FIRST} plain-${FIRST})

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
target_compile_definitions(phrase-book PRIVATE GFORTH="${GFORTH}")
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
target_compile_definitions(fused PRIVATE GFORTH="${GFORTH}")
add_custom_target(tables
        COMMAND           joins forth/folds.fs
        COMMAND           ${WITH_HEADERS} ${CMAKE_COMMAND} -DGFORTH=${GFORTH} -DFUSED=$<TARGET_FILE:fused>
                            -P util/tables.cmake
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

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
#   demo-time   says when the desktop is ready and when its picture is made
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
set(OF_KERNEL ${BUILD}/${KERNELS_OF})
set(LOAD 0x40080000)                # where QEMU's virt machine has a kernel start
set(KERNEL_SIZES ${SIZES_OF_${KERNELS_OF}})
list(TRANSFORM KERNEL_SIZES PREPEND -D)
set(CROSS_CLANG ${CLANG} --target=aarch64-none-elf -c ${KERNEL_SIZES})

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

# The image a kernel has in it, as bytes; and the bridge that shows the
# display on QEMU (baremetal/ramfb.S), which a kernel with a display is
# linked after, since the machine's memory runs on from the end of the
# kernel.  A bridge that draws each pixel more than once across and down is
# another bridge, named for how many.
tool(hex2bin util/hex2bin.c util/image.c)
add_custom_command(
        OUTPUT            ${KB}/image.bin
        COMMAND           ${CMAKE_COMMAND} -E make_directory ${KB}
        COMMAND           hex2bin ${OF_KERNEL}/nuc.hex ${KB}/image.bin
        DEPENDS           hex2bin machine-${KERNELS_OF} ${OF_KERNEL}/nuc.hex)
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
add_custom_target(kernel-parts DEPENDS ${KB}/image.bin ${BRIDGES})

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
#
# Each is another file, named for what it is; KERNEL_FILE is its name,
# which the target kernel-NAME makes: NAME.elf, NAME.bin, which is what
# QEMU's machine has in memory, and NAME.symbols, where its symbols are.
function(kernel WHICH)
    cmake_parse_arguments(IS "DISPLAY;PROFILE;STRICT" "MIPS;SCALE" "" ${ARGN})
    set(NAME ${WHICH})
    set(TOLD)
    set(BRIDGE)
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
    if(TARGET kernel-${NAME})
        return()
    endif()
    set(FILE ${KB}/${NAME})
    add_custom_command(
            OUTPUT            ${FILE}.o
            COMMAND           ${CROSS_CLANG} "-DIMAGE=\"${KB}/image.bin\"" ${TOLD}
                                ${CMAKE_SOURCE_DIR}/baremetal/${WHICH}.S -o ${FILE}.o
            DEPENDS           baremetal/${WHICH}.S kernel-parts ${KB}/image.bin
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
kernel_tool(difftest_dump baremetal/difftest_dump.c ${VM_SOURCES} test/compiler.c test/vm_test.c)
kernel_tool(difftest baremetal/difftest.c)
find_library(UNICORN unicorn PATHS ${BREW_PREFIX}/lib)
if(UNICORN_HEADER AND UNICORN)      # without it difftest does not build, and says what it has not
    target_include_directories(difftest PRIVATE ${UNICORN_HEADER})
    target_link_libraries(difftest ${UNICORN})
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

# The tests: each kernel against vm.c, and the desktop on QEMU with each,
# the small one halting if it leaves memory.  A desktop's screens, and its
# copy of the disk, are kept in a directory named for the kernel.
function(kernel_test WHICH)
    kernel(${WHICH})
    add_test(NAME kernel-${WHICH}
             COMMAND difftest ${KERNEL_FILE}.bin ${KB}/cases.bin ${KERNEL_FILE}.symbols)
    wanted(kernel-${WHICH})
endfunction()
function(qemu_test WHICH)
    kernel(${WHICH} DISPLAY ${ARGN})
    get_filename_component(NAME ${KERNEL_FILE} NAME)
    add_test(NAME qemu-${NAME}
             COMMAND ${CMAKE_COMMAND} -E env --unset=DISK SCALE=1
                     $<TARGET_FILE:guitest> ${KERNEL_FILE}.elf ${OF_KERNEL}/disk.img ${PICTURE} forth ${KB}/screens-${NAME}
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_property(GLOBAL APPEND PROPERTY SCREENS ${KB}/screens-${NAME})
    wanted(kernel-${NAME})
endfunction()
kernel_test(hexaforth)
kernel_test(hexaforth-fast)
qemu_test(hexaforth STRICT)
qemu_test(hexaforth-fast)
get_property(SCREENS GLOBAL PROPERTY SCREENS)
add_custom_target(kernel-tests
        COMMAND           ${CMAKE_COMMAND} -E make_directory ${SCREENS}
        DEPENDS           ${KB}/cases.bin)
wanted(kernel-tests difftest guitest machine-${KERNELS_OF})

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
get_filename_component(PLAIN_KERNEL ${KERNEL_FILE} NAME)
add_custom_target(run
        COMMAND           ${QEMU} -nographic -kernel ${KERNEL_FILE}.elf
        USES_TERMINAL)
add_dependencies(run kernel-${PLAIN_KERNEL})
add_custom_target(size
        COMMAND           kernel_size ${KERNEL_FILE}.symbols ${KERNEL_FILE}.bin)
add_dependencies(size kernel-${PLAIN_KERNEL})

# The desktop is given a disk of its own, which is made of the machine's
# when that is made again and is otherwise as the desktop left it.
add_custom_command(
        OUTPUT            ${KB}/demo.img
        COMMAND           ${CMAKE_COMMAND} -E copy ${OF_KERNEL}/disk.img ${KB}/demo.img
        DEPENDS           machine-${KERNELS_OF} ${OF_KERNEL}/disk.img)
kernel(${KERNEL} DISPLAY SCALE ${SCALE} ${SAID})
get_filename_component(SHOWN_KERNEL ${KERNEL_FILE} NAME)
add_custom_target(demo
        COMMAND           ${QEMU} -m 256 -serial mon:stdio -device ramfb -global virtio-mmio.force-legacy=false
                            -drive "file=${KB}/demo.img,if=none,format=raw,id=hd0${SLOW}"
                            -device virtio-blk-device,drive=hd0
                            -device virtio-keyboard-device -device virtio-tablet-device
                            -kernel ${KERNEL_FILE}.elf
        DEPENDS           ${KB}/demo.img
        USES_TERMINAL
        VERBATIM)
add_dependencies(demo kernel-${SHOWN_KERNEL} machine-${KERNELS_OF})

set(STRICTLY)
if(STRICT)
    set(STRICTLY STRICT)
endif()
kernel(${KERNEL} DISPLAY SCALE ${SCALE} ${STRICTLY})
get_filename_component(TESTED_KERNEL ${KERNEL_FILE} NAME)
add_custom_target(demo-test
        COMMAND           ${CMAKE_COMMAND} -E make_directory ${KB}/screens
        COMMAND           ${CMAKE_COMMAND} -E env --unset=DISK SCALE=${SCALE}
                            $<TARGET_FILE:guitest> ${KERNEL_FILE}.elf ${OF_KERNEL}/disk.img ${PICTURE} forth ${KB}/screens
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_dependencies(demo-test kernel-${TESTED_KERNEL} machine-${KERNELS_OF})

set(HELD)
if(MIPS)
    set(HELD MIPS ${MIPS})
endif()
kernel(${KERNEL} DISPLAY ${HELD})
get_filename_component(TIMED_KERNEL ${KERNEL_FILE} NAME)
add_custom_target(demo-time
        COMMAND           ${CMAKE_COMMAND} -E make_directory ${KB}/timed
        COMMAND           ${CMAKE_COMMAND} -E env DISK=${DISK}
                            $<TARGET_FILE:desktime> ${KERNEL_FILE}.elf ${KERNEL_FILE}.symbols ${OF_KERNEL}/disk.img ${PICTURE}
                            forth ${OF_KERNEL}/nuc.tables ${KB}/timed
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_dependencies(demo-time kernel-${TIMED_KERNEL} machine-${KERNELS_OF})

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
# kernel is in a ROM.  Its memory is 128KB at every width.  The program
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
#               cells: 128KB if it is not said), SYNTH_LAT and SYNTH_KROM
#               say what of
#
set(RB ${BUILD}/rtl)
set(RTL ${CMAKE_SOURCE_DIR}/rtl)
set(RTL_MEMORY 17)                  # the log2 of the bytes of memory a CPU is tested with
set(RTL_PC 16)
set(VERILATED ${VERILATOR} -Wall -Wno-UNUSED -Wno-DECLFILENAME -Wno-PINCONNECTEMPTY -Wno-WIDTH -O3
                        --x-assign fast --x-initial fast)

# The log2 of the cells that so much memory is, of a cell of some size.
function(cells_of RESULT BYTES_LOG2 CELL)
    math(EXPR BYTES "${CELL} / 8")
    while(BYTES GREATER 1)
        math(EXPR BYTES "${BYTES} / 2")
        math(EXPR BYTES_LOG2 "${BYTES_LOG2} - 1")
    endwhile()
    set(${RESULT} ${BYTES_LOG2} PARENT_SCOPE)
endfunction()

# What makes an image into what the Verilog's memory is loaded from, and
# the kernel's ROM, for a cell and so much memory (util/memimage.c).
function(memimage CELL ADDR)
    if(NOT TARGET memimage-${CELL}-${ADDR})
        tool(memimage-${CELL}-${ADDR} util/memimage.c util/image.c)
        target_compile_definitions(memimage-${CELL}-${ADDR} PRIVATE CELL_BITS=${CELL} ADDR_BITS=${ADDR})
    endif()
endfunction()

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
    tool(difftest_dump-${CELL} baremetal/difftest_dump.c ${VM_SOURCES} test/compiler.c test/vm_test.c)
    target_compile_definitions(difftest_dump-${CELL} PRIVATE CELL_BITS=${CELL})
    add_custom_command(
            OUTPUT            ${RB}/cases-${CELL}.bin
            COMMAND           ${CMAKE_COMMAND} -E make_directory ${RB}
            COMMAND           difftest_dump-${CELL} ${RB}/cases-${CELL}.bin
            DEPENDS           difftest_dump-${CELL})
    add_custom_command(
            OUTPUT            ${RB}/session-${CELL}.out
            COMMAND           ${CMAKE_COMMAND} -E make_directory ${RB}
            COMMAND           hexaforth-${M} ${BUILD}/${M}/nuc.hex < ${RTL}/session.fs > ${RB}/session-${CELL}.out
            DEPENDS           hexaforth-${M} machine-${M} ${BUILD}/${M}/nuc.hex rtl/session.fs)
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
    memimage(${CELL} ${ADDR})
    set(TOLD -CFLAGS "-DW=${CELL} -DAB=${ADDR} -DLAT=${LAT}")

    verilog_cpu(${B} ${CELL} ${ADDR} ${LAT})
    add_custom_command(
            OUTPUT            ${B}/tb_soc ${B}/mem.hex
            COMMAND           ${CMAKE_COMMAND} -E make_directory ${B}
            COMMAND           memimage-${CELL}-${ADDR} ${NUC} ${B}
            COMMAND           ${VERILATED} ${TOLD} --cc soc.v hexaforth.v hf_mem.v --top-module soc
                                -GW=${CELL} -GAB=${ADDR} -GLAT=${LAT} "-GINIT=\"${B}/mem.hex\"" ${KROM}
                                --exe ${RTL}/tb_soc.cpp --Mdir ${B}/soc -o ../tb_soc --build -j 4 > ${B}/tb_soc.log 2>&1
            WORKING_DIRECTORY ${RTL}
            COMMENT           "The Verilog system ${NAME} (what Verilator says is in ${B}/tb_soc.log)"
            DEPENDS           rtl/soc.v rtl/hexaforth.v rtl/hf_mem.v rtl/tb_soc.cpp
                              memimage-${CELL}-${ADDR} verilog-${CELL} ${NUC}
            VERBATIM)
    add_custom_command(
            OUTPUT            ${B}/cpu.out ${B}/cpu.err
            COMMAND           ${B}/tb run ${NUC} < ${RTL}/session.fs > ${B}/cpu.out 2> ${B}/cpu.err
            DEPENDS           ${B}/tb verilog-${CELL} ${NUC} rtl/session.fs)
    add_custom_command(
            OUTPUT            ${B}/soc.out ${B}/soc.err
            COMMAND           ${B}/tb_soc < ${RTL}/session.fs > ${B}/soc.out 2> ${B}/soc.err
            DEPENDS           ${B}/tb_soc rtl/session.fs)
    add_custom_target(verilog-${NAME} DEPENDS ${B}/tb ${B}/tb_soc ${B}/cpu.out ${B}/soc.out)
    add_dependencies(verilog-${NAME} verilog-${CELL})

    add_test(NAME verilog-cases-${NAME} COMMAND ${B}/tb cases ${RB}/cases-${CELL}.bin)
    add_test(NAME verilog-session-${NAME}
             COMMAND session_test ${RB}/session-${CELL}.out ${B}/cpu.out ${B}/soc.out ${B}/cpu.err)
    wanted(verilog-${NAME})
endfunction()

tool(session_test test/session_test.c)
wanted(session_test)
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
wanted(verilog-memory)

# Synthesis.
set(SYNTH_CELL 64 CACHE STRING "the bits of a cell of the system `synth` synthesises")
set(SYNTH_ADDR "" CACHE STRING "the log2 of the cells of its memory: 128KB, if it is not said")
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
    memimage(${SYNTH_CELL} ${SYNTH_AB})
    set(SYNTH_ROM COMMAND memimage-${SYNTH_CELL}-${SYNTH_AB} ${BUILD}/default-${SYNTH_CELL}/nuc.hex ${S})
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
#                   and at half of it.  Each bitmap is compared with the
#                   dither a pixel at a time (test/dither.c).  A machine
#                   that has not the memory for one says so, and that is
#                   what is printed
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
#   bench           the first three
#
# A figure is printed by what checked the work it is of (bench/check.c),
# and nothing is printed for work that was not checked.  Everything is
# kept in build/bench/; a line a result is printed, and the target fails
# if a check did.  No figure is kept in the repository: what a change costs
# is what this prints before it and after.
#
set(BB ${BUILD}/bench)
set(BENCH ${CMAKE_SOURCE_DIR}/bench)

# The disk: the system's, and the picture's grays as a file after it
# (bench/disk.c, which also writes what says where the two pictures are).
tool(picture-disk bench/disk.c bench/tiff.c test/dither.c)
add_custom_command(
        OUTPUT            ${BB}/disk.img ${BB}/blocks.fs
        COMMAND           ${CMAKE_COMMAND} -E make_directory ${BB}
        COMMAND           mkdisk ${PICTURE} ${BB}/system.img > ${BB}/disk.log 2>&1
        COMMAND           picture-disk ${PICTURE} ${BB}/system.img ${BB}/disk.img ${BB}/blocks.fs >> ${BB}/disk.log 2>&1
        DEPENDS           mkdisk picture-disk picture ${PICTURE})

# What is typed after bench/dither.fs to dither one picture at one size,
# say where its bitmap is, and stop.
set(PICTURES gray colour)
set(PICTURE_SIZES own half)
foreach(PIC ${PICTURES})
    foreach(SIZE ${PICTURE_SIZES})
        file(WRITE ${BB}/asks-${PIC}-${SIZE}.fs "mark ${PIC}-picture ${SIZE} mark\nb @ . b 1+ @ .\n8000 execute\n")
    endforeach()
endforeach()

# The work on a machine: its nucleus, the reference VM, the Verilog CPU
# with as much memory, and what checks a bitmap of its kind.
function(bench_on NAME)
    string(REGEX MATCH "CELL_BITS=([0-9]+)" HAS "${SIZES_OF_${NAME}}")
    set(CELL ${CMAKE_MATCH_1})
    string(REGEX MATCH "ADDR_BITS=([0-9]+)" HAS "${SIZES_OF_${NAME}}")
    set(ADDR ${CMAKE_MATCH_1})
    set(NUC ${BUILD}/${NAME}/nuc.hex)
    machine_tool(check ${NAME} bench/check.c bench/tiff.c test/dither.c)
    foreach(LAT 1 2)
        set(T ${BB}/tb-${NAME}-${LAT})
        verilog_cpu(${T} ${CELL} ${ADDR} ${LAT})
        set_property(GLOBAL APPEND PROPERTY BENCH_PARTS ${T}/tb)
        set(ON "${CELL} bits LAT=${LAT}")
        set(WANTS bench-parts ${T}/tb ${BB}/disk.img machine-${NAME} ${NUC} check-${NAME})

        set(J ${BB}/draw-${NAME}-${LAT})
        add_custom_command(
                OUTPUT            ${J}.txt
                COMMAND           ${T}/tb run ${NUC} ${BB}/disk.img ${J}.mem < ${BENCH}/draw.fs > ${J}.out 2> ${J}.err
                COMMAND           hexaforth-${NAME} ${NUC} ${BB}/disk.img ${J}.vm < ${BENCH}/draw.fs > ${J}.vm.out 2>&1
                COMMAND           check-${NAME} display ${J}.vm ${J}.mem ${J}.err draw "${ON}" text fills scrolls
                                    > ${J}.txt 2> ${J}.check
                COMMAND           ${CMAKE_COMMAND} -E rm -f ${J}.mem ${J}.vm
                DEPENDS           ${WANTS} hexaforth-${NAME} bench/draw.fs
                VERBATIM)
        set_property(GLOBAL APPEND PROPERTY DRAW_FIGURES ${J}.txt)

        foreach(PIC ${PICTURES})
            foreach(SIZE ${PICTURE_SIZES})
                set(J ${BB}/dither-${PIC}-${SIZE}-${NAME}-${LAT})
                add_custom_command(
                        OUTPUT            ${J}.txt
                        COMMAND           ${CMAKE_COMMAND} -E cat ${BB}/blocks.fs ${BENCH}/tb-disk.fs ${BENCH}/dither.fs
                                            ${BB}/asks-${PIC}-${SIZE}.fs > ${J}.fs
                        COMMAND           ${T}/tb run ${NUC} ${BB}/disk.img ${J}.mem < ${J}.fs > ${J}.out 2> ${J}.err
                        COMMAND           check-${NAME} picture ${PICTURE} ${SIZE} ${J}.mem ${J}.out ${J}.err
                                            "dither ${PIC} ${SIZE}" "${ON}" > ${J}.txt 2> ${J}.check
                        COMMAND           ${CMAKE_COMMAND} -E rm -f ${J}.mem
                        DEPENDS           ${WANTS} ${BB}/blocks.fs bench/tb-disk.fs bench/dither.fs picture ${PICTURE}
                        VERBATIM)
                set_property(GLOBAL APPEND PROPERTY DITHER_FIGURES ${J}.txt)
            endforeach()
        endforeach()
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
    foreach(PIC ${PICTURES})
        set(K ${BB}/costs-${NAME}-${PIC})
        add_custom_command(
                OUTPUT            ${K}/work
                COMMAND           ${CMAKE_COMMAND} -E make_directory ${K}
                COMMAND           ${CMAKE_COMMAND} -E cat ${BB}/vm-disk.fs ${BB}/blocks.fs ${BENCH}/dither.fs
                                    ${BB}/asks-${PIC}-half.fs > ${K}/typed.fs
                COMMAND           ${CMAKE_COMMAND} -E env RUN_WORK=${K}/work
                                    $<TARGET_FILE:counting-${NAME}> ${NUC} ${BB}/disk.img < ${K}/typed.fs > ${K}/typed.out 2>&1
                DEPENDS           counting-${NAME} machine-${NAME} ${NUC} ${BB}/disk.img ${BB}/blocks.fs
                                  bench/dither.fs picture ${PICTURE}
                VERBATIM)
        list(APPEND WORKS ${K}/work)
        list(APPEND COSTED COMMAND ${CMAKE_COMMAND} -E echo "dither ${PIC} half, cells of ${CELL} bits:"
                           COMMAND costs-${NAME} ${NUC} ${BUILD}/${NAME}/nuc.tables ${BUILD}/${NAME}/nuc.sizes ${K}/work ${K}
                           COMMAND shorter-${NAME} ${NUC} ${BUILD}/${NAME}/nuc.tables ${K}/work ${K}/shorter.txt)
    endforeach()
    add_custom_target(dither-costs-${NAME} ${COSTED} DEPENDS ${WORKS} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_dependencies(dither-costs-${NAME} costs-${NAME} shorter-${NAME})
endfunction()
file(WRITE ${BB}/vm-disk.fs "vmdisk-init\n")       # the reference VM's disk, in place of the test bench's
foreach(NAME ${MACHINES})
    bench_on(${NAME})
endforeach()
get_property(BENCH_PARTS GLOBAL PROPERTY BENCH_PARTS)
add_custom_target(bench-parts DEPENDS ${BB}/disk.img ${BB}/blocks.fs ${BENCH_PARTS})

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

    set(M68K_ASSEMBLED ${M68K_AS} -m68000 --register-prefix-optional)
    foreach(PART atk gray)
        add_custom_command(
                OUTPUT            ${MB}/${PART}.o
                COMMAND           ${CMAKE_COMMAND} -E make_directory ${MB}
                COMMAND           ${M68K_ASSEMBLED} -o ${MB}/${PART}.o ${BENCH}/m68k/${PART}.s
                DEPENDS           bench/m68k/${PART}.s)
    endforeach()
    foreach(PIC ${PICTURES})
        foreach(SIZE ${PICTURE_SIZES})
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
# targets that print are of those.
get_property(DRAW_FIGURES GLOBAL PROPERTY DRAW_FIGURES)
get_property(DITHER_FIGURES GLOBAL PROPERTY DITHER_FIGURES)
add_custom_target(draw-figures DEPENDS ${DRAW_FIGURES})
add_custom_target(dither-figures DEPENDS ${DITHER_FIGURES})
add_custom_target(m68k-figures DEPENDS ${M68K_FIGURES})
add_dependencies(draw-figures bench-parts)
add_dependencies(dither-figures bench-parts)
function(figures TARGET)                                # and the kinds it prints
    set(PRINTED)
    foreach(KIND ${ARGN})
        list(APPEND PRINTED ${${KIND}_FIGURES})
    endforeach()
    add_custom_target(${TARGET} COMMAND check-${FIRST} table ${PRINTED})
    foreach(KIND ${ARGN})
        string(TOLOWER ${KIND} OF)
        add_dependencies(${TARGET} ${OF}-figures)
    endforeach()
endfunction()
figures(bench-draw DRAW)
figures(bench-dither DITHER)
figures(bench-m68k M68K)
figures(bench DRAW DITHER M68K)

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
    add_custom_target(gates ${SAID} DEPENDS ${COUNTED})
    add_dependencies(gates tv80 fx68k)
else()
    add_custom_target(gates COMMAND ${CMAKE_COMMAND} -E echo "gates: not run: there is no yosys")
endif()

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
