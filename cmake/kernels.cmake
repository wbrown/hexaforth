#
# The AArch64 kernels, for QEMU's virt machine: baremetal/hexaforth.S, the
# small one, and baremetal/hexaforth-fast.S, decoded by table, which behave
# the same.  A register of the AArch64 is a cell, so they are built for the
# machine of util/machines whose cell is 64 bits: its sizes are what the
# assembler is told, and its nucleus is the image a kernel has in it.
# Everything made is in build/kernel/.
#
#   check       runs each kernel against vm.c, and the desktop on QEMU with
#               each, with the others
#   run         boots KERNEL in QEMU, at its serial console (Ctrl-A X to
#               leave)
#   demo        boots it into the desktop, with a disk, a keyboard and a
#               pointer.  It wants a QEMU with a display; the serial
#               console types into the front window too, and ^Q leaves the
#               desktop for the serial prompt
#   demo-test   drives that desktop with no display and checks its screen,
#               as check does, on the kernel that is said here
#   demo-time   says when the desktop is ready and when its picture is made,
#               with the Mandrill on its disk, its grays, and its grays at
#               half its size
#   size        how big the kernel is
#
# and what those five are of is said when CMake is run, and kept until it
# is said otherwise (cmake -DMIPS=5 -DKERNEL=hexaforth-fast build/cmake):
#
#   KERNEL      which kernel: hexaforth, or hexaforth-fast
#   STRICT      the kernel halts on a fetch or a store outside memory
#   SCALE       each pixel that many across and down, for a screen whose
#               own pixels are small; demo-test then checks as well that
#               every pixel came out as a whole block
#   MIPS        that many million instructions a second and no more,
#               however fast the host is, evenly, so the machine is as slow
#               as one of that speed and as smooth.  It may have a fraction
#   DISK        the disk gives and takes that many thousand bytes a second
#               and no more, as a floppy did: about 40 for an 8-inch one at
#               double density, 23 for a 5 1/4-inch one, half of each at
#               single density.  It is QEMU that holds the drive to it, and
#               it is the rate and no more: no head moves and nothing has
#               to come round.  demo and demo-time take it
#
# Unless they are said, the machine is the one the project is there to
# show: the small kernel, each pixel drawn twice across and down, a million
# instructions a second, and an 8-inch floppy.  MIPS and DISK said as
# nothing hold the machine to no speed (cmake -DMIPS= -DDISK= -DSCALE=1
# build/cmake is the machine as fast as the host makes it).  A value said
# once is kept: cmake -USCALE -UMIPS -UDISK build/cmake has these defaults
# again.  The tests the target `check` runs are not of these: they say
# their own.
#
set(KERNELS_OF 64)
set(KERNEL hexaforth CACHE STRING "which kernel run, demo, demo-test, demo-time and size are of: hexaforth or hexaforth-fast")
option(STRICT "that kernel halts on a fetch or a store outside memory" OFF)
set(SCALE 2 CACHE STRING "how many across and down the bridge draws each pixel of the display")
set(MIPS 1 CACHE STRING "the million instructions a second that kernel is held to; none, if it is said as nothing")
set(DISK 40 CACHE STRING "the thousand bytes a second the disk is held to; none, if it is said as nothing")

if(KERNELS_OF IN_LIST MACHINES)

set(KB ${BUILD}/kernel)
set(OF_KERNEL ${BUILD}/virt)
set(LOAD 0x40080000)                # where QEMU's virt machine has a kernel start
set(KERNEL_SIZES ${SIZES_OF_${KERNELS_OF}})
list(TRANSFORM KERNEL_SIZES PREPEND -D)
set(CROSS_CLANG ${CLANG} --target=aarch64-none-elf -c ${KERNEL_SIZES})

# The machine the kernels are: QEMU's virt, which has the sizes of the
# machine of ${KERNELS_OF} bits and a disk of its own, a virtio block device.  A
# machine's ROM has the driver of the disk it starts from, so this one's
# system is not that machine's: it is made with ROM=virt-disk.fs, in
# build/virt/, as every system is (system).  machine-virt makes its image
# and the disk the desktop is given with it, whose names are that image's,
# and boot-system-virt the image it boots from a disk with and that disk:
# VIRT is the two.  It is made after the machine of ${KERNELS_OF} bits, since all of
# them write what fuse.h keeps in build/.
system(virt ${KERNELS_OF}
       SET ROM=virt-disk.fs
       AFTER machine-${KERNELS_OF} ${BUILD}/${KERNELS_OF}/nuc.hex)
