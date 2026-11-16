#!/bin/sh
# What the machine costs to do its work, in clocks.
#
#   bench/bench.sh [draw] [dither] [m68k] [gates]       (the first three, if none is named)
#
# Runtime is clocks, and the Verilog CPU is what has them (rtl/tb.cpp), so
# the work is done there: on the machine with cells of 64 bits, of 32 and
# of 16 (util/machines says what each is), with the memory read a cycle
# after it is asked for and two (LAT).  A piece of work is a file that is
# typed at the nucleus's prompt, the same file on every machine:
#
#   draw     draw.fs: text, fills and scrolls.  What it leaves on the
#            display is compared with what the reference VM leaves there
#            when the same is typed at it
#   dither   dither.fs: the Mandrill read from a disk a block at a time, as
#            the system reads it, and dithered, from its three samples and
#            from its grays, at its own size and at half of it.  Each
#            bitmap is compared with the dither a pixel at a time
#            (test/dither.c).  A machine that has not the memory for one
#            says so, and that is what is printed
#
# and two things are there to compare the machine with:
#
#   m68k     the same dither on a 68000, written for it (m68k/), with the
#            picture already in its memory, and its bitmap compared with
#            the same dither a pixel at a time.  It wants the 68000's
#            assembler (m68k-elf-as) and fetches Musashi, which is the
#            68000
#   gates    how big the CPU is beside a Z80 and a 68000 (gates.sh)
#
# Nothing is printed for work that was not checked.  Everything is kept in
# build/bench/; a line a result is printed, and the exit status is 0 only
# if every check passed.  No figure is kept in the repository: what a
# change costs is what this prints before it and after.
cd "$(dirname "$0")/.."
OUT=build/bench
mkdir -p $OUT
WHAT=${*:-draw dither m68k}
PICTURE=baremetal/build/mandrill.tiff       # the Mandrill, which baremetal/Makefile fetches,
MKDISK=baremetal/build/mkdisk               # and what makes a disk, which it builds
fail=0

wants() { case " $WHAT " in (*" $1 "*) return 0;; esac; return 1; }

say() { # what, on which machine, result
    printf '%-22s %-17s %s\n' "$1" "$2" "$3"
}

# What tb said passed between two marks: the cycles, and the rest.
between() { # the first mark, tb's errors
    sed -n "s/^mark $1 to $(($1 + 1)): //p" "$2"
}

