# tools/oma-project's line diff, checked against a dynamic-programming LCS on random inputs.
oma_test(oma_project_tests
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp ${CMAKE_CURRENT_SOURCE_DIR}/test_line_diff.cpp
            ${CMAKE_SOURCE_DIR}/tools/oma-project/line_diff.cpp
            ${CMAKE_SOURCE_DIR}/tools/oma-project/line_diff_back.cpp
    LIBS project timeline base)
target_include_directories(oma_project_tests PRIVATE ${CMAKE_SOURCE_DIR}/tools/oma-project)
