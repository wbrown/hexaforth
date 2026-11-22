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

# The CPU with a plain memory and its test bench, in a directory (tb): of
# a cell, the log2 of its memory's cells, and LAT.
function(verilog_cpu B CELL ADDR LAT)
    add_custom_command(
            OUTPUT            ${B}/tb
            COMMAND           ${MKDIR} ${B}
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
            COMMAND           ${MKDIR} ${RB}
            COMMAND           difftest_dump ${RB}/cases-${CELL}.bin ${CELL}
            DEPENDS           difftest_dump)
    add_custom_command(
            OUTPUT            ${RB}/session-${CELL}.out
            COMMAND           ${MKDIR} ${RB}
            COMMAND           hexaforth ${BUILD}/${M}/nuc.hex < ${RTL}/session.fs > ${RB}/session-${CELL}.out
            DEPENDS           hexaforth machine-${M} ${BUILD}/${M}/nuc.hex rtl/session.fs)
    add_custom_target(verilog-${CELL} DEPENDS ${RB}/cases-${CELL}.bin ${RB}/session-${CELL}.out)
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
            COMMAND           ${MKDIR} ${B}
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
        COMMAND           ${MKDIR} ${RB}/c64
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
        set(NB ${BUILD}/nextpnr)
        if(POLICY CMP0225)              # nothing of it is installed, and where it would be is left unsaid, as it was
            cmake_policy(SET CMP0225 OLD)
        endif()
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
        COMMAND           ${MKDIR} ${S}
        ${SYNTH_ROM}
        COMMAND           ${YOSYS} -q -l ${S}/yosys.log -p
                            "${SYNTH_READ} soc.v hexaforth.v hf_mem.v; chparam -set W ${SYNTH_CELL} -set AB ${SYNTH_AB} -set LAT ${SYNTH_LAT} soc; synth_ecp5 -top soc -json ${S}/soc.json"
        COMMAND           ${NEXTPNR} --85k --package CABGA381 --json ${S}/soc.json --lpf-allow-unconstrained
                            --timing-allow-fail -q -l ${S}/nextpnr.log || ${CMAKE_COMMAND} -E true
        COMMAND           synth_said ${S}/nextpnr.log
                            "W=${SYNTH_CELL} LAT=${SYNTH_LAT} KROM=${SYNTH_IS} memory 2^${SYNTH_AB} cells:"
                            > ${S}/result.txt
        COMMAND           ${CAT} ${S}/result.txt
        COMMAND           ${KEEP} synth ${RTL}/hexaforth.v ${S}/result.txt ${S}/nextpnr.log
        WORKING_DIRECTORY ${RTL}
        VERBATIM)
add_dependencies(synth machine-default-${SYNTH_CELL} ${NEXTPNR_BUILT})
