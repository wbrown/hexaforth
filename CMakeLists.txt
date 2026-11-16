cmake_minimum_required(VERSION 3.17)
project(hexaforth C)

set(DEBUG true)
set(CMAKE_C_STANDARD 99)

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
set(MACHINE_SOURCES util/fuse.c vm.c vm_step.h vm.h vm_opcodes.c vm_opcodes.h
                    vm_instruction.h vm_constants.h util/stack.c)
set(GFORTH gforth)
find_program(BREW brew)
if(BREW)
    execute_process(COMMAND ${BREW} --prefix
                    OUTPUT_VARIABLE BREW_PREFIX OUTPUT_STRIP_TRAILING_WHITESPACE)
    set(BREW_CPATH ${BREW_PREFIX}/include)
    if(DEFINED ENV{CPATH})
        set(BREW_CPATH ${BREW_CPATH}:$ENV{CPATH})
    endif()
    set(GFORTH ${CMAKE_COMMAND} -E env CPATH=${BREW_CPATH} gforth)
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
add_custom_command(
        COMMAND           ${CMAKE_BINARY_DIR}/generate-test-cases
        OUTPUT            ${CMAKE_SOURCE_DIR}/forth/test_cases.fs
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

add_custom_target(tests
        ALL
        COMMAND           ${CMAKE_BINARY_DIR}/hexaforth_test 2> hexaforth_test.debug
        DEPENDS           hexaforth_test
                          build/test_cases.hex)

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
