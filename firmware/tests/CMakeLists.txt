# Off-target unit tests: pure logic from core/ compiled for the desktop.
# Nothing in core/ may include a Pico SDK header, and this build proves it.

file(GLOB CORE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../core/*.cpp)
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

if(TEST_SOURCES)
    add_executable(core_tests ${TEST_SOURCES} ${CORE_SOURCES})
    target_include_directories(core_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../core)
    add_test(NAME core_tests COMMAND core_tests)
else()
    message(STATUS "No test sources in firmware/tests yet, skipping core_tests target")
endif()
