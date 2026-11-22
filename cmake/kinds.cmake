#
# The kinds of thing that are made here, each said once: a tool, an image,
# which target makes a file, a report, a test and what it needs built, and
# another's source fetched.  Whatever is one of these is said with what says
# it here, and not by copying another.  The parts after this one say what
# there is of each.
#

# A tool or a test.  Every one is built by a build of no target, so that
# after one no tool is older than its source: a tool that is run by hand is
# then the tool the source says.  One that cannot be built everywhere, or
# not without fetching what it is made of, is said to be left out of that
# build where it is defined (left_out).  The trace of every instruction
# (DEBUG) is in none of them: what has it says so (hexaforth-debug, and the
# test of the instructions).
function(tool NAME)
    add_executable(${NAME} ${ARGN})
    target_compile_options(${NAME} PRIVATE -w)
    target_link_libraries(${NAME} m)
endfunction()
function(left_out)
    set_target_properties(${ARGN} PROPERTIES EXCLUDE_FROM_ALL TRUE)
endfunction()

# A tool or a test that has the machine in it, or reads a machine's image.
# It is one program for every machine: the machine is the one whose image it
# is given, which the image says beside it (util/image.h), and no size of a
# machine is compiled into it.  Its machine stops at a fetch or a store
# outside memory (STRICT).  What every such tool has (COMMON_SOURCES: the
# words, the dictionary, a stack printed, an image read) is compiled once,
# and a tool says the sources that are its own.
set(COMMON_SOURCES vm_opcodes.c vm_debug.c util/stack.c util/image.c)
add_library(common STATIC ${COMMON_SOURCES})
target_compile_options(common PRIVATE -w)
function(machine_tool TOOL)
    set(OWN ${ARGN})
    list(REMOVE_ITEM OWN ${COMMON_SOURCES})
    tool(${TOOL} ${OWN})
    target_compile_definitions(${TOOL} PRIVATE STRICT)
    target_link_libraries(${TOOL} common)
endfunction()

# A tool that has the machine's step in it (util/fuse.h).  It too is one
# program for every machine: its cell is that of the image it is given, and
# fuse.h says what the rest of the machine it tries things on is.
function(stepping_tool TOOL)
    tool(${TOOL} ${ARGN})
    target_include_directories(${TOOL} PRIVATE util)
endfunction()

