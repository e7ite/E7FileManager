cmake_minimum_required(VERSION 3.10)

project(E7FileManager VERSION 1.0)

find_package(PkgConfig REQUIRED)

pkg_check_modules(GTKMM3 REQUIRED IMPORTED_TARGET gtkmm-3.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

file(GLOB_RECURSE ALL_CXX_SOURCE_FILES(${PROJECT_SOURCE_DIR}/src/*.[ch]pp))
add_executable(e7fmgr ${ALL_CXX_SOURCE_FILES})
target_link_libraries(e7fmgr PUBLIC PkgConfig::GTKMM3 absl::strings)

file(GLOB CLANG_FORMAT NAME "/usr/bin/clang-format-[0-9]*")
if(CLANG_FORMAT)
    message("${CLANG_FORMAT} found!")
    add_custom_command(TARGET e7fmgr
        PRE_BUILD
        COMMAND ${CLANG_FORMAT} -i -style=Google ${ALL_CXX_SOURCE_FILES}
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
        COMMAND ${CLANG_TIDY} -checks=clang-analyzer-* --warnings-as-errors=clang-analyzer-* -p ${PROJECT_SOURCE_DIR}/build ${ALL_CXX_SOURCE_FILES}
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

include(GoogleTest) 

set(ABSL_PROPAGATE_CXX_STD ON)
add_subdirectory(abseil-cpp)
