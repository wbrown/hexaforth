#
# Disks, and what goes on them: what makes a disk, the source that is laid
# out to go on one, and the pictures, which are fetched and are on a disk
# of their own.
#

# What makes a disk (util/mkdisk.c).
tool(mkdisk util/mkdisk.c util/image.c)

# The source that goes on a disk as blocks (doc/boot.md), to be compiled
# when it is wanted, is written to the width of a block's line: every file
# of forth/ but cross.fs's own (cross.fs, and the machine it reads,
# basewords.fs), the kernel's (nuc.fs), the lists of what a system includes
# (system.fs, picture.fs), and what is made (fused.fs, folds.fs), whose
# makers lay it out themselves.  The target `wrap` fits them to it
# (util/wrap.c), writing the files again; what it changed is read in git's
# diff before it is kept.
set(DISK_SOURCES ${FORTH_SOURCES})
list(FILTER DISK_SOURCES EXCLUDE REGEX "/(cross|basewords|nuc|system|picture|fused|folds)\\.fs$")
tool(wrap-source util/wrap.c)
add_custom_target(wrap
        COMMAND           wrap-source ${DISK_SOURCES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/forth
        VERBATIM)

# A picture of the USC-SIPI image database is fetched as the database has
# it, by its number there: a TIFF that is not compressed, which is what the
# system reads.  The Mandrill is the one the tests and the desktop are given.
function(fetched FILE NUMBER)
    add_custom_command(
            OUTPUT            ${FILE}
            COMMAND           ${MKDIR} ${BUILD}
            COMMAND           ${CURL} -sSfL --remove-on-error -o ${FILE}
                                "https://sipi.usc.edu/database/download.php?vol=misc&img=${NUMBER}"
            VERBATIM)
endfunction()
set(PICTURE ${BUILD}/mandrill.tiff)
fetched(${PICTURE} 4.2.03)
add_custom_target(picture DEPENDS ${PICTURE})
owned(picture ${PICTURE})

# A disk is no more than a floppy holds (DISK_FLOPPY, vm_constants.h), and
# the pictures are on one of their own: an index and the Mandrill under
# its label, which the viewer finds it by on whichever disk has it
# (mkdisk --disk; doc/disk.md, Several disks).  It is a machine's second
# disk, wherever a machine is given the picture.  pictures_disk makes such
# a disk of any picture, under the Mandrill's label, which is the one the
# viewer asks for.
function(pictures_disk DISK FROM)
    get_filename_component(AT ${DISK} DIRECTORY)
    get_filename_component(LOG ${DISK} NAME_WE)
    add_custom_command(
            OUTPUT            ${DISK}
            COMMAND           ${MKDIR} ${AT}
            COMMAND           mkdisk --disk ${DISK} mandrill.tiff=${FROM} > ${AT}/${LOG}.log 2>&1
            DEPENDS           mkdisk picture ${FROM})
endfunction()
set(PICTURES ${BUILD}/pictures.img)
pictures_disk(${PICTURES} ${PICTURE})
add_custom_target(pictures DEPENDS ${PICTURES})
owned(pictures ${PICTURES})

# The Mandrill as one-sample gray TIFFs, using the dither's own grays: at
# full size, and at half size.  bench/grays.c writes them; the benchmarks
# have them on a disk of their own, and demo-time times the desktop with
# each.
set(GRAYS ${BUILD}/mandrill-grays.tiff)
set(SMALL ${BUILD}/mandrill-small.tiff)
tool(picture-grays bench/grays.c bench/tiff.c test/dither.c)
add_custom_command(
        OUTPUT            ${GRAYS} ${SMALL}
        COMMAND           picture-grays ${PICTURE} ${GRAYS} ${SMALL} > ${BUILD}/mandrill-grays.log 2>&1
        DEPENDS           picture-grays picture ${PICTURE})

# A disk for the desktop, made as QEMU's is: block 1 for the editor, and an
# image's tables of names.  The picture is not on it: that is the
# pictures' disk (above).  mkdisk's output goes to a .log file of the same
# name beside it.
function(desk_disk DISK TABLES)
    get_filename_component(AT ${DISK} DIRECTORY)
    get_filename_component(LOG ${DISK} NAME_WE)
    add_custom_command(
            OUTPUT            ${DISK}
            COMMAND           ${MKDIR} ${AT}
            COMMAND           mkdisk ${DISK} ${CMAKE_SOURCE_DIR}/baremetal/block1.fs ${TABLES}
                                > ${AT}/${LOG}.log 2>&1
            DEPENDS           mkdisk baremetal/block1.fs ${TABLES})
endfunction()

# Two more of that database, smoother than the Mandrill, for what is
# measured of pictures (tiles-report): Peppers and the F-16.  Nothing else
# reads them.
set(PEPPERS ${BUILD}/peppers.tiff)
set(AIRPLANE ${BUILD}/airplane.tiff)
fetched(${PEPPERS} 4.2.07)
fetched(${AIRPLANE} 4.2.05)
