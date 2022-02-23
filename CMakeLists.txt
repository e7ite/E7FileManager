cmake_minimum_required(VERSION 3.10)

project(E7FileManager VERSION 1.0)

find_package(PkgConfig REQUIRED)

pkg_check_modules(GTKMM3 REQUIRED IMPORTED_TARGET gtkmm-3.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_COMPILE_FEATURES ${CMAKE_CXX_COMPILE_FEATURES} -g)
set(CMAKE_CXX_STANDARD_REQUIRED True)

file(GLOB_RECURSE ALL_CXX_FILES(${PROJECT_SOURCE_DIR}/src/*.[ch]pp))
file(GLOB_RECURSE ALL_CXX_TEST_FILES(${PROJECT_SOURCE_DIR}/src/*_test.[ch]pp))

# Get the set difference to not compile tests as regular source files
list(APPEND ALL_CXX_SOURCE_FILES ${ALL_CXX_FILES})
list(REMOVE_ITEM ALL_CXX_SOURCE_FILES ${ALL_CXX_TEST_FILES})

add_executable(e7fmgr ${ALL_CXX_SOURCE_FILES})
target_link_libraries(e7fmgr PUBLIC PkgConfig::GTKMM3 absl::status)

file(GLOB CLANG_FORMAT NAME "/usr/bin/clang-format-[0-9]*")
if(CLANG_FORMAT)
    message("${CLANG_FORMAT} found!")
    add_custom_command(TARGET e7fmgr
        PRE_BUILD
        COMMAND ${CLANG_FORMAT} -i -style=Google ${ALL_CXX_FILES}
        DEPENDS e7fmgr
    )
else()
    message("Could not find clang-format. Will not be automatically formatting code.")
endif()

file(GLOB CLANG_TIDY NAME "/usr/bin/clang-tidy-[0-9]*")
if(CLANG_TIDY)
    message("${CLANG_TIDY} found!")
    add_custom_command(TARGET e7fmgr
        PRE_BUILD
        COMMAND ${CLANG_TIDY} -checks=clang-analyzer-* --warnings-as-errors=clang-analyzer-* -p ${PROJECT_SOURCE_DIR}/build ${ALL_CXX_FILES}
        DEPENDS e7fmgr
    )
else()
    message("Could not find clang-tidy. Will not be linting code for common errors.")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/e2239ee6043f73722e7aa812a459f54a28552929.zip
)
FetchContent_MakeAvailable(googletest)
enable_testing()
add_compile_options(-g)
add_executable(gui_test ${PROJECT_SOURCE_DIR}/src/gui.hpp ${PROJECT_SOURCE_DIR}/src/gui.cpp ${PROJECT_SOURCE_DIR}/src/gui_test.cpp)
target_link_libraries(gui_test PUBLIC gtest_main PkgConfig::GTKMM3 gmock_main absl::status)

include(GoogleTest)
gtest_discover_tests(gui_test)

set(ABSL_PROPAGATE_CXX_STD ON)
add_subdirectory(abseil-cpp)
