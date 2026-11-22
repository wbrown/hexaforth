#
# The benchmarks: what the machine costs to do its work, in clocks.
#
# Runtime is clocks, and the Verilog CPU is what has them (rtl/tb.cpp), so
# the work is done there: on each machine of util/machines, with as much
# memory as it has, and with what the memory read back a cycle after it is
# asked for and two (LAT).  A piece of work is a file that is typed at the
# nucleus's prompt, the same file on every machine:
#
#   bench-draw      bench/draw.fs: text, fills and scrolls.  What it leaves
#                   on the display is compared with what the reference VM
#                   leaves there when the same is typed at it
#   bench-dither    bench/dither.fs: the Mandrill read from a disk a block
#                   at a time, as the system reads it, and dithered, from
#                   its three samples and from its grays, at its own size
#                   and at half of it, and from its half-size grays at their
#                   own size and at twice it.  Each bitmap is compared with the
#                   dither a pixel at a time (test/dither.c).  A machine
#                   that has not the memory for one says so, and that is
#                   what is printed
#   bench-gray      bench/gray.fs: keeping the Mandrill as gray in sixteen
#                   levels, one sample per 4 by 4 pixels (forth/gray.fs);
#                   loading the blue noise mask from the disk; drawing the
#                   whole picture through it at half size; and copying the
#                   bitmap to the display (copy-bits): a figure for each.
#                   The bitmap, and the display where it was copied, are
#                   compared with the same done a pixel at a time
#                   (test/gray.h)
#
# and two things are there to compare the machine with:
#
#   bench-m68k      the same dither on a 68000, written for it
#                   (bench/m68k/), with the picture already in its memory,
#                   and its bitmap compared with the same dither a pixel at
#                   a time.  It wants the 68000's assembler (m68k-elf-as),
#                   and fetches Musashi, which is the 68000
#   gates           how big the CPU is beside a Z80 and a 68000
#
#   bench           the first four
#
# A figure is printed by what checked the work it is of (bench/check.c),
# and nothing is printed for work that was not checked.  Everything is
# kept in build/bench/; a line a result is printed, and the target fails
# if a check did.  No figure is kept in the repository: what a change costs
# is what this prints before it and after.
#
set(BB ${BUILD}/bench)
set(BENCH ${CMAKE_SOURCE_DIR}/bench)

# The benchmarks' disks, each no more than a floppy: the system's, which
# the machine has as its first; the pictures' disk, which has the Mandrill;
# and a disk of the Mandrill's grays and its grays at half size, as
# one-sample TIFFs (GRAYS and SMALL, which are made with the pictures:
# cmake/disks.cmake).  A machine is given the three, and bench/pictures.fs,
# typed at it, finds each picture by its label.  It says a label with
# parse-name, which the nucleus has: the system with the picture alone has
# no s" (macros.fs).
add_custom_command(
        OUTPUT            ${BB}/system.img
        COMMAND           ${MKDIR} ${BB}
        COMMAND           mkdisk ${BB}/system.img > ${BB}/system-disk.log 2>&1
        DEPENDS           mkdisk)
add_custom_command(
        OUTPUT            ${BB}/grays.img
        COMMAND           ${MKDIR} ${BB}
        COMMAND           mkdisk --disk ${BB}/grays.img grays.tiff=${GRAYS} small.tiff=${SMALL}
                            > ${BB}/grays-disk.log 2>&1
        DEPENDS           mkdisk ${GRAYS} ${SMALL})
set(BENCH_DISK_FILES ${BB}/system.img ${PICTURES} ${BB}/grays.img)
set(BENCH_DISKS "${BB}/system.img,${PICTURES},${BB}/grays.img")    # as a machine is told its disks

