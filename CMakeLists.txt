cmake_minimum_required(VERSION 3.18)

#
# How everything here is built, tested and measured.  This file names the
# project, and says where what is made goes and how the C is compiled.  The
# rest is in the parts it reads, in cmake/, in the order they are read: a
# part says at its top what it is of, and uses what the parts before it
# say.  They are all the one directory to CMake, so a target or a file that
# one part makes is named as it is in any part after it.
#

# What CMake does itself where a command is wanted, each by a word.
set(MKDIR  ${CMAKE_COMMAND} -E make_directory)      # a directory, and those it is in, if they are not there
set(CAT    ${CMAKE_COMMAND} -E cat)                 # files, one after another, printed
set(ECHO   ${CMAKE_COMMAND} -E echo)                # a line, printed
set(WITH   ${CMAKE_COMMAND} -E env)                 # a command, with what its environment has said before it
set(COPY   ${CMAKE_COMMAND} -E copy)                # a file, as another
set(REMOVE ${CMAKE_COMMAND} -E rm -f)               # files, taken away if they are there

include(cmake/needs.cmake)              # what the project needs that is not in it: before it is named

project(hexaforth C)

# What is made is in build/, which is not checked in: what cross.fs writes,
# there and in a directory of it for each machine, and all that is made of
# that.  The C is built where CMake is told to build, which is build/cmake
# wherever it is said here.
set(BUILD ${CMAKE_SOURCE_DIR}/build)

# How each C file is compiled is written out when CMake is run
# (compile_commands.json, where CMake builds), for a language server:
# clangd reads it, and so knows a file's definitions and headers.  clangd
# looks for it in build/ under each directory above a source file, so a
# link to it is put there, wherever CMake was told to build.  Only what is
# compiled as a target here is in it: not what gforth builds of
# util/fuse.h, the kernels or the Verilog.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BINARY_DIR STREQUAL "${BUILD}")
    file(MAKE_DIRECTORY ${BUILD})
    file(CREATE_LINK ${CMAKE_BINARY_DIR}/compile_commands.json
                     ${BUILD}/compile_commands.json SYMBOLIC)
endif()

set(CMAKE_C_STANDARD 99)

# Built as a release unless it is said otherwise: built with no flags, the
# tests do not link.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# What the compiler made on the way, the assembly among it, is kept beside
# each object: a source is compiled for each tool that has it, often at
# once, and kept where it was compiled from they would be written over each
# other.
set(CMAKE_C_FLAGS_RELEASE "-save-temps=obj -O2 -fverbose-asm -fcommon")
set(CMAKE_C_FLAGS_DEBUG "-save-temps=obj -g -fverbose-asm -fcommon")

include(cmake/kinds.cmake)              # the kinds of thing that are made, each said once
include(cmake/default-machine.cmake)    # the generated Forth, the default machine's tests, and the tests that need no image
include(cmake/disks.cmake)              # what makes a disk, the source that goes on one, and the pictures
include(cmake/machines.cmake)           # the machines of util/machines: the images, disks, tests and reports of each
include(cmake/reports.cmake)            # the reports of the first machine
include(cmake/glossary.cmake)           # the glossary of the words, generated from the source
include(cmake/kernels.cmake)            # the AArch64 kernels, and the desktop on QEMU
include(cmake/verilog.cmake)            # the Verilog, its tests, and synthesis
include(cmake/bench.cmake)              # the benchmarks
include(cmake/gates.cmake)              # how big the CPU is beside two others
include(cmake/said.cmake)               # the files and the figures the tree's text says
include(cmake/clangd.cmake)             # what the language server says of the C
include(cmake/check.cmake)              # check and quick, which run the tests: last, being of every test and every tool