machine_made(virt)
set(VIRT machine-virt boot-system-virt)

# What it is made of: the target examine-virt.  Its ROM has the virtio
# disk's driver in it and is the fullest there is, so this is where how long
# its kernel is, and what its ROM has left, is asked.
examined(examine-virt ${OF_KERNEL})
shortened(shorter-virt ${OF_KERNEL})

# A number that may have a fraction, times a whole number, as a whole
# number: CMake's own arithmetic is of whole numbers.
function(times RESULT NUMBER BY)
    if(NOT "${NUMBER}" MATCHES "^([0-9]*)\\.?([0-9]*)$")
        message(FATAL_ERROR "${NUMBER} is not a number")
    endif()
    set(UNITS "${CMAKE_MATCH_1}")
    set(PARTS "${CMAKE_MATCH_2}")
    string(LENGTH "${PARTS}" DIGITS)
    set(OF 1)
    while(DIGITS GREATER 0)
        math(EXPR OF "${OF} * 10")
        math(EXPR DIGITS "${DIGITS} - 1")
    endwhile()
    foreach(WHICH UNITS PARTS)                          # a number that starts with a nothing is not read as tens
        string(REGEX REPLACE "^0+" "" ${WHICH} "${${WHICH}}")
        if("${${WHICH}}" STREQUAL "")
            set(${WHICH} 0)
        endif()
    endforeach()
    math(EXPR IS "${UNITS} * ${BY} + ${PARTS} * ${BY} / ${OF}")
    set(${RESULT} ${IS} PARENT_SCOPE)
endfunction()

# The image a kernel has in it, as bytes; or its ROM alone, which is the
# first 4KB of the image the machine boots from a disk with: what is after
# a kernel is the machine's memory, so a kernel that has only the ROM in
# it starts with nothing else there (doc/boot.md).  And the bridge that
# shows the display on QEMU (baremetal/ramfb.S), which a kernel with a
# display is linked after, since the machine's memory runs on from the end
# of the kernel.  A bridge that draws each pixel more than once across and
# down is another bridge, named for how many.
tool(hex2bin util/hex2bin.c util/image.c)
add_custom_command(
        OUTPUT            ${KB}/image.bin ${KB}/rom.bin
        COMMAND           ${MKDIR} ${KB}
        COMMAND           hex2bin ${OF_KERNEL}/nuc.hex ${KB}/image.bin
        COMMAND           hex2bin --rom ${OF_KERNEL}/boot/nuc.hex ${KB}/rom.bin
        DEPENDS           hex2bin ${VIRT} ${OF_KERNEL}/nuc.hex ${OF_KERNEL}/boot/nuc.hex)
function(bridge SCALED)
    set(BRIDGE ${KB}/ramfb.o)
    set(TOLD)
    if(NOT SCALED EQUAL 1)
        set(BRIDGE ${KB}/ramfb-x${SCALED}.o)
        set(TOLD -DSCALE=${SCALED})
    endif()
    set(BRIDGE ${BRIDGE} PARENT_SCOPE)
    get_property(BRIDGES GLOBAL PROPERTY BRIDGES)
    if(BRIDGE IN_LIST BRIDGES)
        return()
    endif()
    set_property(GLOBAL APPEND PROPERTY BRIDGES ${BRIDGE})
    add_custom_command(
            OUTPUT            ${BRIDGE}
            COMMAND           ${MKDIR} ${KB}
            COMMAND           ${CROSS_CLANG} ${TOLD} ${CMAKE_SOURCE_DIR}/baremetal/ramfb.S -o ${BRIDGE}
            DEPENDS           baremetal/ramfb.S)
endfunction()
bridge(1)
bridge(${SCALE})
get_property(BRIDGES GLOBAL PROPERTY BRIDGES)
add_custom_target(kernel-parts DEPENDS ${KB}/image.bin ${KB}/rom.bin ${BRIDGES})

