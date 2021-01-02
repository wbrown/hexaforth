cmake_minimum_required(VERSION 3.17)
project(vliw_test C)

set(CMAKE_C_STANDARD 99)

add_executable(hexaforth
        main.c
        compiler.c
        vm_opcodes.h
        vm_constants.h
        vm.h
        vm_debug.c
        vm_debug.h
        vm_opcodes.c
        vm.c
        compiler.h vm_test.c vm_test.h)