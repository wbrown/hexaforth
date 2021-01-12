cmake_minimum_required(VERSION 3.17)
project(hexaforth C)

set(CMAKE_C_STANDARD 99)

# hexaforth nucleus image from swapforth
add_custom_command(
        COMMAND           gforth cross.fs basewords.fs nuc.fs
        OUTPUT            ${CMAKE_SOURCE_DIR}/build/nuc.hex
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/forth
        DEPENDS           forth/cross.fs
                          forth/basewords.fs
                          forth/nuc.fs)

# basewords.fs opcode description for forth environment
add_custom_command(
        COMMAND           ${CMAKE_BINARY_DIR}/basewords-fs-gen
                            ${CMAKE_SOURCE_DIR}/forth/basewords.fs
        OUTPUT            ${CMAKE_SOURCE_DIR}/forth/basewords.fs
        DEPENDS           basewords-fs-gen)

add_executable(basewords-fs-gen
        baseword_fs_gen.c
        vm_opcodes.c
        vm_opcodes.h
        vm.h
        vm_debug.c
        vm_debug.h)

# kick off our entire forth environment
add_custom_target(forth-env
        ALL
        DEPENDS           ${CMAKE_SOURCE_DIR}/build/nuc.hex)

# VM tests
add_executable(hexaforth-test
        test/main.c
        test/vm_test.c
        test/vm_test.h
        test/compiler.c
        test/compiler.h
        vm_opcodes.h
        vm_opcodes.c
        vm_constants.h
        vm.h
        vm_debug.c
        vm_debug.h
        vm.c)

add_custom_target(tests
        ALL
        COMMAND           ${CMAKE_BINARY_DIR}/hexaforth-test
        DEPENDS           hexaforth-test)

# VM itself
add_executable(hexaforth
        main.c
        vm.c
        vm.h
        vm_opcodes.h
        vm_constants.h
        vm_debug.c
        vm_debug.h
        vm_opcodes.c
        build/nuc.hex)