# A kernel: which of the two, and how it is built.
#
#   DISPLAY     with the bridge, which it calls
#   PROFILE     the same, counting how often each instruction is executed.
#               The counts are kept 4MB past the stacks, which is clear of
#               the pixels only when they are not scaled, so this is always
#               the plain bridge.  (Nothing reads them now: the target
#               `profile` counts on the reference VM.)
#   STRICT      halting on a fetch or a store outside memory
#   MIPS n      held to that many million instructions a second
#   SCALE n     with the bridge that draws each pixel that many times
#   ROM         with only the kernel's ROM in it and not the image: the
#               machine reads the rest from its disk
#
# Each is another file, named for what it is; KERNEL_FILE is its name, and
# the target kernel-NAME makes what has it: NAME.elf, NAME.bin, which is
# what QEMU's machine has in memory, and NAME.symbols, where its symbols
# are.
function(kernel WHICH)
    cmake_parse_arguments(IS "DISPLAY;PROFILE;STRICT;ROM" "MIPS;SCALE" "" ${ARGN})
    set(NAME ${WHICH})
    set(TOLD)
    set(BRIDGE)
    set(IMAGE ${KB}/image.bin)
    if(IS_ROM)
        string(APPEND NAME -rom)
        set(IMAGE ${KB}/rom.bin)
    endif()
    if(IS_PROFILE)
        string(APPEND NAME -profile)
        list(APPEND TOLD -DBRIDGE -DPROFILE)
        bridge(1)
    elseif(IS_DISPLAY)
        string(APPEND NAME -display)
        list(APPEND TOLD -DBRIDGE)
        if(NOT IS_SCALE)
            set(IS_SCALE 1)
        endif()
        bridge(${IS_SCALE})
    endif()
    if(IS_STRICT)
        string(APPEND NAME -strict)
        list(APPEND TOLD -DSTRICT)
    endif()
    if(IS_MIPS)
        string(APPEND NAME -m${IS_MIPS})
        times(IPS ${IS_MIPS} 1000000)
        list(APPEND TOLD -DIPS=${IPS})
    endif()
    if(IS_DISPLAY AND NOT IS_SCALE EQUAL 1)
        string(APPEND NAME -x${IS_SCALE})
    endif()
    set(KERNEL_FILE ${KB}/${NAME} PARENT_SCOPE)
    if(TARGET kernel-${NAME})
        return()
    endif()
    set(FILE ${KB}/${NAME})
    add_custom_command(
            OUTPUT            ${FILE}.o
            COMMAND           ${CROSS_CLANG} "-DIMAGE=\"${IMAGE}\"" ${TOLD}
                                ${CMAKE_SOURCE_DIR}/baremetal/${WHICH}.S -o ${FILE}.o
            DEPENDS           baremetal/${WHICH}.S kernel-parts ${IMAGE}
            VERBATIM)
    add_custom_command(
            OUTPUT            ${FILE}.elf ${FILE}.bin ${FILE}.symbols
            COMMAND           ${LLD} -Ttext=${LOAD} -e _start -o ${FILE}.elf ${BRIDGE} ${FILE}.o
            COMMAND           ${OBJCOPY} -O binary ${FILE}.elf ${FILE}.bin
            COMMAND           ${NM} ${FILE}.elf > ${FILE}.symbols
            DEPENDS           ${FILE}.o kernel-parts ${BRIDGE})
    add_custom_target(kernel-${NAME} DEPENDS ${FILE}.elf ${FILE}.bin ${FILE}.symbols)
    owned(kernel-${NAME} ${FILE}.elf ${FILE}.bin ${FILE}.symbols)
endfunction()

# What is built for the kernels' machine and is not the machine's own VM
# stopping where it is strict: as the kernels are built, unless they are
# told to be.
function(kernel_tool TOOL)
    tool(${TOOL} ${ARGN})
    target_compile_definitions(${TOOL} PRIVATE ${SIZES_OF_${KERNELS_OF}})
endfunction()

# A kernel against vm.c, which is the machine: the cases difftest_dump
# writes, each with what vm.c left of it, are run on the kernel under
# Unicorn (baremetal/difftest.c).
kernel_tool(difftest baremetal/difftest.c)
find_library(UNICORN unicorn PATHS ${BREW_PREFIX}/lib)
if(UNICORN_HEADER AND UNICORN)      # without it difftest does not build, and says what it has not
    target_include_directories(difftest PRIVATE ${UNICORN_HEADER})
    target_link_libraries(difftest ${UNICORN})
else()
    left_out(difftest)
endif()
add_custom_command(
        OUTPUT            ${KB}/cases.bin
        COMMAND           ${MKDIR} ${KB}
        COMMAND           difftest_dump ${KB}/cases.bin
        DEPENDS           difftest_dump)

