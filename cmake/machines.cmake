#
# The machines the system is tried on, and everything that is built for one
# and run on it.
#
# util/machines is the machines.  For each, everything made is in a
# directory of build/ that has its name, which is the bits of its cell:
# the machine's definition for cross.fs (basewords.fs), its nucleus
# (nuc.hex, and what cross.fs writes beside it), the disks its tests are
# given, and what the tests leave.  Nothing is compiled for a machine: a
# tool or a test is one program, where CMake builds, and is of the machine
# whose image it is given (machine_tool, stepping_tool).
#
# Nothing here is built unless it is asked for: a test builds what it
# needs before it is run (needs, cmake/kinds.cmake), the target `check`
# runs every test, and each report is a target of its own.
#

# The seeds the tests that try things by chance are each run with.
set(SEEDS 3 4 CACHE STRING "the seeds the tests that try things by chance are run with")

# The nucleus on the reference VM, typed at (test/nucleus.c), and the
# desktop on it (test/deskvm.c) with what looks at its screen.
set(TYPED_SOURCES test/nucleus.c vm.c)
set(DESK_SOURCES test/deskvm.c test/deskcheck.c test/tables.c test/dither.c bench/tiff.c)

# A machine is two things.  It is sizes, which the generator is told and
# makes its definition of (basewords.fs): no tool is built for them, since
# cross.fs says them beside every image it makes (nuc.machine), and a tool
# is the machine of the image it is given.  And it is a system: the images
# cross.fs makes for it and the disks that go with them.  A machine of
# util/machines is both under one name.  QEMU's has the sizes of one of
# those and a system of its own, since its ROM has another disk's driver.
#
# What is made for a system is the target machine-NAME's to make.
# Whatever else is made from one of those files depends on the file, so
# that it is made again when the file is, and on the target, so that the
# file is made in one place and not by two targets at once.  made() says a
# file is that target's.
function(made NAME)
    set_property(GLOBAL APPEND PROPERTY MADE_FOR_${NAME} ${ARGN})
    owned(machine-${NAME} ${ARGN})
endfunction()