# How often each cell of the dithered picture occurs, cut into cells of 16,
# 32 and 64 pixels (bench/patterns.c): the target cell-patterns prints it
# for the picture at its own size and at half, and keeps it in
# build/bench/patterns-own.txt and patterns-half.txt.
tool(patterns bench/patterns.c bench/tiff.c test/dither.c)
add_custom_target(cell-patterns
        COMMAND           ${MKDIR} ${BB}
        COMMAND           patterns ${PICTURE} own > ${BB}/patterns-own.txt
        COMMAND           patterns ${PICTURE} half > ${BB}/patterns-half.txt
        COMMAND           ${CAT} ${BB}/patterns-own.txt ${BB}/patterns-half.txt
        DEPENDS           picture ${PICTURE})

# Three ways a picture could be stored are tried on the Mandrill, Peppers
# and the F-16, each by a tool of bench/ named as its target is: the target
# NAME-report prints what the tool says of each picture, at each size where
# it takes one, and each is left as pictures in build/bench/NAME/, in a
# directory named for the picture and the size.
#
#   tiles-report    as a codebook of tiles and a tile's number for each
#                   place, drawn as a font is (bench/tiles.c): how small,
#                   and how near the dither, at its own size and at half
#   masks-report    kept as gray, in few bits and at less than its size,
#                   and drawn black and white through a threshold mask
#                   (bench/masks.c): how small, at the same two sizes
#   quads-report    one byte per 2 by 2 block, expanded back to full size
#                   and dithered (bench/quads.c): each way's size, and how
#                   far it is from the picture's grays
function(pictures_report NAME)                          # and the sizes its tool takes, if it takes one
    tool(${NAME} bench/${NAME}.c bench/tiff.c test/dither.c)
    set(EACH ${ARGN})
    if(NOT EACH)
        set(EACH whole)
    endif()
    set(SAID)
    foreach(SOURCE ${PICTURE} ${PEPPERS} ${AIRPLANE})
        get_filename_component(PIC ${SOURCE} NAME_WE)
        foreach(SIZE ${EACH})
            set(AT ${BB}/${NAME}/${PIC})
            set(OF)
            if(ARGN)
                set(AT ${AT}-${SIZE})
                set(OF ${SIZE})
            endif()
            add_custom_command(
                    OUTPUT            ${AT}.txt
                    COMMAND           ${MKDIR} ${AT}
                    COMMAND           ${NAME} ${SOURCE} ${OF} ${AT} > ${AT}.txt
                    DEPENDS           ${NAME} ${SOURCE})
            list(APPEND SAID ${AT}.txt)
        endforeach()
    endforeach()
    add_custom_target(${NAME}-report
            COMMAND           ${CAT} ${SAID}
            DEPENDS           ${SAID})
endfunction()
pictures_report(tiles own half)
pictures_report(masks own half)
pictures_report(quads)

# The dithers that are timed, each as PICTURE:SIZE:CHECKED.  PICTURE is
# one of the pictures in blocks.fs, SIZE is the size it is dithered at, and
# CHECKED is which dither of the Mandrill the bitmap must match.  The small
# grays are the Mandrill at half size: dithered at their own size they
# match the Mandrill dithered at half, and dithered at twice their size,
# with each gray as a 2 by 2, they match "doubled" (see bench/check.c).
# asks-PIC-SIZE.fs is what is typed after bench/dither.fs to run one: it
# dithers, prints where the bitmap is, and stops.  dither-costs counts the
# Mandrill and its grays at half size (COSTED_PICTURES).
set(COSTED_PICTURES gray colour)
set(DITHERS gray:own:own gray:half:half colour:own:own colour:half:half small:own:half small:twice:doubled)
foreach(DITHER ${DITHERS})
    string(REPLACE ":" ";" DITHER ${DITHER})
    list(GET DITHER 0 PIC)
    list(GET DITHER 1 SIZE)
    file(WRITE ${BB}/asks-${PIC}-${SIZE}.fs "mark ${PIC}-picture ${SIZE} mark\nb @ . b 1+ @ .\nbye\n")
endforeach()