# What drives the desktop on QEMU and looks at its screen (baremetal/qemu.c:
# the checks are the reference VM's too, test/deskcheck.c), what only
# watches it, and what says how big a kernel is.
set(QEMU_SOURCES baremetal/qemu.c test/deskcheck.c test/tables.c test/dither.c bench/tiff.c vm_opcodes.c vm_debug.c)
kernel_tool(guitest baremetal/guitest.c ${QEMU_SOURCES})
kernel_tool(desktime baremetal/desktime.c ${QEMU_SOURCES})
target_compile_definitions(guitest PRIVATE QEMU_AARCH64="${QEMU_AARCH64}")
target_compile_definitions(desktime PRIVATE QEMU_AARCH64="${QEMU_AARCH64}")
tool(kernel_size baremetal/size.c)

# The tests: each kernel against vm.c; the desktop on QEMU with the small
# one, which halts if it leaves memory; and the desktop with each started
# from its ROM alone.  The fast kernel's desktop is checked only so: it is
# the same checks after a boot and a compile as well, and the kernels'
# devices are the same code.  A desktop's screens, and its copy of the
# disk, are kept in a directory named for the kernel.
#
# A kernel has the machine's image in it, so a test of one needs the kernel
# built, which is built again when the image is; kernel-tests is the cases,
# and the directories the screens are kept in.  The desktop on QEMU is
# given the machine's disk as well, which has the names that go with that
# image: a kernel and a disk of two builds do not know each other's names.
function(kernel_test WHICH)
    kernel(${WHICH})
    add_test(NAME kernel-${WHICH}
             COMMAND difftest ${KERNEL_FILE}.bin ${KB}/cases.bin ${KERNEL_FILE}.symbols)
    needs(kernel-${WHICH} kernel-${WHICH} difftest kernel-tests)
