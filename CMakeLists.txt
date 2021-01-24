# Minimum required version
cmake_minimum_required(VERSION 3.10)

# Setting the project name. This must be placed up here or some Cmake variables go undefined
project(E7FileManager VERSION 1.0)

# Checks if pkg-config is installed on the system and errors out if not found
find_package(PkgConfig REQUIRED)

# Create PkgConfig::gtkmm-3.0 variables to link with target_link_libraries
pkg_check_modules(GTKMM3 REQUIRED IMPORTED_TARGET gtkmm-3.0)

# Set C++ Standard to C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Add the executable to build (1st arg) and the folder for its source (2nd arg)
add_executable(e7fmgr ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)

# Link target_link_libraries
target_link_libraries(e7fmgr PUBLIC PkgConfig::GTKMM3)

# Let the user know where the binary is located
message("Done! Binary \"e7mgr\" is stored in build/")