# A system: its name, which is its directory of build/ and what its
# targets are named for, and the machine whose sizes it has.
#
#   system(NAME OF
#          [SET name=value...]  what cross.fs is told for every image of it
#                               (ROM=virt-disk.fs)
#          [AFTER what...])     what its nucleus is made after
#
# Its nucleus depends on forth-env, which makes the two generated Forth
# files.  A system whose machine has a display also has:
#
#   the names that go with its image (nuc.tables)
#   the decompiler, and a table for it made from the system's own image
#     (tabled, cmake/default-machine.cmake); the test tables-NAME checks
#     the table against the image
#   a disk for its desktop
#   its boot image (BOOT=on): the system without what forth/apps.fs lists,
#     which the boot compiles from the disk (doc/boot.md).
#     boot-system-NAME makes it in its own directory, with its two disks.
#     It is built after the nucleus because both builds write the answers
#     fuse.h keeps in build/
#
# system() keeps the settings every image of the system is built with
# (TOLD_NAME) and the system's table (TABLE_OF_NAME).
#
# The default machine's system uses forth/fused.fs as its table.  That file
# is checked in, cross.fs reads it when FUSED is not set, and forth-env
# makes it from this system's images.  Every other system has its table in
# its own directory.
#
# A system whose NAME differs from OF, such as QEMU's, has the sizes of the
# machine OF and a disk the reference VM cannot boot from.  Its table takes
# the instructions of a boot from that machine's boot (BOOTED_BY_NAME): the
# same compiler, source and sizes.  Its test tables-NAME is left out of
# on_the_vm, so only check runs it.
function(system NAME OF)
    cmake_parse_arguments(IT "" "" "SET;AFTER" ${ARGN})
    set(AT ${BUILD}/${NAME})
    set(WORDS ${WORDS_OF_${OF}})
    get_property(DEFAULT GLOBAL PROPERTY DEFAULT_MACHINE)
    set(NAMES)
    set(TOLD ${IT_SET})
    set(SOURCES ${FORTH_SOURCES})
    set(TABLE)
    if("${SIZES_OF_${OF}}" MATCHES "DISP_W=")
        set(NAMES tables)                               # the names, which a machine with a display has
        if(NAME STREQUAL "${DEFAULT}")
            set(TABLE ${FUSED_FS})
            tabled(${AT} ${TABLE}
                   OF "the machine ${NAME}"
                   WORDS ${WORDS}
                   SET ${IT_SET}
                   AFTER ${IT_AFTER})
        else()
            set(TABLE ${AT}/fused.fs)
            set(ELSEWHERE)
            if(NOT NAME STREQUAL OF)
                set(ELSEWHERE BOOTED ${BOOTED_BY_${OF}})
            endif()
            tabled(${AT} ${TABLE}
                   OF "the machine ${NAME}"
                   WORDS ${WORDS}
                   SET ${IT_SET}
                   AFTER forth-env ${IT_AFTER}
                   ${ELSEWHERE})
            list(APPEND TOLD FUSED=${TABLE})
            set(SOURCES ${SOURCES_BUT_TABLE} ${TABLE})
        endif()
        set(BOOTED_BY_${NAME} ${BOOTED_FOR} CACHE INTERNAL "the four files of the booted machine that fused takes for the system ${NAME}")
    endif()
    set(TOLD_${NAME} ${TOLD} CACHE INTERNAL "the settings every image of the system ${NAME} is built with")
    set(TABLE_OF_${NAME} "${TABLE}" CACHE INTERNAL "the decompiler's table the system ${NAME} is built with")
    image(${AT}
          WORDS ${WORDS}
          SET ${TOLD}
          MAKES hex lst sizes inline ${NAMES}
          FROM ${SOURCES}
          AFTER forth-env ${IT_AFTER}
          IS "The nucleus for the machine ${NAME}")
    set(NUCLEUS ${IMAGE_FILES})
    made(${NAME} ${NUCLEUS})
    image_owned(machine-${NAME} ${AT})
    if(NOT NAMES)
        return()
    endif()
    made(${NAME} ${TABLE})
    table_test(tables-${NAME} ${AT} ${TABLE} MADE ${NUCLEUS_MADE} NEEDS machine-${NAME})
    if(NAME STREQUAL OF)
        on_the_vm(tables-${NAME})
    endif()

    # The desktop's disk: block 1 for the editor, and the names that go
    # with this image.
    desk_disk(${AT}/disk.img ${AT}/nuc.tables)
    made(${NAME} ${AT}/disk.img)

    image(${AT}/boot
          WORDS ${WORDS}
          SET ${TOLD} BOOT=on
          MAKES hex tables
          FROM ${SOURCES}
          AFTER machine-${NAME} ${NUCLEUS}
          IS "The image the machine ${NAME} boots from a disk with")
    # Its disk: block 1 is forth/apps.fs, and after it each file it
    # includes, which the boot compiles from its source (mkdisk follows
    # the includes, so apps.fs is the one list).
    add_custom_command(
            OUTPUT            ${AT}/boot/apps.img ${AT}/boot/apps.ipl
            COMMAND           mkdisk --boot ${AT}/boot/nuc.hex ${AT}/boot/apps.img ${AT}/boot/apps.ipl
                                ${CMAKE_SOURCE_DIR}/forth/apps.fs > ${AT}/boot/apps-disk.log 2>&1
            DEPENDS           mkdisk ${AT}/boot/nuc.hex ${FORTH_SOURCES})
    # And the disk it starts its desktop from: the same, with the
    # desktop's own things at their blocks (mkdisk --boot-desk), and the
    # names that go with that image.
    add_custom_command(
            OUTPUT            ${AT}/boot/desk.img ${AT}/boot/desk.ipl
            COMMAND           mkdisk --boot-desk ${AT}/boot/nuc.hex ${AT}/boot/desk.img ${AT}/boot/desk.ipl
                                ${AT}/boot/nuc.tables
                                ${CMAKE_SOURCE_DIR}/forth/apps.fs > ${AT}/boot/desk-disk.log 2>&1
            DEPENDS           mkdisk ${AT}/boot/nuc.hex ${FORTH_SOURCES})
    add_custom_target(boot-system-${NAME} DEPENDS ${AT}/boot/nuc.hex ${AT}/boot/apps.img ${AT}/boot/desk.img)
    image_owned(boot-system-${NAME} ${AT}/boot)
    owned(boot-system-${NAME} ${AT}/boot/apps.img ${AT}/boot/apps.ipl ${AT}/boot/desk.img ${AT}/boot/desk.ipl)
endfunction()

# A system's nucleus again with the peephole off (PEEPHOLE=off), which
# plain-NAME makes in a directory of its own.  It reads the calls the first
# build compiled in place (nuc.inline), so it is made after it.  It is made
# of the machine the two builds are compared on, and the reports are of.
function(plain NAME)
    set(AT ${BUILD}/${NAME})
    image(${AT}/plain
          WORDS ${WORDS_OF_${NAME}}
          SET ${TOLD_${NAME}} PEEPHOLE=off
          MAKES hex tables
          FIRST ${COPY} ${AT}/nuc.inline ${AT}/plain/nuc.inline
          AFTER machine-${NAME} ${AT}/nuc.hex ${AT}/nuc.inline
          IS "The nucleus for the machine ${NAME}, with the peephole off")
    add_custom_target(plain-${NAME} DEPENDS ${IMAGE_FILES})
    image_owned(plain-${NAME} ${AT}/plain)
endfunction()

# A second system: the picture alone (forth/picture.fs), which
# picture-system-NAME makes in a directory of its own, after the nucleus.
# It is for the machine of 64K cells, which with the whole desktop has not
# the memory to dither the picture at its own size (bench_on).
function(picture_system NAME)
    set(AT ${BUILD}/${NAME})
    image(${AT}/picture
          WORDS ${WORDS_OF_${NAME}}
          SET SYSTEM=picture.fs
          AFTER machine-${NAME} ${AT}/nuc.hex
          IS "The system with the picture alone, for the machine ${NAME}")
    add_custom_target(picture-system-${NAME} DEPENDS ${IMAGE_FILES})
    image_owned(picture-system-${NAME} ${AT}/picture)
