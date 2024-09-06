file(GLOB_RECURSE TESTS_SOURCES "*.cpp")
foreach (TEST_SOURCE ${TESTS_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
endforeach ()

target_link_libraries(test_print PRIVATE print)
target_link_libraries(test_demangle PRIVATE demangle)