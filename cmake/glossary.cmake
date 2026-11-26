#
# The glossary: doc/words.md lists every word that has a header, with the
# stack comment and the description from the Forth source
# (util/glossary.c).  It is generated from the first machine's nucleus and
# checked in.
#
#   words            the target that writes doc/words.md again
#   words-doc        the test that doc/words.md is what the tool writes now
#   agents-words     the test that the stack comments in AGENTS.md's tables
#                    are the ones in the source
#
tool(glossary util/glossary.c)
set(PUBLIC ${BUILD}/${FIRST}/nuc.public)
set(WORDS_NOW ${BUILD}/${FIRST}/words.md)
add_custom_command(
        OUTPUT            ${WORDS_NOW}
        COMMAND           glossary ${PUBLIC} forth nuc.fs > ${WORDS_NOW}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        DEPENDS           glossary machine-${FIRST} ${FORTH_SOURCES})
add_custom_target(words-now DEPENDS ${WORDS_NOW})
add_custom_target(words
        COMMAND           ${COPY} ${WORDS_NOW} doc/words.md
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        DEPENDS           ${WORDS_NOW})

add_test(NAME words-doc COMMAND ${CMAKE_COMMAND} -E compare_files ${WORDS_NOW} doc/words.md
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
needs(words-doc words-now)
add_test(NAME agents-words COMMAND glossary ${PUBLIC} forth nuc.fs --check AGENTS.md
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
needs(agents-words glossary machine-${FIRST})
on_the_vm(words-doc agents-words)