endfunction()

# machine(NAME SIZE=value...) builds a machine.  Each size is named as
# vm_constants.h names it.
#
# The default machine is the first machine in util/machines (the property
# DEFAULT_MACHINE).  It is built with the checked-in definition,
# forth/basewords.fs, and it is the only build of those sizes.  machine()
# still generates a definition from the sizes in util/machines, and passes
# it to default_machine_is() (cmake/default-machine.cmake), whose test
# compares it with forth/basewords.fs.
function(machine NAME)
    set(AT ${BUILD}/${NAME})
    set(SIZES_OF_${NAME} ${ARGN} CACHE INTERNAL "the sizes of the machine ${NAME}, each as the name vm_constants.h has for it and what it is")

    # Its definition for cross.fs, which the generator makes of the machine
    # as it is told it: the bits of a cell, the bits of an address and the
    # display's width, height and first cell, as far as they are said.
    set(SAID)
    foreach(SIZE CELL_BITS ADDR_BITS DISP_W DISP_H DISP_BASE)
        if("${ARGN}" MATCHES "(^|;)${SIZE}=([^;]*)")
            list(APPEND SAID ${CMAKE_MATCH_2})
        endif()
    endforeach()
    add_custom_command(
            OUTPUT            ${AT}/basewords.fs
            COMMAND           ${MKDIR} ${AT}
            COMMAND           basewords-fs-gen ${AT}/basewords.fs ${SAID} > ${AT}/basewords.log
            DEPENDS           basewords-fs-gen)
    get_property(DEFAULT GLOBAL PROPERTY DEFAULT_MACHINE)
    if(NAME STREQUAL "${DEFAULT}")
        set(WORDS_OF_${NAME} ./basewords.fs CACHE INTERNAL "the definition the machine ${NAME} is built with")
    else()
        set(WORDS_OF_${NAME} ${AT}/basewords.fs CACHE INTERNAL "the definition the machine ${NAME} is built with")
    endif()
    system(${NAME} ${NAME})
    if(NAME STREQUAL "${DEFAULT}")
        made(${NAME} ${AT}/basewords.fs)
        default_machine_is(${NAME} ${AT}/basewords.fs)
    endif()
endfunction()

# When all that is made for a machine has been said.
function(machine_made NAME)
    get_property(FILES GLOBAL PROPERTY MADE_FOR_${NAME})
    add_custom_target(machine-${NAME} DEPENDS ${FILES})
endfunction()

# default_machine(RESULT CELL) sets RESULT to the name of the machine that
# vm_constants.h describes for a cell of CELL bits when no other size is
# given.  For the default machine's cell size that is the default machine.
# For any other it is default-CELL, which is created on first use.
function(default_machine RESULT CELL)
    get_property(DEFAULT GLOBAL PROPERTY DEFAULT_MACHINE)
    if("${SIZES_OF_${DEFAULT}}" MATCHES "(^|;)CELL_BITS=${CELL}(;|$)")
        set(${RESULT} ${DEFAULT} PARENT_SCOPE)
        return()
    endif()
    set(NAME default-${CELL})
    get_property(MADE GLOBAL PROPERTY MACHINE_MADE_${NAME})
    if(NOT MADE)
        set_property(GLOBAL PROPERTY MACHINE_MADE_${NAME} TRUE)
        machine(${NAME} CELL_BITS=${CELL})
        machine_made(${NAME})
    endif()
    set(${RESULT} ${NAME} PARENT_SCOPE)
endfunction()

# The reference VM is `hexaforth` (cmake/default-machine.cmake).
# An image's code, word by word, as the decompiler shows it, with every
# instruction's place, number and fields (util/disasm_hex.c, which says at
# its top what it shows): disasm_hex IMAGE.hex [NAME... | FROM TO].  It has
# the machine in it, for where a branch goes.
machine_tool(disasm_hex util/disasm_hex.c test/nucleus.c)
# The reference VM, counting how often the instruction at each place is run
# (bench/counting.c): counting IMAGE.hex [DISK.img [MEMORY.out]], and the
# counts written where RUN_COUNTS says.
machine_tool(counting bench/counting.c)
# What work counted that way costs (bench/costs.c); and its loops, each
# with the fewest instructions found that do a turn of it (bench/shorter.c).
stepping_tool(costs bench/costs.c util/image.c)
stepping_tool(shorter bench/shorter.c util/image.c)
target_compile_definitions(shorter PRIVATE SEARCH_STATES=${SEARCH_STATES})
# What would make a kernel's ROM shorter: its loops, and every run of its
# words, each with the fewest instructions found that do it (shorter
# --rom).  shortened() makes the target that says it of the image in a
# directory, shorter-NAME, and it takes as long as its searches do: what is
# found is in shorter.txt there as it is found, and the run each search is
# of in shorter.log.
function(shortened TARGET AT)
    report(${TARGET}
            COMMAND           shorter ${AT}/nuc.hex ${AT}/nuc.tables --rom ${AT}/shorter.txt 2> ${AT}/shorter.log
            COMMAND           ${KEEP} shorter ${AT}/nuc.hex ${AT}/shorter.txt)
