#
# Every test there is, run: a line for each, and all that each said kept
# (Testing/Temporary/LastTest.log, where CMake builds).  `ctest -V -R NAME`
# there runs the ones whose names have that in them, and prints what they
# say.  Either way a test's needs are built before it is run (needs,
# cmake/kinds.cmake): a line for each of those too, built-TARGET, and a
# test whose needs do not build is not run.
#
# The tests run in parallel, CHECK_JOBS at a time: 4 unless CMake is told
# otherwise (cmake -DCHECK_JOBS=8 build/cmake), so that the machine is left
# room for other work while they run.  That is safe because no test writes
# a file another test uses: each writes to its own directory (QEMU's tests
# to a fresh temporary one), what the Verilog and the kernels are compared
# with is made when they are built, and one thing is built at a time.  A
# new test that writes files needs a directory of its own too.
#
set(CHECK_JOBS 4 CACHE STRING "how many tests the target check runs at once")
get_property(EVERY_TEST DIRECTORY PROPERTY TESTS)
foreach(TEST ${EVERY_TEST})                             # each that has not been given longer
    get_test_property(${TEST} TIMEOUT GIVEN)
    if(NOT GIVEN)
        set_tests_properties(${TEST} PROPERTIES TIMEOUT ${TEST_SECONDS})
    endif()
    # A test that does not say what it needs would be run on whatever
    # happened to be built: there is none.
    get_test_property(${TEST} FIXTURES_REQUIRED NEEDED)
    get_test_property(${TEST} FIXTURES_SETUP BUILDS)
    if(NOT NEEDED AND NOT BUILDS)
        message(FATAL_ERROR "the test ${TEST} does not say what it needs built (needs)")
    endif()
endforeach()
# Before its tests, each of the two says the names the system's source
# defines again, on every machine (util/again.c, of what cross.fs wrote
# beside each image), and what each machine's memory has used and has left
# (util/map.c, --table: memory-table-NAME): so each has those made first,
# as a report has what it names.  Neither is an error.  They are said where
# whoever is at work reads them, and before the tests, so that they are
# there when a test fails.
set(AGAIN_SAID)
foreach(NAME ${MACHINES})
    list(APPEND AGAIN_SAID ${NAME} ${BUILD}/${NAME}/nuc.shadows)
endforeach()
get_property(MEMORY_TABLES GLOBAL PROPERTY MEMORY_TABLES)
function(tests_run TARGET)                              # and which tests, as ctest is told: every one, if it is not
    report(${TARGET}
            COMMAND           again ${AGAIN_SAID}
            COMMAND           ${CAT} ${MEMORY_TABLES}
            COMMAND           ${CMAKE_CTEST_COMMAND} --output-on-failure -j ${CHECK_JOBS} ${ARGN}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            USES_TERMINAL)
endfunction()
tests_run(check)

# quick is the tests that need only the reference VM (ON_THE_VM): every
# machine's prompt, boot, desktop and drawing, and the two builds compared.
# It is what is run while the source is being changed, and it is what
# building with no target named does: it is built after everything else
# such a build makes, since its tests build what they need themselves, one
# target at a time.
tests_run(quick -L ${ON_THE_VM})
set_target_properties(quick PROPERTIES EXCLUDE_FROM_ALL FALSE)
get_property(EVERY_TARGET DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(EACH ${EVERY_TARGET})
    get_target_property(LEFT_OUT ${EACH} EXCLUDE_FROM_ALL)
    if(NOT LEFT_OUT AND NOT "${EACH}" STREQUAL "quick")
        add_dependencies(quick ${EACH})
    endif()
endforeach()
