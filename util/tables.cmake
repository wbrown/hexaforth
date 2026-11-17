# forth/fused.fs, the words the decompiler shows an instruction as, made
# again (the target `tables` runs this, from where the source is, and tells
# it where gforth is, GFORTH, and the tool that makes the table, FUSED).
#
# The table is in the image it is made from.  So the image is built with
# the table there is, the table made of that image, and the image built
# again, until the table stops changing.  The image is the default
# machine's (build/nuc.hex), and what cross.fs says is build/cross.log.
foreach(TURN RANGE 1 6)
    execute_process(
            COMMAND           ${GFORTH} ./cross.fs ./basewords.fs ./nuc.fs
            WORKING_DIRECTORY forth
            OUTPUT_FILE       ../build/cross.log
            ERROR_FILE        ../build/cross.log
            RESULT_VARIABLE   FAILED)
    if(FAILED)
        message(FATAL_ERROR "the image was not built: see build/cross.log")
    endif()
    file(READ forth/fused.fs BEFORE)
    execute_process(
            COMMAND           ${FUSED} build/nuc.hex build/nuc.tables forth/fused.fs
            RESULT_VARIABLE   FAILED)
    if(FAILED)
        message(FATAL_ERROR "the table was not made")
    endif()
    file(READ forth/fused.fs AFTER)
    if(BEFORE STREQUAL AFTER)
        message("forth/fused.fs settled after ${TURN}")
        return()
    endif()
endforeach()
message(FATAL_ERROR "forth/fused.fs was still changing after ${TURN} images")
