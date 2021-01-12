cmake_minimum_required(VERSION 3.17)
project(hexaforth C)

set(DEBUG false)
set(CMAKE_C_STANDARD 99)

add_compile_options(-save-temps -g -fverbose-asm -fcommon)

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
                          forth/nuc.fs)

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

# kick off our entire forth environment
add_custom_target(forth-env
        ALL
        DEPENDS           ${CMAKE_SOURCE_DIR}/build/nuc.hex)

# VM itself
add_library(vm_core
        vm.c
        vm.h
        vm_opcodes.h
        vm_constants.h)

add_library(vm_debug
        vm_debug.c
        vm_debug.h)

# VM tests
add_executable(hexaforth-test
        vm_opcodes.c
        test/main.c
        test/vm_test.c
        test/vm_test.h
        test/compiler.c
        test/compiler.h)
target_link_libraries(hexaforth-test
        vm_core
        vm_debug)
target_compile_definitions(hexaforth-test
        PUBLIC
        TEST)

add_custom_target(tests
        ALL
        COMMAND           ${CMAKE_BINARY_DIR}/hexaforth-test
        DEPENDS           hexaforth-test)

# Main executable
add_executable(hexaforth
        main.c
        vm_opcodes.c
        vm_opcodes.h
        build/nuc.hex)
target_link_libraries(hexaforth
        vm_core)
if(DEBUG)
    target_link_libraries(hexaforth
            vm_debug)
endif()