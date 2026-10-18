# oma_test(<name> SOURCES <srcs...> LIBS <project libs...>
#          [EXTERNAL <items...>] [WORKING_DIRECTORY <dir>])
function(oma_test name)
    cmake_parse_arguments(ARG "" "WORKING_DIRECTORY" "SOURCES;LIBS;EXTERNAL" ${ARGN})
    add_executable(${name} ${ARG_SOURCES})
    set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)
    target_include_directories(${name} PRIVATE
        ${CMAKE_SOURCE_DIR}/tests/support ${CMAKE_SOURCE_DIR}/third_party/cest)
    target_compile_options(${name} PRIVATE ${OMA_TEST_WARNINGS})
    foreach(lib ${ARG_LIBS})
        target_link_libraries(${name} PRIVATE oma_${lib})
    endforeach()
    target_link_libraries(${name} PRIVATE ${ARG_EXTERNAL})
    oma_add_test(${name} ${ARG_WORKING_DIRECTORY})
endfunction()

# Register an executable (or an arbitrary command) with CTest. run_test.sh appends the Cest name
# filter from `make test FILTER=...`; the sanitizer environment is applied per test.
function(oma_add_test name)
    add_test(NAME ${name}
        COMMAND sh ${CMAKE_SOURCE_DIR}/tests/support/run_test.sh $<TARGET_FILE:${name}>)
    set(wd ${CMAKE_SOURCE_DIR})
    if(ARGN)
        set(wd ${CMAKE_SOURCE_DIR}/${ARGN})
    endif()
    set_tests_properties(${name} PROPERTIES WORKING_DIRECTORY ${wd})
    if(OMA_TEST_ENV)
        set_tests_properties(${name} PROPERTIES ENVIRONMENT "${OMA_TEST_ENV}")
    endif()
endfunction()
