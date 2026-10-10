# apps/omamovie — the Qt Quick editor shell. Qt lives only here (CLAUDE.md §5.2).
# QML is kept as plain files: a development build reads the source tree and an installed build
# reads share/omamovie/qml, so main.cpp's qml_dir() fallback and the smoke run are unchanged.
# GuiPrivate carries the versioned private headers (rhi/qrhi.h) the preview item imports, the same
# coordinate the old Makefile added by hand.
find_package(Qt6 REQUIRED COMPONENTS Quick Test DBus GuiPrivate)

file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_executable(omamovie ${APP_SOURCES})
set_target_properties(omamovie PROPERTIES
    AUTOMOC ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
target_include_directories(omamovie PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(omamovie PRIVATE ${OMA_TEST_WARNINGS})
target_link_libraries(omamovie PRIVATE
    oma_project oma_playback oma_timeline oma_audio oma_compositor oma_media oma_gpu oma_base
    Qt6::Quick Qt6::Test Qt6::DBus Qt6::GuiPrivate)

# Headless Session tests: the app sources without main.cpp, Qt's offscreen platform.
set(APP_LIB_SOURCES ${APP_SOURCES})
list(FILTER APP_LIB_SOURCES EXCLUDE REGEX "/main\\.cpp$")
file(GLOB APP_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/tests/app/*.cpp)

add_executable(app_tests ${APP_LIB_SOURCES} ${APP_TEST_SOURCES})
set_target_properties(app_tests PROPERTIES
    AUTOMOC ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)
target_include_directories(app_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests/support ${CMAKE_SOURCE_DIR}/third_party/cest)
target_compile_options(app_tests PRIVATE ${OMA_TEST_WARNINGS})
target_link_libraries(app_tests PRIVATE
    oma_project oma_playback oma_timeline oma_audio oma_compositor oma_media oma_gpu oma_base
    Qt6::Quick Qt6::Test Qt6::DBus Qt6::GuiPrivate)
add_dependencies(app_tests fixtures)

add_test(NAME app COMMAND sh ${CMAKE_SOURCE_DIR}/tests/support/run_test.sh $<TARGET_FILE:app_tests>)
set_tests_properties(app PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
if(OMA_TEST_ENV)
    set_tests_properties(app PROPERTIES ENVIRONMENT "${OMA_TEST_ENV}")
endif()

# The installed tree: bin/omamovie finds its QML in ../share/omamovie/qml.
install(TARGETS omamovie RUNTIME DESTINATION bin)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/qml/
    DESTINATION share/omamovie/qml FILES_MATCHING PATTERN "*.qml")
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/data/omamovie.desktop DESTINATION share/applications)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/data/omamovie.svg
    DESTINATION share/icons/hicolor/scalable/apps)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/data/omamovie-mime.xml
    DESTINATION share/mime/packages RENAME omamovie.xml)
install(FILES ${CMAKE_SOURCE_DIR}/LICENSE DESTINATION share/licenses/${OMA_LICENSE_NAME})
