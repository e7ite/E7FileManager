cmake_minimum_required(VERSION 3.10)

project(E7FileManager VERSION 1.0)

find_package(PkgConfig REQUIRED)

pkg_check_modules(GTKMM3 REQUIRED IMPORTED_TARGET gtkmm-3.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(e7fmgr ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)

target_link_libraries(e7fmgr PUBLIC PkgConfig::GTKMM3)

message("Done! Binary \"e7mgr\" is stored in build/")
