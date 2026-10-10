# libs/project tests: save/load round trips, refused and broken files, atomic replacement.
file(GLOB SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
oma_test(project_tests SOURCES ${SOURCES} LIBS project timeline base)

# The oma-project CLI, exercised end to end.
add_test(NAME cli COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/test_cli.sh $<TARGET_FILE:oma-project>)
set_tests_properties(cli PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
if(OMA_TEST_ENV)
    set_tests_properties(cli PROPERTIES ENVIRONMENT "${OMA_TEST_ENV}")
endif()