# An image is made from every Forth file: nuc.fs includes system.fs and
# through it the rest.  So an image is out of date when any of them
# changes, and a file that is added is found the next time anything is
# built.  test_cases.fs is made here, and only its own image needs it:
# that image is of cross.fs, the machine and the tests, and no other file.
file(GLOB FORTH_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/forth/*.fs)
list(FILTER FORTH_SOURCES EXCLUDE REGEX "/test_cases\\.fs$")

# cross.fs asks the machine which instructions do the work of which
# (util/fuse.h, which has vm.c in it), through gforth's C interface.  So an
# image is out of date when the machine changes too.  gforth builds that C
# with libtool; and where gforth is Homebrew's, the compiler has to be told
# where Homebrew's headers are, gforth's own among them.
set(MACHINE_SOURCES util/fuse.h util/named.h util/room.h vm.c vm_loop.h vm_step.h vm.h vm_opcodes.c
                    vm_opcodes.h vm_instruction.h vm_constants.h util/stack.c)
set(WITH_HEADERS)                   # what is put before a command that has gforth build C
if(BREW)
    set(BREW_CPATH ${BREW_PREFIX}/include)
    if(DEFINED ENV{CPATH})
        set(BREW_CPATH ${BREW_CPATH}:$ENV{CPATH})
    endif()
    set(WITH_HEADERS ${WITH} CPATH=${BREW_CPATH})
endif()
set(RUN_GFORTH ${WITH_HEADERS} ${GFORTH})

# An image: cross.fs, run by gforth on a program, with what it writes in a
# directory and what it says in a log there.  Every image is made by this,
# and it is the one place that says how.
#
#   image(DIRECTORY
#         [SOURCE file]         the program, a file of forth/: nuc.fs if none
#                               is said
#         [WORDS file]          the machine cross.fs reads: forth/basewords.fs
#                               if none is said
#         [SET name=value...]   what cross.fs is told besides, in its
#                               environment (BOOT=on, ROM=virt-disk.fs)
#         [MAKES kind...]       the files it is made for, of those cross.fs
#                               writes, by what each ends in: hex if none is
#                               said
#         [FIRST command]       a command that is run before cross.fs
#         [FROM file...]        the source it is out of date after: every
#                               file of forth/ if none is said
#         [AFTER what...]       what else it is made after, files and targets
#         [LOG name]            its log: cross.log if none is said
#         [IS text])            what is said as it is made
#
# An image is out of date when the machine changes too (MACHINE_SOURCES).
# The files it is made for are left in IMAGE_FILES.
function(image DIRECTORY)
    cmake_parse_arguments(IT "" "SOURCE;WORDS;LOG;IS" "SET;MAKES;FIRST;FROM;AFTER" ${ARGN})
    if(NOT IT_SOURCE)
        set(IT_SOURCE nuc.fs)
    endif()
    if(NOT IT_WORDS)
        set(IT_WORDS ./basewords.fs)
    endif()
    if(NOT IT_MAKES)
        set(IT_MAKES hex)
    endif()
    if(NOT IT_FROM)
        set(IT_FROM ${FORTH_SOURCES})
    endif()
    if(NOT IT_LOG)
        set(IT_LOG cross.log)
    endif()
    get_filename_component(STEM ${IT_SOURCE} NAME_WE)
    set(FILES)
    foreach(KIND ${IT_MAKES})
        list(APPEND FILES ${DIRECTORY}/${STEM}.${KIND})
    endforeach()
    set(BEFORE)
    if(IT_FIRST)
        set(BEFORE COMMAND ${IT_FIRST})
    endif()
    set(WORDS_FILE ${IT_WORDS})
    if(IT_WORDS MATCHES "^\\./")                        # a file of forth/, said from there
        string(REGEX REPLACE "^\\./" "${CMAKE_SOURCE_DIR}/forth/" WORDS_FILE ${IT_WORDS})
    endif()
    add_custom_command(
            OUTPUT            ${FILES}
            COMMAND           ${MKDIR} ${DIRECTORY}
            ${BEFORE}
            COMMAND           ${WITH} BUILD=${DIRECTORY}/ ${IT_SET}
                                ${RUN_GFORTH} ./cross.fs ${IT_WORDS} ./${IT_SOURCE} > ${DIRECTORY}/${IT_LOG} 2>&1
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/forth
            COMMENT           "${IT_IS} (what cross.fs says is in ${DIRECTORY}/${IT_LOG})"
            DEPENDS           ${IT_FROM} ${WORDS_FILE} ${MACHINE_SOURCES} ${IT_AFTER})
    set(IMAGE_FILES ${FILES} PARENT_SCOPE)
endfunction()

# Which target makes a file: said where the file is made (owned), and asked
# (owner) by what is made of the file, and by a report of every file it
# names (report).  A file that no target is said to make has no owner.
function(owned TARGET)
    foreach(FILE ${ARGN})
        string(MD5 KEY "${FILE}")
        set_property(GLOBAL PROPERTY OWNER_${KEY} ${TARGET})
    endforeach()
endfunction()
function(owner RESULT FILE)
    string(MD5 KEY "${FILE}")
    get_property(OWNER GLOBAL PROPERTY OWNER_${KEY})
    set(${RESULT} "${OWNER}" PARENT_SCOPE)
endfunction()

# The files cross.fs writes beside an image in a directory, which are all
# the one target's that makes the image.
function(image_owned TARGET DIRECTORY)
    foreach(KIND hex tables lst sizes inline public needs shadows)
        owned(${TARGET} ${DIRECTORY}/nuc.${KIND})
    endforeach()
endfunction()

# What a target measures is kept (util/keep.c): a target that measures
# writes what it found where it always does, and so over what it found the
# time before, which is then not there to be compared with.  Each such
# target ends by keeping what it wrote in build/kept/, under the image it
# was measured of as that image is, a number of its bytes, and the target's
# name: a change that makes another image is kept beside the one before
# and not over it.  build/kept/log.txt has a line for each time, in order,
# with the commit the tree was at, and is what is read to find the one
# before.  KEEP is put before a label, the image and the files.
set(KEPT ${BUILD}/kept)
tool(keep util/keep.c)
set(KEEP keep ${GIT} ${KEPT})

# A report: a target that runs what it is told and says what was found,
# said as add_custom_target is.  It is run from where the source is, unless
# it says where.  What it depends on is not said: it is the target that
# makes each file its commands name, a machine's image or its disks or the
# pictures or a kernel (owned), so that each is made, and made in one place,
# before the report reads it.  Files said with commas between them, as a
# machine's disks are, are each looked at, and so is one said after an
# equals sign.  A tool it runs is built first, as CMake has it.
function(report NAME)
    set(WHERE)
    if(NOT "WORKING_DIRECTORY" IN_LIST ARGN)
        set(WHERE WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    endif()
    add_custom_target(${NAME} ${ARGN} ${WHERE})
    set(OWNERS)
    foreach(SAID ${ARGN})
        string(REGEX REPLACE "[,=]" ";" FILES "${SAID}")
        foreach(FILE ${FILES})
            owner(OWNER ${FILE})
            if(OWNER)
                list(APPEND OWNERS ${OWNER})
            endif()
        endforeach()
    endforeach()
    list(REMOVE_DUPLICATES OWNERS)
    if(NOT OWNERS)
        message(FATAL_ERROR "the report ${NAME} names nothing that is made: it would be run on whatever was there")
    endif()
    add_dependencies(${NAME} ${OWNERS})
endfunction()

# A test is CTest's (add_test), and says what it needs built (needs).
enable_testing()

# How long a test may run before it is stopped and fails, so that a hung
# test (a machine running wild, a desktop that never comes up) does not
# hang check.  Tests on the host take seconds and QEMU's take minutes; each
# limit is at least ten times that.  A test that hits its limit has hung,
# or has got much slower, which is worth looking into either way.  What a
# test needs built may take as long as building does.
set(TEST_SECONDS 60)
set(QEMU_TEST_SECONDS 600)
set(BUILD_SECONDS 3600)

# What a test needs built: the targets that make what it runs and what it
# reads.  Every test says them, here and nowhere else, and is run only when
# they have been built as the source now has them.  Each target is built by
# a test of its own, built-TARGET, which the tests that need it wait for
# (CTest's fixtures): so however a test is run, by the target check or by
# `ctest -R` with its name, what it needs is built first, and if that does
# not build the test is not run, and is said to have failed.  It is never
# run on what was built before the source changed.  One target is built at
# a time.
function(needs TEST)
    foreach(WHAT ${ARGN})
        get_property(BUILT GLOBAL PROPERTY BUILT_FOR_TESTS)
        if(NOT WHAT IN_LIST BUILT)
            set_property(GLOBAL APPEND PROPERTY BUILT_FOR_TESTS ${WHAT})
            add_test(NAME built-${WHAT}
                     COMMAND ${WITH} --unset=MAKEFLAGS --unset=MFLAGS --unset=MAKELEVEL
                             ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target ${WHAT})
            set_tests_properties(built-${WHAT} PROPERTIES
                                 FIXTURES_SETUP built-${WHAT} RESOURCE_LOCK the-build TIMEOUT ${BUILD_SECONDS})
        endif()
        set_property(TEST ${TEST} APPEND PROPERTY FIXTURES_REQUIRED built-${WHAT})
    endforeach()
endfunction()

# The tests that need only the reference VM, with no QEMU and no Verilog,
# have a label, and the target `quick` runs those (cmake/check.cmake).  A
# test is given it here.
set(ON_THE_VM vm)
function(on_the_vm)
    set_property(TEST ${ARGN} APPEND PROPERTY LABELS ${ON_THE_VM})
endfunction()

# Another's source, which is not kept here, is fetched as it was at a
# commit, into a directory: the target NAME fetches it, when what is made of
# it is first asked for, and nothing is built of it there.
include(ExternalProject)
function(source_fetched NAME FROM COMMIT AT)
    ExternalProject_Add(${NAME}
            GIT_REPOSITORY    ${FROM}
            GIT_TAG           ${COMMIT}
            SOURCE_DIR        ${AT}
            CONFIGURE_COMMAND ""
            BUILD_COMMAND     ""
            INSTALL_COMMAND   ""
            EXCLUDE_FROM_ALL  TRUE)
endfunction()
