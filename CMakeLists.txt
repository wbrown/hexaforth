cmake_minimum_required(VERSION 3.17)
project(hexaforth C)

set(DEBUG true)
set(CMAKE_C_STANDARD 99)

set(CMAKE_C_FLAGS_RELEASE "-save-temps -O2 -fverbose-asm -fcommon")
set(CMAKE_C_FLAGS_DEBUG "-save-temps -g -fverbose-asm -fcommon")

if(DEBUG)
   add_compile_definitions(DEBUG)
endif()

# hexaforth nucleus image from swapforth
add_custom_command(
        COMMAND           gforth cross.fs basewords.fs nuc.fs
        OUTPUT            ${CMAKE_SOURCE_DIR}/build/nuc.hex
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/forth
        DEPENDS           forth/cross.fs
                          forth/basewords.fs
                          forth/nuc.fs
                          basewords-fs-gen)

# adhoc test
add_custom_command(
        COMMAND           gforth cross.fs basewords.fs test.fs
        OUTPUT            ${CMAKE_SOURCE_DIR}/build/test.hex
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/forth
        DEPENDS           forth/cross.fs
                          forth/basewords.fs
                          forth/test.fs)

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
        COMMAND           gforth cross.fs basewords.fs test_cases.fs
        OUTPUT            ${CMAKE_SOURCE_DIR}/build/test_cases.hex
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/forth
        DEPENDS           forth/test_cases.fs
                          forth/cross.fs
                          forth/basewords.fs)

# kick off our entire forth environment
add_custom_target(forth-env
        ALL
        DEPENDS           ${CMAKE_SOURCE_DIR}/build/nuc.hex
                          ${CMAKE_SOURCE_DIR}/build/test.hex)

# VM itself
add_library(vm_core
        vm.c
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
        build/nuc.hex
        build/test.hex)
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
        build/nuc.hex
        build/test.hex)
target_link_libraries(hexaforth-debug
        vm_core_debug)
target_compile_definitions(hexaforth-debug
        PUBLIC
        DEBUG)
endif()