endfunction()
# Two measurements of the same work, by word (bench/compared.c): compared
# WAS WAS.work IS IS.work, of what is built and of what was kept
# (build/kept/, and build/kept/log.txt says what there is).
stepping_tool(compared bench/compared.c util/image.c)

# A test of a machine: what it is called, what is built for it, and what
# that is run with.  It is run from where the source is.  It needs what is
# built for it, and what is made for the machine: its image, its disks.  A
# test that reads more than that says so itself.
function(machine_test TEST NAME TOOL)
    add_test(NAME ${TEST} COMMAND ${TOOL} ${ARGN} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    needs(${TEST} ${TOOL} machine-${NAME})
    on_the_vm(${TEST})
endfunction()

# The tests that type at the prompt of a machine's reference VM: the
# nucleus; comments, and a block of a disk read as source; regions a cell
# at a time against the one form of their pixels; the dither against a
# dither a pixel at a time; the TIFF reader; and for a machine with a
# display, the drawing words against a model of them, and the desktop, its
# windows worked with keys and a pointer and its screen looked at.  The
# same programs and the same tests on every machine: what is different is
# the image.  The two that start a machine from its ROM stop at a store
# into it, where a machine whose kernel is a ROM would go wrong (ROM_BYTES).
machine_tool(prompt_test test/prompt_test.c ${TYPED_SOURCES})
machine_tool(source_test test/source_test.c ${TYPED_SOURCES})
machine_tool(boot_test test/boot_test.c test/tables.c ${TYPED_SOURCES})
target_compile_definitions(boot_test PRIVATE ROM_BYTES=KROM_BYTES)
machine_tool(region_test test/region_test.c ${TYPED_SOURCES})
machine_tool(dither_test test/dither_test.c test/dither.c ${TYPED_SOURCES})
machine_tool(tiff_test test/tiff_test.c test/dither.c ${TYPED_SOURCES})
machine_tool(display_test test/display_test.c test/tables.c ${TYPED_SOURCES})
machine_tool(gray_test test/gray_test.c test/dither.c ${TYPED_SOURCES})
machine_tool(desktop_test test/desktop_test.c ${DESK_SOURCES} ${TYPED_SOURCES})
target_compile_definitions(desktop_test PRIVATE ROM_BYTES=KROM_BYTES)
# What a machine compiled at its boot, beside the same words as cross.fs
# lays them (bench/booted.c): the target booted-NAME.
machine_tool(booted_sizes bench/booted.c)
function(machine_tests NAME)
    set(AT ${BUILD}/${NAME})
    set(NUC ${AT}/nuc.hex)

    machine_test(prompt-${NAME} ${NAME} prompt_test ${NUC})

    # `hexaforth --typed` on this machine (main.c):
    #
    #   typed-NAME          a file with no errors, which leaves 3 4 on the stack
    #   typed-thrown-NAME   a file with an undefined word, which throws -13
    #                       and is named in the error
    machine_test(typed-${NAME} ${NAME} hexaforth --machine ${NAME} --typed test/typed.fs)
    set_tests_properties(typed-${NAME} PROPERTIES
                         PASS_REGULAR_EXPRESSION "21 twice \\. 42  ok.*data stack \\(2 cells, top last\\): 3 4\nhalted\n")
    machine_test(typed-thrown-${NAME} ${NAME} hexaforth --machine ${NAME} --typed test/typed-thrown.fs)
    set_tests_properties(typed-thrown-${NAME} PROPERTIES
                         PASS_REGULAR_EXPRESSION "error: -13 no-such-word.*halted after 1 uncaught error")

    # The block is a file that is there for this, laid down as a disk's
    # source is.
    add_custom_command(
            OUTPUT            ${AT}/source.img
            COMMAND           ${MKDIR} ${AT}
            COMMAND           mkdisk ${AT}/source.img ${CMAKE_SOURCE_DIR}/test/source.fs > ${AT}/source-disk.log 2>&1
            DEPENDS           mkdisk test/source.fs)
    machine_test(source-${NAME} ${NAME} source_test ${NUC} ${AT}/source.img)
    made(${NAME} ${AT}/source.img)

    # The machine started from its ROM alone, and a disk mkdisk made of the
    # same image, with test/boot.fs as its block 1 and test/boot-more.fs
    # after it (doc/boot.md); or with no disk and what mkdisk wrote for the
    # console; or with a disk whose block 1 is test/boot-error.fs.
    add_custom_command(
            OUTPUT            ${AT}/boot.img ${AT}/boot.ipl ${AT}/boot-error.img ${AT}/boot-error.ipl
            COMMAND           ${MKDIR} ${AT}
            COMMAND           mkdisk --boot ${NUC} ${AT}/boot.img ${AT}/boot.ipl
                                ${CMAKE_SOURCE_DIR}/test/boot.fs ${CMAKE_SOURCE_DIR}/test/boot-more.fs
                                > ${AT}/boot-disk.log 2>&1
            COMMAND           mkdisk --boot ${NUC} ${AT}/boot-error.img ${AT}/boot-error.ipl
                                ${CMAKE_SOURCE_DIR}/test/boot-error.fs >> ${AT}/boot-disk.log 2>&1
            DEPENDS           mkdisk ${NUC} test/boot.fs test/boot-more.fs test/boot-error.fs)
    # A second disk, for the checks that give a machine two: an index and
    # the files of test/second/, one of them under a label the first disk
    # has too.
    add_custom_command(
            OUTPUT            ${AT}/second.img
            COMMAND           ${MKDIR} ${AT}
            COMMAND           mkdisk --disk ${AT}/second.img
                                ${CMAKE_SOURCE_DIR}/test/second/second.fs ${CMAKE_SOURCE_DIR}/test/second/boot-more.fs
                                > ${AT}/second-disk.log 2>&1
            DEPENDS           mkdisk test/second/second.fs test/second/boot-more.fs)
    made(${NAME} ${AT}/second.img)
    # And the disk of the image the machine boots from (boot-system-NAME).
    machine_test(boot-${NAME} ${NAME} boot_test ${NUC} ${AT}/boot.img ${AT}/boot.ipl ${AT}/boot-error.img forth
                 ${AT}/boot/apps.img ${AT}/second.img)
    needs(boot-${NAME} boot-system-${NAME})
    made(${NAME} ${AT}/boot.img ${AT}/boot.ipl ${AT}/boot-error.img)

    foreach(SEED ${SEEDS})
        machine_test(regions-${NAME}-${SEED} ${NAME} region_test ${NUC} ${SEED})
        machine_test(dither-${NAME}-${SEED} ${NAME} dither_test ${NUC} ${SEED})
        machine_test(tiff-${NAME}-${SEED} ${NAME} tiff_test ${NUC} ${SEED})
    endforeach()

    if(NOT "${SIZES_OF_${NAME}}" MATCHES "DISP_W=")
        return()
    endif()

    foreach(SEED ${SEEDS})
        machine_test(display-${NAME}-${SEED} ${NAME} display_test ${NUC} forth ${SEED})
        machine_test(gray-${NAME}-${SEED} ${NAME} gray_test ${NUC} ${SEED})
    endforeach()

    # The desktop, with the system's disk for it (system); and the pictures'
    # disk as its second, which has the Mandrill.  The test keeps its
    # screens, and its copy of the first disk, in a directory of its own.
    file(MAKE_DIRECTORY ${AT}/desktop)
    machine_test(desktop-${NAME} ${NAME} desktop_test ${NUC} ${AT}/disk.img ${PICTURES} ${PICTURE} forth
                 ${AT}/desktop names)
    needs(desktop-${NAME} picture pictures)

    # The same checks on the machine started from its ROM alone, with a disk
    # it can start from that is the desktop's too (boot-system-NAME): the
    # desktop they are run on is the one its boot compiled from the disk's
    # source (doc/boot.md).
    file(MAKE_DIRECTORY ${AT}/booted)
    machine_test(desktop-booted-${NAME} ${NAME} desktop_test ${AT}/boot/nuc.hex ${AT}/boot/desk.img ${PICTURES}
                 ${PICTURE} forth ${AT}/booted names rom)
    needs(desktop-booted-${NAME} boot-system-${NAME} picture pictures)

    # What the machine compiled at that boot (booted_sizes).
    report(booted-${NAME}
            COMMAND           ${WITH} BOOTED=${AT}/booted/memory
                                $<TARGET_FILE:desktop_test> ${AT}/boot/nuc.hex ${AT}/boot/desk.img ${PICTURES}
                                ${PICTURE} forth ${AT}/booted names rom > ${AT}/booted/checks.txt
            COMMAND           booted_sizes ${AT}/boot/nuc.hex ${AT}/boot/nuc.tables ${AT}/booted/memory
                                ${AT}/booted/disk.img ${AT}/nuc.hex ${AT}/nuc.tables > ${AT}/booted/sizes.txt
            COMMAND           ${ECHO} "what the machine ${NAME} compiled at its boot: ${AT}/booted/sizes.txt"
            COMMAND           ${KEEP} booted ${AT}/boot/nuc.hex ${AT}/booted/sizes.txt)
endfunction()

# After any change to a compiler or a peephole: the image against the image
# built with the peephole off (test/builds_test.c, which says how it
# compares them).
stepping_tool(builds_test test/builds_test.c util/image.c)
function(builds_test NAME)
    set(AT ${BUILD}/${NAME})
    add_test(NAME builds
             COMMAND builds_test ${AT}/plain/nuc.hex ${AT}/plain/nuc.tables ${AT}/nuc.hex ${AT}/nuc.tables
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    needs(builds builds_test machine-${NAME} plain-${NAME})
    on_the_vm(builds)
endfunction()

# The whole of a machine's image as it is laid out, to be read for a better
# way of laying it: listing-NAME writes build/NAME/listing.txt
# (util/disasm_hex.c, --layout, which says what is in it).  The desktop is
# worked first on the VM that counts (bench/profile.c, as the target
# `profile` works it), so that each call to a word on another page says how
# often it was run; what that left is in build/NAME/listing/.
#
# The reference VM that counts how often the instruction at each place is
# run (bench/counting.c), in place of vm.c, is the VM the desktop is worked
# on (profiler); by itself it is `counting`.
set(COUNTED_SOURCES test/nucleus.c bench/counting.c)
machine_tool(profiler bench/profile.c ${DESK_SOURCES} ${COUNTED_SOURCES})
target_compile_definitions(profiler PRIVATE COUNTING_A_TEST)
function(listed NAME)
    set(AT ${BUILD}/${NAME})
    set(L ${AT}/listing)
    report(listing-${NAME}
            COMMAND           ${MKDIR} ${L}
            COMMAND           profiler ${AT}/nuc.hex ${AT}/nuc.tables ${AT}/nuc.sizes ${AT}/disk.img ${PICTURES} ${PICTURE}
                                forth ${L} > ${L}/profile.txt 2> ${L}/profile.log
            COMMAND           disasm_hex ${AT}/nuc.hex --no-color --layout --counts ${L}/desktop.counts
                                > ${AT}/listing.txt 2> ${L}/listing.log
            COMMAND           ${ECHO} "the image of the machine ${NAME}, laid out: ${AT}/listing.txt"
            COMMAND           ${KEEP} listing ${AT}/nuc.hex ${AT}/listing.txt ${L}/desktop.counts ${L}/profile.txt
            COMMENT           "The machine ${NAME}'s image as it is laid out (what went wrong, if anything did, is in ${L})")
endfunction()

# Where a machine's memory goes: memory-map-NAME prints the map (util/map.c)
# of its image as cross.fs built it; of the machine started from that with
# its disk, at its prompt; of the machine that boots from a disk
# (boot-system-NAME), once its boot has compiled what the disk has; and of
# the desktop, when its checks are done.  The last three are run on the
# reference VM that watches the heap (bench/heap.c: `heap` by itself, and
# as the VM in the desktop's test, desktop_test_heap), and after the map of
# each is what the heap was given out to while it ran, and the highest cell
# of each stack that was used, with what the return stack held then.  Each
# map, what the watch said and the memory they are of are kept in
# build/NAME/map/.
set(WATCHED_SOURCES test/nucleus.c bench/heap.c)
machine_tool(map util/map.c)
machine_tool(heap bench/heap.c)
machine_tool(desktop_test_heap test/desktop_test.c ${DESK_SOURCES} ${WATCHED_SOURCES})
target_compile_definitions(desktop_test_heap PRIVATE WATCHING_A_TEST)
function(mapped NAME)
    set(AT ${BUILD}/${NAME})
    set(MP ${AT}/map)
    report(memory-map-${NAME}
            COMMAND           ${MKDIR} ${MP}/desktop
            COMMAND           map ${AT}/nuc.hex ${AT}/nuc.tables > ${MP}/image.txt
            COMMAND           ${COPY} ${AT}/disk.img ${MP}/disk.img
            COMMAND           ${WITH} HEAP=${MP}/prompt.heap HEAP_NAMES=${AT}/nuc.tables
                                $<TARGET_FILE:heap> ${AT}/nuc.hex ${MP}/disk.img ${MP}/prompt.mem
                                < ${CMAKE_SOURCE_DIR}/bench/idle.fs > ${MP}/prompt.out 2>&1
            COMMAND           map ${AT}/nuc.hex ${AT}/nuc.tables ${MP}/prompt.mem > ${MP}/prompt.txt
            COMMAND           ${COPY} ${AT}/boot/apps.img ${MP}/apps.img
            COMMAND           ${WITH} HEAP=${MP}/boot.heap HEAP_NAMES=${AT}/boot/nuc.tables
                                $<TARGET_FILE:heap> ${AT}/boot/nuc.hex ${MP}/apps.img ${MP}/boot.mem
                                < ${CMAKE_SOURCE_DIR}/bench/idle.fs > ${MP}/boot.out 2>&1
            COMMAND           map ${AT}/boot/nuc.hex ${AT}/boot/nuc.tables ${MP}/boot.mem > ${MP}/boot.txt
            COMMAND           ${WITH} MEMORY=${MP}/desktop.mem HEAP=${MP}/desktop.heap
                                HEAP_NAMES=${AT}/nuc.tables
                                $<TARGET_FILE:desktop_test_heap> ${AT}/nuc.hex ${AT}/disk.img ${PICTURES}
                                ${PICTURE} forth ${MP}/desktop names > ${MP}/desktop.out 2>&1
            COMMAND           map ${AT}/nuc.hex ${AT}/nuc.tables ${MP}/desktop.mem > ${MP}/desktop.txt
            COMMAND           ${CAT} ${MP}/image.txt ${MP}/prompt.txt ${MP}/prompt.heap
                                ${MP}/boot.txt ${MP}/boot.heap ${MP}/desktop.txt ${MP}/desktop.heap
            COMMAND           ${KEEP} memory-map ${AT}/nuc.hex ${MP}/image.txt ${MP}/prompt.txt ${MP}/prompt.heap
                                ${MP}/desktop.txt ${MP}/desktop.heap
            COMMAND           ${KEEP} memory-map ${AT}/boot/nuc.hex ${MP}/boot.txt ${MP}/boot.heap
            VERBATIM)

    # The same as a table (map --table): what each part of memory has used
    # and has left, of the system as cross.fs built it and of the machine
    # that boots without the applications and has compiled them from its
    # disk.  That machine is run to its prompt here, and stopped if it runs
    # on (vm_stopped.h).  memory-table-NAME makes build/NAME/map/table.txt,
    # which quick and check show.
    report(memory-table-${NAME}
            COMMAND           ${MKDIR} ${MP}
            COMMAND           ${COPY} ${AT}/boot/apps.img ${MP}/table.img
            COMMAND           ${WITH} STOP_BUDGET=2000000000
                                $<TARGET_FILE:hexaforth> ${AT}/boot/nuc.hex ${MP}/table.img ${MP}/table.mem
                                < ${CMAKE_SOURCE_DIR}/bench/idle.fs > ${MP}/table.out 2>&1
            COMMAND           map --table ${AT}/nuc.hex ${AT}/nuc.tables ${AT}/boot/nuc.hex ${AT}/boot/nuc.tables
                                ${MP}/table.mem > ${MP}/table.txt
            VERBATIM)
    owned(memory-table-${NAME} ${MP}/table.txt)
    set_property(GLOBAL APPEND PROPERTY MEMORY_TABLES ${MP}/table.txt)
endfunction()

# `compiling-64` and the like: what it costs a machine to boot from its
# disk and compile from source there what its image is without.  The
# reference VM that counts (bench/counting.c) is started with the boot's
# image in memory, the disk the desktop boots from and the pictures' disk,
# and runs to its prompt.  It boots from a copy of the disk: a boot writes
# to its disk (the names it keeps, forth/files.fs), and the tests use the
# original.  Then what the work costs is said (bench/costs.c):
# how many instructions and cycles, each word with all it calls, and the
# loops.  What the machine said, the work by place and the reports are in
# build/NAME/compiling/, where `disasm_hex ... --counts` reads the work to
# say how often each instruction of a word was run.  And the reference VM
# that watches the dictionary searched (bench/finds.c), for finding-NAME.
machine_tool(finds bench/finds.c)
function(compiled NAME)
    set(AT ${BUILD}/${NAME})
    set(CP ${AT}/compiling)
    report(compiling-${NAME}
            COMMAND           ${MKDIR} ${CP}
            COMMAND           ${COPY} ${AT}/boot/desk.img ${CP}/desk.img
            COMMAND           ${WITH} RUN_WORK=${CP}/work
                                $<TARGET_FILE:counting> ${AT}/boot/nuc.hex ${CP}/desk.img,${PICTURES}
                                < ${CMAKE_SOURCE_DIR}/bench/idle.fs > ${CP}/boot.out 2>&1
            COMMAND           costs ${AT}/boot/nuc.hex ${AT}/boot/nuc.tables ${AT}/boot/nuc.sizes ${CP}/work ${CP}
                                > ${CP}/costs.txt 2> ${CP}/costs.log
            COMMAND           ${CAT} ${CP}/boot.out ${CP}/costs.txt
            COMMAND           ${KEEP} compiling ${AT}/boot/nuc.hex ${CP}/work ${CP}/boot.out ${CP}/costs.txt
                                ${CP}/paths.txt ${CP}/loops.txt ${CP}/vars.txt
            VERBATIM)

    # `finding-64` and the like: what that boot asks the dictionary for.
    # The same machine is run on the reference VM that watches sfind
    # (bench/finds.c): how often it is asked, how many headers it looks at,
    # what it would look at were the dictionary several chains, and each
    # name by what was looked at for it.  It is in
    # build/NAME/compiling/finds.txt.
    report(finding-${NAME}
            COMMAND           ${MKDIR} ${CP}
            COMMAND           ${COPY} ${AT}/boot/desk.img ${CP}/desk.img
            COMMAND           ${WITH} FINDS=${CP}/finds.txt FINDS_NAMES=${AT}/boot/nuc.tables
                                $<TARGET_FILE:finds> ${AT}/boot/nuc.hex ${CP}/desk.img,${PICTURES}
                                < ${CMAKE_SOURCE_DIR}/bench/idle.fs > ${CP}/finds.out 2>&1
            COMMAND           ${CAT} ${CP}/finds.txt
            COMMAND           ${KEEP} finding ${AT}/boot/nuc.hex ${CP}/finds.txt
            VERBATIM)
endfunction()

# util/machines, a line a machine: the bits of a cell, the log2 of memory's
# cells, and the display, across and down and the cell it starts at.  The
# first of them is the default machine, the one the two builds are compared
# on, and that the reports are of.
file(STRINGS util/machines MACHINE_LINES REGEX "^[^#]")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS util/machines)
set(MACHINES)
foreach(LINE ${MACHINE_LINES})
    separate_arguments(SAID UNIX_COMMAND "${LINE}")
    list(GET SAID 0 CELL)
    list(GET SAID 1 ADDR)
    if(NOT MACHINES)
        set_property(GLOBAL PROPERTY DEFAULT_MACHINE ${CELL})
    endif()
    set(ITS_SIZES CELL_BITS=${CELL} ADDR_BITS=${ADDR})
    list(LENGTH SAID HOW_MUCH)
    if(HOW_MUCH GREATER 2)
        list(GET SAID 2 ACROSS)
        list(GET SAID 3 DOWN)
        list(GET SAID 4 STARTS)
        list(APPEND ITS_SIZES DISP_W=${ACROSS} DISP_H=${DOWN} DISP_BASE=${STARTS})
    endif()
    machine(${CELL} ${ITS_SIZES})
    machine_tests(${CELL})
    machine_made(${CELL})
    if(HOW_MUCH GREATER 2)
        examined(examine-${CELL} ${BUILD}/${CELL})      # the names it reads are a machine's that has a display
        shortened(shorter-${CELL} ${BUILD}/${CELL})
        listed(${CELL})
        mapped(${CELL})
        compiled(${CELL})
        if(ADDR EQUAL 16)                               # 64K cells
            picture_system(${CELL})
        endif()
    endif()
    list(APPEND MACHINES ${CELL})
endforeach()
list(GET MACHINES 0 FIRST)
plain(${FIRST})
builds_test(${FIRST})

# hexaforth runs a built nucleus when it is given no image.  util/image.c
# finds that nucleus (built_nucleus): BUILT is the build directory, and
# FIRST_MACHINE is the default machine.
set_source_files_properties(util/image.c PROPERTIES
        COMPILE_DEFINITIONS "BUILT=\"${BUILD}\";FIRST_MACHINE=\"${FIRST}\"")

# Tests of `hexaforth --typed` that run on the first machine only:
#
#   typed-unfinished    the file ends inside a definition
#   typed-forever       the last line is an infinite loop, stopped by the budget
#   typed-underflow     a line takes a cell from an empty stack
#   typed-exit-ok       the exit status is 0 for a file with no errors
#   typed-exit-error    the exit status is non-zero for a file with an error
#
# A machine that stops without halting is reported with the line of the
# file it was running.  CTest ignores the exit status of a test that has
# PASS_REGULAR_EXPRESSION, so the exit status has tests of its own.
machine_test(typed-unfinished ${FIRST} hexaforth --typed test/typed-unfinished.fs)
set_tests_properties(typed-unfinished PROPERTIES PASS_REGULAR_EXPRESSION "end of file inside")
add_test(NAME typed-forever
         COMMAND ${WITH} STOP_BUDGET=1000000 $<TARGET_FILE:hexaforth> --typed test/typed-forever.fs
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(typed-forever PROPERTIES PASS_REGULAR_EXPRESSION "it ran on.*while running line 5: forever")
needs(typed-forever hexaforth machine-${FIRST})
on_the_vm(typed-forever)
machine_test(typed-underflow ${FIRST} hexaforth --typed test/typed-underflow.fs)
set_tests_properties(typed-underflow PROPERTIES
                     PASS_REGULAR_EXPRESSION "stack out of range \\(data -1.*while running line 3: 1 drop drop")

# The trace (vm_stopped.h): `trace-on` typed after two definitions, which
# are then run.
#
#   typed-traced        each instruction of the two definitions is reported
#                       with its fields and the stacks, the call from one to
#                       the other is followed, a call to a word of the image
#                       is one line, and nothing is reported after `trace-off`
#   typed-trace-most    the trace stops after the number of reports that
#                       TRACE_MOST gives
#
# A `;` would end the regular expression, so the one the trace prints after
# an instruction with a return folded into it is matched by a dot.
machine_test(typed-traced ${FIRST} hexaforth --typed test/typed-traced.fs)
set_tests_properties(typed-traced PROPERTIES PASS_REGULAR_EXPRESSION
        "sum +false +N->IN +IN\\^N +->T +d\\+1 +r\\+0 +alu +data: 3 0 .*twice\\+1 +\\+ . +T->IN +IN\\+N +->T +d-1 +r-1 +RET +alu +data: 2 4 .*\\(loopdone\\).*exit +T->IN.*RET +alu +data: 6 .*\n6  ok.*4 twice \\. 8  ok")
add_test(NAME typed-trace-most
         COMMAND ${WITH} TRACE_MOST=5 $<TARGET_FILE:hexaforth> --typed test/typed-traced.fs
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(typed-trace-most PROPERTIES PASS_REGULAR_EXPRESSION "the trace stopped after 5 instructions")
needs(typed-trace-most hexaforth machine-${FIRST})
on_the_vm(typed-trace-most)
machine_test(typed-exit-ok ${FIRST} hexaforth --typed test/typed.fs)
machine_test(typed-exit-error ${FIRST} hexaforth --typed test/typed-thrown.fs)
set_tests_properties(typed-exit-error PROPERTIES WILL_FAIL TRUE)
