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
#                       which the part that wants it fetches
#   WITHOUT name        it is needed only where that other need is not here
#                       and no package has it: it is what the other is built
#                       from
function(need NAME)
    cmake_parse_arguments(IT "BUILT_HERE" "HEADER;FOR;BREW;APT;WITHOUT" "PROGRAM" ${ARGN})
    set_property(GLOBAL APPEND PROPERTY NEEDED ${NAME})
    foreach(PART PROGRAM HEADER FOR BREW APT BUILT_HERE WITHOUT)
        set_property(GLOBAL PROPERTY NEEDED_${NAME}_${PART} "${IT_${PART}}")
    endforeach()
endfunction()

need(C_COMPILER     PROGRAM cc gcc clang        APT gcc
     FOR "everything: the C is compiled with it, and gforth builds util/fuse.h with it")
need(CXX_COMPILER   PROGRAM c++ g++ clang++     APT g++
     FOR "the Verilog: Verilator makes C++ of it")
need(MAKE_PROGRAM   PROGRAM make gmake          APT make                        BREW make
     FOR "everything: CMake builds with it, and so does Verilator")
need(GIT            PROGRAM git                 APT git                         BREW git
     FOR "bench-m68k and gates, which fetch the 68000 and the two other CPUs")
need(CURL           PROGRAM curl                APT curl                        BREW curl
     FOR "the Mandrill, which the tests that have a disk are given")
need(GFORTH         PROGRAM gforth              APT gforth                      BREW gforth
     FOR "every image: cross.fs is run by it")
need(LIBTOOL        PROGRAM glibtool libtool    APT libtool-bin                 BREW libtool
     FOR "every image: gforth builds util/fuse.h through it")
need(LTDL_HEADER    HEADER ltdl.h               APT libltdl-dev                 BREW libtool
     FOR "every image: what gforth builds of util/fuse.h is built with it")
need(UNICORN_HEADER HEADER unicorn/unicorn.h    APT libunicorn-dev              BREW unicorn
     FOR "the tests kernel-hexaforth and kernel-hexaforth-fast: the kernels are run against vm.c on it")
need(CLANG          PROGRAM clang               APT clang                       BREW llvm
     FOR "the AArch64 kernels, which it assembles")
need(LLD            PROGRAM ld.lld              APT lld                         BREW lld
     FOR "the AArch64 kernels, which it links")
need(OBJCOPY        PROGRAM llvm-objcopy        APT llvm                        BREW llvm
     FOR "the AArch64 kernels, as what QEMU's machine has in memory")
need(NM             PROGRAM llvm-nm             APT llvm                        BREW llvm
     FOR "the AArch64 kernels: where a kernel's symbols are")
need(QEMU_AARCH64   PROGRAM qemu-system-aarch64 APT qemu-system-arm             BREW qemu
     FOR "the desktop on a kernel: the qemu- tests, run, demo, demo-test and demo-time")
need(VERILATOR      PROGRAM verilator           APT verilator                   BREW verilator
     FOR "the Verilog: the verilog- tests and the benchmarks")
need(YOSYS          PROGRAM yosys               APT yosys                       BREW yosys
     FOR "gates and synth")
need(NEXTPNR        PROGRAM nextpnr-ecp5        APT nextpnr-ecp5                BUILT_HERE
     FOR "synth")
need(TRELLIS        PROGRAM ecppack             BREW prjtrellis                 WITHOUT NEXTPNR
     FOR "synth, where nextpnr-ecp5 is built here: the ECP5's database, which its own is made from")
need(BOOST_HEADER   HEADER boost/version.hpp    BREW boost                      WITHOUT NEXTPNR
     FOR "synth, where nextpnr-ecp5 is built here: it is built with it")
need(EIGEN_HEADER   HEADER eigen3/Eigen/Core    BREW eigen                      WITHOUT NEXTPNR
     FOR "synth, where nextpnr-ecp5 is built here: it is built with it")
need(CLANGD         PROGRAM clangd              APT clangd                      BREW llvm
     FOR "the target clangd: what the language server says of the C")
need(M68K_AS        PROGRAM m68k-elf-as m68k-linux-gnu-as           APT binutils-m68k-linux-gnu BREW m68k-elf-binutils
     FOR "bench-m68k: the 68000's assembler")
need(M68K_LD        PROGRAM m68k-elf-ld m68k-linux-gnu-ld           APT binutils-m68k-linux-gnu BREW m68k-elf-binutils
     FOR "bench-m68k: the 68000's linker")
need(M68K_OBJCOPY   PROGRAM m68k-elf-objcopy m68k-linux-gnu-objcopy APT binutils-m68k-linux-gnu BREW m68k-elf-binutils
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
        set(INSTALLED ${AS_ROOT} ${WITH} DEBIAN_FRONTEND=noninteractive ${APT} install -y ${ASKED})
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
