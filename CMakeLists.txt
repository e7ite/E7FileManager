cmake_minimum_required(VERSION 3.10)

project(E7FileManager VERSION 1.0)

find_package(PkgConfig REQUIRED)

pkg_check_modules(GTKMM3 REQUIRED IMPORTED_TARGET gtkmm-3.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_CLANG_TIDY "clang-tidy-11;-checks=clang-analyzer-*;--warnings-as-errors=clang-analyzer-*")

file(GLOB_RECURSE ALL_CXX_SOURCE_FILES(${PROJECT_SOURCE_DIR}/src/*.[ch]pp))
add_executable(e7fmgr ${ALL_CXX_SOURCE_FILES})
target_link_libraries(e7fmgr PUBLIC PkgConfig::GTKMM3)

# Can't organize by pairing with other CXX settings due to this needing
# declaration of target, and CMAKE_CXX_CLANG_TIDY requiring to be declared before target.
find_program(CLANG_FORMAT "clang-format(-[0-9]{1,2})?")
if(CLANG_FORMAT)
    add_custom_command(TARGET e7fmgr
        PRE_BUILD
        COMMAND ${CLANG_FORMAT} -i -style=Google ${ALL_CXX_SOURCE_FILES}
        DEPENDS e7fmgr
    )
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

message("Done! Binary \"e7mgr\" is stored in build/")
