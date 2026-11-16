#!/bin/sh
# How big the CPU is, beside a Z80 and a 68000.
#
#   bench/gates.sh
#
# Each is put through one flow of Yosys, to NAND gates of two inputs,
# inverters and D flip-flops, and what it comes to is given as gates: a
# NAND is one, an inverter half of one and a flip-flop four.  A memory that
# is a register file is made flip-flops, and one that is a ROM or a stack
# is left a memory and said, not counted.  The CPU is rtl/hexaforth.v with
# cells of 16 bits, of 32 and of 64, and each again without its
# multiplier, which the other two have none of.
#
# The other two are not kept here.  They are fetched, as they were when
# this was written: TV80 for the Z80, and fx68k for the 68000, whose
# microcode is the ROMs that are left out.  fx68k's structures are made
# packed for Yosys to read, which changes nothing they are.
#
# It wants yosys.  Everything is kept in build/bench/gates/.
cd "$(dirname "$0")/.."
OUT=$(pwd)/build/bench/gates
mkdir -p $OUT
if ! command -v yosys > /dev/null 2>&1; then
    echo "gates: not run: there is no yosys"
    exit 0
fi

fetch() { # directory, where from, which commit
    [ -d $OUT/$1 ] && return 0
    git clone -q $2 $OUT/$1 > $OUT/$1.log 2>&1 && git -C $OUT/$1 checkout -q $3 >> $OUT/$1.log 2>&1
}

# One flow for every CPU.
count() { # label, directory, top, what reads it, memories to make flip-flops, "nomul" to leave multipliers out
    if [ "$6" = nomul ]; then MAP='techmap t:$mul %n'; else MAP='techmap'; fi
    if [ -n "$5" ]; then MEMORIES="memory_map $5"; else MEMORIES=""; fi
    if ! (cd "$2" && yosys -q -l "$OUT/$1.log" -p "
        $4
        hierarchy -top $3
        proc -norom
        synth -top $3 -flatten -noalumacc -run :fine
        $MEMORIES
        opt -fast -full
        $MAP
        opt -fast
        abc -g NAND
        opt -fast
        dfflegalize -cell \$_DFF_P_ 01 -cell \$_DFF_PN0_ 01 -cell \$_DFF_PN1_ 01 -cell \$_DFF_PP0_ 01 -cell \$_DFF_PP1_ 01
        abc -g NAND
        opt_clean
        tee -o $OUT/$1.stat stat
    ") > /dev/null 2>&1; then
        printf '%-26s FAIL %s\n' "$1" "$(grep -m1 ERROR "$OUT/$1.log")"
        return 1
    fi
    awk -v label="$1" '
        /\$_NAND_/ { nand = $1 }  /\$_NOT_/ { not = $1 }  /\$_DFF_/ { ff += $1 }
        /\$mem_v2/ { memories = $1 }  /\$mul/ { multipliers = $1 }
        END { printf "%-26s %6d gates: NAND %6d  NOT %6d  flip-flops %5d;  memories left out %d%s\n",
                     label, (4 * nand + 2 * not + 16 * ff) / 4, nand, not, ff, memories,
                     (multipliers ? ", multipliers left out " multipliers : "") }' "$OUT/$1.stat"
}

fail=0
for W in 16 32 64; do
    READ="read_verilog hexaforth.v; chparam -set W $W -set LAT 1 hexaforth"
    count "hexaforth-$W" rtl hexaforth "$READ" "" || fail=1
    count "hexaforth-$W-no-multiplier" rtl hexaforth "$READ" "" nomul || fail=1
done

if fetch tv80 https://github.com/hutch31/tv80 66a131c38d05ef58b3d8c4f1507a72e6e4aa5d65; then
    count z80-tv80 $OUT/tv80/rtl/core tv80s \
        "read_verilog tv80s.v tv80_core.v tv80_alu.v tv80_mcode.v tv80_reg.v" 't:$mem_v2' || fail=1
else
    echo "z80-tv80: FAIL could not be fetched"
    fail=1
fi

if fetch fx68k https://github.com/ijor/fx68k 0602ee4627b10f301298f2673d826cdd6baa9327; then
    mkdir -p $OUT/fx68k-packed
    cp $OUT/fx68k/fx68kAlu.sv $OUT/fx68k/uaddrPla.sv $OUT/fx68k/microrom.mem $OUT/fx68k/nanorom.mem $OUT/fx68k-packed/
    sed 's/typedef struct {/typedef struct packed {/' $OUT/fx68k/fx68k.sv > $OUT/fx68k-packed/fx68k.sv
    count m68000-fx68k $OUT/fx68k-packed fx68k \
        "read_verilog -sv fx68k.sv fx68kAlu.sv uaddrPla.sv" "fx68k/excUnit.regs68L fx68k/excUnit.regs68H" || fail=1
else
    echo "m68000-fx68k: FAIL could not be fetched"
    fail=1
fi
exit $fail