endfunction()
function(qemu_test WHICH)
    kernel(${WHICH} DISPLAY ${ARGN})
    get_filename_component(NAME ${KERNEL_FILE} NAME)
    set(ON ${OF_KERNEL}/disk.img)
    set(BOOTS)
    if("ROM" IN_LIST ARGN)              # it starts from the disk, and compiles its desktop from there:
        set(ON ${OF_KERNEL}/boot/desk.img)
        set(BOOTS boot)                 # it is to say what it compiles, and its screen to show it
    endif()
    add_test(NAME qemu-${NAME}
             COMMAND ${WITH} --unset=DISK SCALE=1
                     $<TARGET_FILE:guitest> ${KERNEL_FILE}.elf ${ON} ${PICTURES} ${PICTURE} forth
                     ${KB}/screens-${NAME} ${BOOTS}
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(qemu-${NAME} PROPERTIES TIMEOUT ${QEMU_TEST_SECONDS})
    set_property(GLOBAL APPEND PROPERTY SCREENS ${KB}/screens-${NAME})
    needs(qemu-${NAME} kernel-${NAME} guitest kernel-tests ${VIRT} picture pictures)
endfunction()
kernel_test(hexaforth)
kernel_test(hexaforth-fast)
qemu_test(hexaforth STRICT)
qemu_test(hexaforth STRICT ROM)
qemu_test(hexaforth-fast ROM)
get_property(SCREENS GLOBAL PROPERTY SCREENS)
add_custom_target(kernel-tests
        COMMAND           ${MKDIR} ${SCREENS}
        DEPENDS           ${KB}/cases.bin)

# The kernel that is said when CMake is run, and what is done with it.
set(SAID)
if(STRICT)
    list(APPEND SAID STRICT)
endif()
if(MIPS)
    list(APPEND SAID MIPS ${MIPS})
endif()
set(SLOW)
if(DISK)
    times(RATE ${DISK} 1024)
    set(SLOW ",throttling.bps-read=${RATE},throttling.bps-write=${RATE}")
endif()
set(QEMU ${QEMU_AARCH64} -M virt -cpu cortex-a53 -nic none)
# QEMU is run where CMake builds: a file its monitor is told to write is
# written there.
set(QEMU_AT WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

kernel(${KERNEL} ${SAID})
report(run
        COMMAND           ${QEMU} -nographic -kernel ${KERNEL_FILE}.elf
        ${QEMU_AT}
        USES_TERMINAL)
report(size
        COMMAND           kernel_size ${KERNEL_FILE}.symbols ${KERNEL_FILE}.bin > ${KERNEL_FILE}.size.txt
        COMMAND           ${CAT} ${KERNEL_FILE}.size.txt
        COMMAND           ${KEEP} size ${KERNEL_FILE}.bin ${KERNEL_FILE}.size.txt)

# The desktop on QEMU with a display: the target NAME boots the kernel last
# said (kernel) with a disk, the pictures' disk, a keyboard and a pointer.
# The disk is its own, a copy of FROM: it is made of that when that is made
# again, and is otherwise as the desktop left it.  The machine's disks are
# its block devices in the order of their places (forth/virt-disk.fs), so
# each is said its place: the disk it is given or starts from in the first,
# and the pictures' disk, which nothing writes to, in the next.
set(SLOW_READ)
if(DISK)
    set(SLOW_READ ",throttling.bps-read=${RATE}")
endif()
function(desktop_shown NAME FROM)
    owner(MAKES_IT ${FROM})
    add_custom_command(
            OUTPUT            ${KB}/${NAME}.img
            COMMAND           ${COPY} ${FROM} ${KB}/${NAME}.img
            DEPENDS           ${MAKES_IT} ${FROM})
    report(${NAME}
            COMMAND           ${QEMU} -m 256 -serial mon:stdio -device ramfb -global virtio-mmio.force-legacy=false
                                -drive "file=${KB}/${NAME}.img,if=none,format=raw,id=hd0${SLOW}"
                                -device virtio-blk-device,drive=hd0,bus=virtio-mmio-bus.0
                                -drive "file=${PICTURES},if=none,format=raw,id=hd1,readonly=on${SLOW_READ}"
                                -device virtio-blk-device,drive=hd1,bus=virtio-mmio-bus.1
                                -device virtio-keyboard-device -device virtio-tablet-device
                                -kernel ${KERNEL_FILE}.elf
            DEPENDS           ${KB}/${NAME}.img
            ${QEMU_AT}
            USES_TERMINAL
            VERBATIM)
endfunction()
kernel(${KERNEL} DISPLAY SCALE ${SCALE} ${SAID})
desktop_shown(demo ${OF_KERNEL}/disk.img)

# demo-boot is the same machine with nothing in its memory but the kernel's
# ROM: it reads its image from the disk, compiles the applications and the
# desktop from their source there, saying on the screen what it is at, and
# opens the desktop (doc/boot.md).
kernel(${KERNEL} DISPLAY SCALE ${SCALE} ROM ${SAID})
desktop_shown(demo-boot ${OF_KERNEL}/boot/desk.img)

set(STRICTLY)
if(STRICT)
    set(STRICTLY STRICT)
endif()
kernel(${KERNEL} DISPLAY SCALE ${SCALE} ${STRICTLY})
report(demo-test
        COMMAND           ${MKDIR} ${KB}/screens
        COMMAND           ${WITH} --unset=DISK SCALE=${SCALE}
                            $<TARGET_FILE:guitest> ${KERNEL_FILE}.elf ${OF_KERNEL}/disk.img ${PICTURES} ${PICTURE} forth
                            ${KB}/screens)

set(HELD)
if(MIPS)
    set(HELD MIPS ${MIPS})
endif()
kernel(${KERNEL} DISPLAY ${HELD})
# demo-time runs three times, with a different picture on the disk each
# time: the Mandrill, its grays, and its grays at half size.  The gray
# files are smaller to read, and dither to the same bitmaps as the Mandrill
# at full and half size.  Each run's files are in build/kernel/timed-PIC.
set(TIMES)
set(TIMED_DISKS)
foreach(PIC colour gray small)
    set(PICTURES_OF ${PICTURES})
    set(FROM ${PICTURE})
    if(PIC STREQUAL "gray")
        set(FROM ${GRAYS})
    elseif(PIC STREQUAL "small")
        set(FROM ${SMALL})
    endif()
    if(NOT PIC STREQUAL "colour")           # a pictures' disk with that one on it, under the label the viewer asks for
        set(PICTURES_OF ${KB}/pictures-${PIC}.img)
        pictures_disk(${PICTURES_OF} ${FROM})
    endif()
    list(APPEND TIMES
         COMMAND ${MKDIR} ${KB}/timed-${PIC}
         COMMAND ${ECHO} "${PIC}: ${FROM}"
         COMMAND ${WITH} DISK=${DISK}
                 $<TARGET_FILE:desktime> ${KERNEL_FILE}.elf ${KERNEL_FILE}.symbols ${OF_KERNEL}/disk.img
                 ${PICTURES_OF} ${FROM} forth ${OF_KERNEL}/nuc.tables ${KB}/timed-${PIC})
    list(APPEND TIMED_DISKS ${PICTURES_OF})
endforeach()
report(demo-time ${TIMES}
        DEPENDS           ${TIMED_DISKS})

# The kernel that counts: nothing reads its counts, and the target
# kernel-hexaforth-profile still builds it.
kernel(hexaforth PROFILE)

endif()
