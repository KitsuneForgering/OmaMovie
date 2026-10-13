# libs/project tests: save/load round trips, refused and broken files, atomic replacement.
# Also the timeline's edit-property generator, so every generated state is saved and reloaded.
file(GLOB SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB PROPERTY_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/tests/timeline/property_*.cpp)
oma_test(project_tests SOURCES ${SOURCES} ${PROPERTY_SOURCES} LIBS project timeline project-ir base)
target_include_directories(project_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests/timeline)

# The oma-project CLI, exercised end to end.
add_test(NAME cli COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/test_cli.sh $<TARGET_FILE:oma-project>)
set_tests_properties(cli PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
if(OMA_TEST_ENV)
    set_tests_properties(cli PROPERTIES ENVIRONMENT "${OMA_TEST_ENV}")
endif()