# What is typed after bench/gray.fs: keep the colour picture as gray, one
# sample per 1 << GRAY_KEEP pixels a side; load the GRAY_MASK by GRAY_MASK
# blue noise mask; draw the whole picture through it, each sample 1 <<
# GRAY_DRAW pixels a side; copy the bitmap to the display at GRAY_AT pixels
# across at the top, as a window shows it (its content is a pixel in from
# the frame, so rarely on a cell boundary); print where the bitmap is; and
# stop.  This is the viewer's work on the 16-bit machine, which lacks the
# memory to draw the gray at full size.  check is given the same numbers.
set(GRAY_KEEP 2)
set(GRAY_DRAW 1)
set(GRAY_MASK 64)
set(GRAY_AT 1)
file(WRITE ${BB}/asks-gray.fs
     "display-init\n"
     "mark colour-picture ${GRAY_KEEP} keep mark 0 mask-blue${GRAY_MASK} m ! mark ${GRAY_DRAW} draw mark "
     "${GRAY_DRAW} ${GRAY_AT} show mark\n"
     "b @ . b 1+ @ .\nbye\n")

# A benchmark's work, counted: bench/WORK.fs and what is typed after it
# (ASKS), typed at the counting VM of the machine NAME, with the benchmarks'
# disks.  What was typed, what the machine printed and the work by place
# are left in the directory K: typed.fs, typed.out and work.
function(work_counted K NAME WORK ASKS)
    set(NUC ${BUILD}/${NAME}/nuc.hex)
    add_custom_command(
            OUTPUT            ${K}/work
            COMMAND           ${MKDIR} ${K}
            COMMAND           ${CAT} ${BENCH}/pictures.fs ${BENCH}/${WORK}.fs ${ASKS} > ${K}/typed.fs
            COMMAND           ${WITH} RUN_WORK=${K}/work
                                $<TARGET_FILE:counting> ${NUC} ${BENCH_DISKS} < ${K}/typed.fs > ${K}/typed.out 2>&1
            DEPENDS           counting machine-${NAME} ${NUC} ${BENCH_DISK_FILES} bench/pictures.fs
                              bench/${WORK}.fs picture ${PICTURE}
            VERBATIM)
endfunction()

