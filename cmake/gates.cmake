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
# It wants yosys.  Everything is kept in build/bench/gates/: BB and BENCH
# are the benchmarks' (cmake/bench.cmake), and RTL the Verilog's.
#
set(GB ${BB}/gates)
if(YOSYS)
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

    source_fetched(tv80 https://github.com/hutch31/tv80 66a131c38d05ef58b3d8c4f1507a72e6e4aa5d65 ${GB}/tv80)
    gates(z80-tv80 IN ${GB}/tv80/rtl/core TOP tv80s
          READ "read_verilog tv80s.v tv80_core.v tv80_alu.v tv80_mcode.v tv80_reg.v"
          MEMORIES "t:$mem_v2" AFTER tv80)

    source_fetched(fx68k https://github.com/ijor/fx68k 0602ee4627b10f301298f2673d826cdd6baa9327 ${GB}/fx68k)
    set(PACKED ${GB}/fx68k-packed)
    add_custom_command(
            OUTPUT            ${PACKED}/fx68k.sv
            COMMAND           ${MKDIR} ${PACKED}
            COMMAND           ${COPY} ${GB}/fx68k/fx68kAlu.sv ${GB}/fx68k/uaddrPla.sv
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
else()
    add_custom_target(gates COMMAND ${ECHO} "gates: not run: there is no yosys")
endif()
