#
# What clangd says of the C: the target `clangd`.
#
# A language server says what it finds wrong with a file each time the file
# is changed, so what it says of the tree as it is would be said again
# after every change.  This asks it of every file of C that is compiled as
# a target, a file at a time, as it is configured (.clangd) and as CMake
# says the file is compiled (compile_commands.json, which CMakeLists.txt
# has written); keeps what it printed of each in build/clangd/; and prints
# what it found wrong and of how many files (util/clangd.c), failing if it
# found anything.  It is of the targets the parts before this one make, so
# a part that compiles C is read before it.
#
# It is asked for its warnings as well as its errors, which it leaves out
# unless told.  It is not asked to try each of its features at every word
# of the file (--check-locations), which it does unless told: one of those
# is each change it can offer to make to the source, and its offer to turn
# round the two sides of an operator fails on an operator that is in a
# macro (DISK_DRIVE_OF, vm_constants.h).  Nothing here asks it for such a
# change, and a server does not try one until it is asked.
#
# It is asked of every header too, in each directory that has a file of C:
# a header is not compiled by itself, but clangd reads it by itself in a
# session, so it is to include what it needs.  What is fetched or made in
# build/ (the 68000, bench-m68k) is not asked of: it is another's source,
# and is not changed here.
#
tool(clangd_said util/clangd.c)
get_property(EVERY_TARGET DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
set(C_FILES)
foreach(TARGET ${EVERY_TARGET})
    get_target_property(KIND ${TARGET} TYPE)
    if(NOT KIND MATCHES "^(EXECUTABLE|STATIC_LIBRARY|SHARED_LIBRARY|MODULE_LIBRARY|OBJECT_LIBRARY)$")
        continue()
    endif()
    get_target_property(SOURCES ${TARGET} SOURCES)
    foreach(SOURCE ${SOURCES})
        if(SOURCE MATCHES "\\.c$")
            get_filename_component(SOURCE ${SOURCE} ABSOLUTE)
            string(FIND "${SOURCE}" "${BUILD}/" MADE)
            if(NOT MADE EQUAL 0)
                list(APPEND C_FILES ${SOURCE})
            endif()
        endif()
    endforeach()
endforeach()
list(REMOVE_DUPLICATES C_FILES)
set(C_DIRECTORIES)
foreach(FILE ${C_FILES})
    get_filename_component(WHERE ${FILE} DIRECTORY)
    list(APPEND C_DIRECTORIES ${WHERE})
endforeach()
list(REMOVE_DUPLICATES C_DIRECTORIES)
foreach(WHERE ${C_DIRECTORIES})
    file(GLOB HEADERS CONFIGURE_DEPENDS ${WHERE}/*.h)
    list(APPEND C_FILES ${HEADERS})
endforeach()
list(SORT C_FILES)
if(CLANGD)
    set(CS ${BUILD}/clangd)
    set(ASKED)
    set(SAID)
    foreach(FILE ${C_FILES})
        file(RELATIVE_PATH LOG ${CMAKE_SOURCE_DIR} ${FILE})
        string(REPLACE "/" "-" LOG ${LOG})
        set(LOG ${CS}/${LOG}.log)
        list(APPEND ASKED COMMAND ${CLANGD} --check=${FILE} --check-warnings --check-locations=0
                                    > ${LOG} 2>&1 || ${CMAKE_COMMAND} -E true)
        list(APPEND SAID ${LOG})
    endforeach()
    add_custom_target(clangd
            COMMAND           ${MKDIR} ${CS}
            ${ASKED}
            COMMAND           clangd_said ${SAID}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            VERBATIM)
else()
    add_custom_target(clangd COMMAND ${ECHO} "clangd: not run: there is no clangd")
endif()