# The work on a machine: its nucleus, the reference VM, the Verilog CPU
# with as much memory (verilog_cpu, cmake/verilog.cmake), and what checks a
# bitmap of its kind (bench_check).
machine_tool(bench_check bench/check.c bench/tiff.c test/dither.c)
function(bench_on NAME)
    string(REGEX MATCH "CELL_BITS=([0-9]+)" HAS "${SIZES_OF_${NAME}}")
    set(CELL ${CMAKE_MATCH_1})
    string(REGEX MATCH "ADDR_BITS=([0-9]+)" HAS "${SIZES_OF_${NAME}}")
    set(ADDR ${CMAKE_MATCH_1})
    set(NUC ${BUILD}/${NAME}/nuc.hex)
    foreach(LAT 1 2)
        set(T ${BB}/tb-${NAME}-${LAT})
        verilog_cpu(${T} ${CELL} ${ADDR} ${LAT})
        set_property(GLOBAL APPEND PROPERTY BENCH_PARTS ${T}/tb)
        set(ON "${CELL} bits LAT=${LAT}")
        set(WANTS bench-parts ${T}/tb ${BENCH_DISK_FILES} machine-${NAME} ${NUC} bench_check)

        set(J ${BB}/draw-${NAME}-${LAT})
        add_custom_command(
                OUTPUT            ${J}.txt
                COMMAND           ${T}/tb run ${NUC} ${BENCH_DISKS} ${J}.mem < ${BENCH}/draw.fs > ${J}.out 2> ${J}.err
                COMMAND           hexaforth ${NUC} ${BENCH_DISKS} ${J}.vm < ${BENCH}/draw.fs > ${J}.vm.out 2>&1
                COMMAND           bench_check ${NUC} display ${J}.vm ${J}.mem ${J}.err draw "${ON}" text fills scrolls
                                    > ${J}.txt 2> ${J}.check
                COMMAND           ${REMOVE} ${J}.mem ${J}.vm
                DEPENDS           ${WANTS} hexaforth bench/draw.fs
                VERBATIM)
        set_property(GLOBAL APPEND PROPERTY DRAW_FIGURES ${J}.txt)
        set_property(GLOBAL APPEND PROPERTY DRAW_KEPT
                     COMMAND ${KEEP} draw-${NAME}-${LAT} ${NUC} ${J}.txt ${BENCH}/draw.fs)

        foreach(DITHER ${DITHERS})
            string(REPLACE ":" ";" DITHER ${DITHER})
            list(GET DITHER 0 PIC)
            list(GET DITHER 1 SIZE)
            list(GET DITHER 2 MADE)
            set(J ${BB}/dither-${PIC}-${SIZE}-${NAME}-${LAT})
            # The machine of 64K cells, with the whole desktop, has not the
            # memory for a bitmap of the Mandrill's size: that is dithered
            # on its system with the picture alone (forth/picture.fs), and
            # its line says so.
            set(SYSTEM_IMAGE ${NUC})
            set(SAYS "${ON}")
            set(SYSTEM_WANTS)
            if(NOT MADE STREQUAL "half" AND ADDR EQUAL 16)
                set(SYSTEM_IMAGE ${BUILD}/${NAME}/picture/nuc.hex)
                set(SAYS "${ON}, picture.fs")
                set(SYSTEM_WANTS picture-system-${NAME} ${SYSTEM_IMAGE})
            endif()
            add_custom_command(
                    OUTPUT            ${J}.txt
                    COMMAND           ${CAT} ${BENCH}/tb-disk.fs ${BENCH}/pictures.fs ${BENCH}/dither.fs
                                        ${BB}/asks-${PIC}-${SIZE}.fs > ${J}.fs
                    COMMAND           ${T}/tb run ${SYSTEM_IMAGE} ${BENCH_DISKS} ${J}.mem < ${J}.fs > ${J}.out 2> ${J}.err
                    COMMAND           bench_check ${SYSTEM_IMAGE} picture ${PICTURE} ${MADE} ${J}.mem ${J}.out ${J}.err
                                        "dither ${PIC} ${SIZE}" "${SAYS}" > ${J}.txt 2> ${J}.check
                    COMMAND           ${REMOVE} ${J}.mem
                    DEPENDS           ${WANTS} ${SYSTEM_WANTS} bench/pictures.fs bench/tb-disk.fs bench/dither.fs
                                        picture ${PICTURE}
                    VERBATIM)
            set_property(GLOBAL APPEND PROPERTY DITHER_FIGURES ${J}.txt)
            set_property(GLOBAL APPEND PROPERTY DITHER_KEPT
                         COMMAND ${KEEP} dither-${PIC}-${SIZE}-${NAME}-${LAT} ${SYSTEM_IMAGE} ${J}.txt ${J}.fs)
        endforeach()

        set(J ${BB}/gray-${NAME}-${LAT})
        add_custom_command(
                OUTPUT            ${J}.txt
                COMMAND           ${CAT} ${BENCH}/tb-disk.fs ${BENCH}/pictures.fs ${BENCH}/gray.fs
                                    ${BB}/asks-gray.fs > ${J}.fs
                COMMAND           ${T}/tb run ${NUC} ${BENCH_DISKS} ${J}.mem < ${J}.fs > ${J}.out 2> ${J}.err
                COMMAND           bench_check ${NUC} gray ${PICTURE} ${GRAY_KEEP} ${GRAY_DRAW} ${GRAY_MASK} ${GRAY_AT}
                                    ${J}.mem ${J}.out ${J}.err "gray colour" "${ON}" keep mask draw copy
                                    > ${J}.txt 2> ${J}.check
                COMMAND           ${REMOVE} ${J}.mem
                DEPENDS           ${WANTS} bench/pictures.fs bench/tb-disk.fs bench/gray.fs picture ${PICTURE}
                VERBATIM)
        set_property(GLOBAL APPEND PROPERTY GRAY_FIGURES ${J}.txt)
        set_property(GLOBAL APPEND PROPERTY GRAY_KEPT
                     COMMAND ${KEEP} gray-${NAME}-${LAT} ${NUC} ${J}.txt ${J}.fs)
    endforeach()

    # Where the dither's clocks go (dither-costs-NAME): the same work, at
    # half the picture's size, typed at the machine's counting VM with the
    # VM's own disk, and then what each word costs with all it calls, its
    # loops and its variables (bench/costs.c, in clocks by bench/cycles.h),
    # and the fewest instructions found that do a turn of each loop
    # (bench/shorter.c, in shorter.txt).  What is kept is in
    # build/bench/costs-NAME-PIC/, what the machine printed in typed.out
    # there.
    set(WORKS)
    set(COSTED)
    foreach(PIC ${COSTED_PICTURES})
        set(K ${BB}/costs-${NAME}-${PIC})
        work_counted(${K} ${NAME} dither ${BB}/asks-${PIC}-half.fs)
        list(APPEND WORKS ${K}/work)
        list(APPEND COSTED COMMAND ${ECHO} "dither ${PIC} half, cells of ${CELL} bits:"
                           COMMAND costs ${NUC} ${BUILD}/${NAME}/nuc.tables ${BUILD}/${NAME}/nuc.sizes ${K}/work ${K}
                           COMMAND shorter ${NUC} ${BUILD}/${NAME}/nuc.tables ${K}/work ${K}/shorter.txt
                           COMMAND ${KEEP} dither-costs-${PIC} ${NUC} ${K}/work ${K}/typed.fs ${K}/paths.txt
                                     ${K}/loops.txt ${K}/vars.txt ${K}/shorter.txt)
    endforeach()
    add_custom_target(dither-costs-${NAME} ${COSTED} DEPENDS ${WORKS} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    # The same for the gray (gray-costs-NAME): bench/gray.fs's work typed at
    # the counting VM, then the cost of each word, loop and variable, in
    # build/bench/gray-costs-NAME/.
    set(K ${BB}/gray-costs-${NAME})
    work_counted(${K} ${NAME} gray ${BB}/asks-gray.fs)
    add_custom_target(gray-costs-${NAME}
            COMMAND           ${ECHO} "the gray, cells of ${CELL} bits:"
            COMMAND           costs ${NUC} ${BUILD}/${NAME}/nuc.tables ${BUILD}/${NAME}/nuc.sizes ${K}/work ${K}
            COMMAND           ${KEEP} gray-costs ${NUC} ${K}/work ${K}/typed.fs ${K}/paths.txt ${K}/loops.txt ${K}/vars.txt
            DEPENDS           ${K}/work
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endfunction()
foreach(NAME ${MACHINES})
    bench_on(${NAME})
endforeach()
get_property(BENCH_PARTS GLOBAL PROPERTY BENCH_PARTS)
add_custom_target(bench-parts DEPENDS ${BENCH_DISK_FILES} ${BENCH_PARTS})

# The 68000.  Musashi is fetched as it was when this was written; the
# program is where it is put in the 68000's memory.
set(MB ${BB}/m68k)
set(M68K_AT 0x1000)
set(M68K_FIGURES)
if(M68K_AS)
    set(MUSASHI ${BB}/Musashi)
    source_fetched(musashi https://github.com/kstenerud/Musashi 313ebf1bd9f4d0d93341eb5ce21fd8a119e9dbdd ${MUSASHI})
    set_source_files_properties(${MUSASHI}/m68kmake.c ${MUSASHI}/m68kcpu.c ${MUSASHI}/softfloat/softfloat.c
                                PROPERTIES GENERATED TRUE)
    tool(m68kmake ${MUSASHI}/m68kmake.c)
    add_dependencies(m68kmake musashi)
    add_custom_command(
            OUTPUT            ${MB}/m68kops.c ${MB}/m68kops.h
            COMMAND           ${MKDIR} ${MB}
            COMMAND           m68kmake ${MB} ${MUSASHI}/m68k_in.c > ${MB}/m68kmake.log 2>&1
            DEPENDS           m68kmake)
    tool(run68k bench/m68k/run68k.c bench/tiff.c test/dither.c
                ${MUSASHI}/m68kcpu.c ${MB}/m68kops.c ${MUSASHI}/softfloat/softfloat.c)
    target_include_directories(run68k PRIVATE ${MUSASHI} ${MB})
    add_dependencies(run68k musashi)
    left_out(m68kmake run68k)                           # they are made of what is fetched

    set(M68K_ASSEMBLED ${M68K_AS} -m68000 --register-prefix-optional)
    foreach(PART atk gray)
        add_custom_command(
                OUTPUT            ${MB}/${PART}.o
                COMMAND           ${MKDIR} ${MB}
                COMMAND           ${M68K_ASSEMBLED} -o ${MB}/${PART}.o ${BENCH}/m68k/${PART}.s
                DEPENDS           bench/m68k/${PART}.s)
    endforeach()
    # What the 68000 dithers: the Mandrill and its grays, each at its own
    # size and at half.
    foreach(PIC gray colour)
        foreach(SIZE own half)
            set(J ${MB}/${PIC}-${SIZE})
            set(SAMPLES 3)
            if(PIC STREQUAL gray)
                set(SAMPLES 1)
            endif()
            set(HALF 0)
            if(SIZE STREQUAL half)
                set(HALF 1)
            endif()
            add_custom_command(
                    OUTPUT            ${J}.txt
                    COMMAND           ${M68K_ASSEMBLED} --defsym SAMPLES=${SAMPLES} --defsym HALF=${HALF}
                                        -o ${J}.o ${BENCH}/m68k/glue.s
                    COMMAND           ${M68K_LD} -Ttext=${M68K_AT} -e _start -o ${J}.elf ${J}.o ${MB}/atk.o ${MB}/gray.o
                    COMMAND           ${M68K_OBJCOPY} -O binary ${J}.elf ${J}.bin
                    COMMAND           run68k ${J}.bin ${M68K_AT} ${PICTURE} ${PIC} ${SIZE} > ${J}.txt 2> ${J}.log
                    DEPENDS           run68k bench/m68k/glue.s ${MB}/atk.o ${MB}/gray.o picture ${PICTURE})
            list(APPEND M68K_FIGURES ${J}.txt)
        endforeach()
    endforeach()
else()
    file(WRITE ${BB}/m68k.txt "m68k                   68000             not run: there is no m68k-elf-as\n")
    list(APPEND M68K_FIGURES ${BB}/m68k.txt)
endif()

# What was found, as figures: each kind is one target's to make, and the
# targets that print are of those.  A target that prints figures keeps
# them, each under the image it is of (KEEP): every time, since a figure
# that is already kept is not kept again.  The 68000's are of no image of
# this machine's, and change only when its program does.
foreach(KIND DRAW DITHER GRAY)
    get_property(${KIND}_FIGURES GLOBAL PROPERTY ${KIND}_FIGURES)
    get_property(${KIND}_KEPT GLOBAL PROPERTY ${KIND}_KEPT)
    string(TOLOWER ${KIND} KIND_OF)
    add_custom_target(${KIND_OF}-figures DEPENDS ${${KIND}_FIGURES})
endforeach()
set(M68K_KEPT)
add_custom_target(m68k-figures DEPENDS ${M68K_FIGURES})
function(figures TARGET)                                # and the kinds it prints
    set(PRINTED)
    set(KEPT_SO)
    foreach(KIND ${ARGN})
        if(NOT ${KIND}_FIGURES)                         # a kind that has nothing to print was not said: it is not left out
            message(FATAL_ERROR "the target ${TARGET} has no figure of the kind ${KIND} to print")
        endif()
        list(APPEND PRINTED ${${KIND}_FIGURES})
        list(APPEND KEPT_SO ${${KIND}_KEPT})
    endforeach()
    add_custom_target(${TARGET} COMMAND bench_check table ${PRINTED} ${KEPT_SO})
    foreach(KIND ${ARGN})
        string(TOLOWER ${KIND} OF)
        add_dependencies(${TARGET} ${OF}-figures)
    endforeach()
endfunction()
figures(bench-draw DRAW)
figures(bench-dither DITHER)
figures(bench-gray GRAY)
figures(bench-m68k M68K)
figures(bench DRAW DITHER GRAY M68K)
