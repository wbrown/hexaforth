#
# The files the tree's text says by name, each of which is to be there: the
# target `said` (util/said.c).
#
# A file is named by what includes it, by CMakeLists.txt and its parts, in
# comments and in CLAUDE.md, and when its name changes nothing that is built
# finds the places that only say it.  The kinds of file that are looked for are the
# tree's source; build/ is made, and worktrees/ is other trees.
#
# What is said and is not a file of the tree is said here, by what it is:
# another's, which the system has or which is fetched; what is made when
# something is built; a name that stands for whatever file a reader has; a
# file that is gone, where doc/boot.md says what was; and a word of the
# Forth that ends as a file's name does.
#
set(SAID_ANOTHERS ltdl.h unicorn/unicorn.h libcc.fs
                  m68k.h m68kcpu.c m68kops.c m68kops.h m68kmake.c m68k_in.c softfloat/softfloat.c
                  tv80s.v tv80_core.v tv80_alu.v tv80_mcode.v tv80_reg.v)
set(SAID_MADE     test_cases.fs cmake_install.cmake typed.fs asks-gray.fs asks-PIC-SIZE.fs asks-view-PIC-SIZE.fs half.fs
                  phrasetest.fs Vhexaforth.h Vhexaforth___024root.h Vhf_mem.h Vsoc.h)
set(SAID_ANY      name.fs program.fs yourprogram.fs mine.fs)
set(SAID_GONE     vmdisk.fs virtio-blk.fs)
set(SAID_WORDS    cw.h)
tool(said_files util/said.c)
add_custom_target(said
        COMMAND           said_files ${GIT} c h cpp fs S v cmake md
                            --not worktrees
                            --besides ${SAID_ANOTHERS} ${SAID_MADE} ${SAID_ANY} ${SAID_GONE} ${SAID_WORDS}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        VERBATIM)

#
# The figures the tree's comments and documents say: the target `figures`
# (util/said.c).  How big a thing is, how many there are of a thing and
# what a thing costs is what a target prints, and is not written down
# beside it to be kept in step.  This prints each line of a comment or a
# document that has a number and then one of these words, to be read: it
# cannot tell what the machine is from how much there was of something,
# and fails nothing.
#
set(FIGURES_OF % percent bytes KB MB instructions cycles clocks rules words lines blocks files
               tests checks times million states loops cells headers names entries variables
               seconds turns pairs calls characters pixels windows glyphs definitions places)
add_custom_target(figures
        COMMAND           said_files ${GIT} --figures ${FIGURES_OF} --not worktrees
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        VERBATIM)