if wants draw || wants dither; then
    # The disk: the system's, and the picture's grays as a file after it.
    make -C baremetal build/mandrill.tiff build/mkdisk > $OUT/picture.log 2>&1
    gcc -O2 -w -o $OUT/disk bench/disk.c bench/tiff.c test/dither.c
    if ! { $MKDISK $PICTURE $OUT/system.img &&
           $OUT/disk $PICTURE $OUT/system.img $OUT/disk.img $OUT/blocks.fs; } > $OUT/disk.log 2>&1; then
        say disk "" "FAIL $(tail -1 $OUT/disk.log)"
        exit 1
    fi

    while read C A W H BASE <&3; do
        case $C in (\#*) continue;; esac
        # The machine: its nucleus, the reference VM, the Verilog CPU with
        # as much memory, and what checks a bitmap of its kind.
        if ! util/build-machine.sh $C $A $W $H $BASE > $OUT/build-$C.log 2>&1; then
            say build "$C bits" "FAIL $(tail -1 $OUT/build-$C.log)"
            fail=1
            continue
        fi
        mv build/nuc-$C-$A.hex $OUT/nuc-$C.hex
        mv build/hexaforth-$C-$A $OUT/vm-$C
        gcc -O2 -w $(cat build/machine-$C-$A.flags) -o $OUT/check-$C bench/check.c bench/tiff.c test/dither.c
        for LAT in 1 2; do
            T=$OUT/tb-$C-$LAT
            if ! make -C rtl W=$C AB=$A LAT=$LAT B=../$T ../$T/tb > $OUT/tb-$C-$LAT.log 2>&1; then
                say build "$C bits LAT=$LAT" "FAIL $(tail -1 $OUT/tb-$C-$LAT.log)"
                fail=1
                continue
            fi
            ON="$C bits LAT=$LAT"

            if wants draw; then
                J=$OUT/draw-$C-$LAT
                $T/tb run $OUT/nuc-$C.hex $OUT/disk.img $J.mem < bench/draw.fs > $J.out 2> $J.err
                $OUT/vm-$C $OUT/nuc-$C.hex $OUT/disk.img $J.vm < bench/draw.fs > $J.vm.out 2>&1
                if $OUT/check-$C $J.vm $J.mem > $J.check 2>&1; then
                    say "draw text" "$ON" "$(between 1 $J.err)"
                    say "draw fills" "$ON" "$(between 2 $J.err)"
                    say "draw scrolls" "$ON" "$(between 3 $J.err)"
                else
                    say draw "$ON" "FAIL $(tail -1 $J.check)"
                    fail=1
                fi
                rm -f $J.mem $J.vm
            fi

            if wants dither; then
                for PIC in gray colour; do
                    for SIZE in own half; do
                        J=$OUT/dither-$PIC-$SIZE-$C-$LAT
                        { cat $OUT/blocks.fs bench/dither.fs
                          echo "mark $PIC-picture $SIZE mark"
                          echo "b @ . b 1+ @ ."
                          echo "8000 execute"
                        } > $J.fs
                        $T/tb run $OUT/nuc-$C.hex $OUT/disk.img $J.mem < $J.fs > $J.out 2> $J.err
                        if grep -aq 'error: ' $J.out; then
                            say "dither $PIC $SIZE" "$ON" "the machine could not: $(grep -ao 'error: [-0-9]*' $J.out | head -1)"
                        elif $OUT/check-$C $PICTURE $SIZE $J.mem $J.out > $J.check 2>&1; then
                            say "dither $PIC $SIZE" "$ON" "$(between 1 $J.err)"
                        else
                            say "dither $PIC $SIZE" "$ON" "FAIL $(tail -1 $J.check)"
                            fail=1
                        fi
                        rm -f $J.mem
                    done
                done
            fi
        done
    done 3< util/machines
fi

if wants m68k; then
    make -C baremetal build/mandrill.tiff > $OUT/picture.log 2>&1
    if ! command -v m68k-elf-as > /dev/null 2>&1; then
        say m68k 68000 "not run: there is no m68k-elf-as"
    else
        # Musashi, as it was when this was written.
        M=$OUT/Musashi
        if [ ! -d $M ]; then
            git clone -q https://github.com/kstenerud/Musashi $M > $OUT/musashi.log 2>&1 &&
                git -C $M checkout -q 313ebf1bd9f4d0d93341eb5ce21fd8a119e9dbdd >> $OUT/musashi.log 2>&1
        fi
        mkdir -p $OUT/m68k
        AT=0x1000                           # where the program is, in the 68000's memory
        if ! { gcc -O1 -w -o $OUT/m68k/m68kmake $M/m68kmake.c &&
               $OUT/m68k/m68kmake $OUT/m68k $M/m68k_in.c &&
               gcc -O2 -w -I$M -I$OUT/m68k -o $OUT/m68k/run68k bench/m68k/run68k.c bench/tiff.c test/dither.c \
                   $M/m68kcpu.c $OUT/m68k/m68kops.c $M/softfloat/softfloat.c -lm &&
               m68k-elf-as -m68000 --register-prefix-optional -o $OUT/m68k/atk.o bench/m68k/atk.s &&
               m68k-elf-as -m68000 --register-prefix-optional -o $OUT/m68k/gray.o bench/m68k/gray.s
             } > $OUT/m68k/build.log 2>&1; then
            say m68k 68000 "FAIL $(tail -1 $OUT/m68k/build.log)"
            fail=1
        else
            for PIC in gray colour; do
                for SIZE in own half; do
                    J=$OUT/m68k/$PIC-$SIZE
                    case $PIC in (gray) SAMPLES=1;; (*) SAMPLES=3;; esac
                    case $SIZE in (half) HALF=1;; (*) HALF=0;; esac
                    if m68k-elf-as -m68000 --register-prefix-optional --defsym SAMPLES=$SAMPLES --defsym HALF=$HALF \
                           -o $J.o bench/m68k/glue.s > $J.log 2>&1 &&
                       m68k-elf-ld -Ttext=$AT -e _start -o $J.elf $J.o $OUT/m68k/atk.o $OUT/m68k/gray.o >> $J.log 2>&1 &&
                       m68k-elf-objcopy -O binary $J.elf $J.bin >> $J.log 2>&1 &&
                       $OUT/m68k/run68k $J.bin $AT $PICTURE $PIC $SIZE >> $J.log 2>&1; then
                        say "dither $PIC $SIZE" 68000 "$(tail -1 $J.log)"
                    else
                        say "dither $PIC $SIZE" 68000 "FAIL $(tail -1 $J.log)"
                        fail=1
                    fi
                done
            done
        fi
    fi
fi

if wants gates; then
    sh bench/gates.sh || fail=1
fi
exit $fail
