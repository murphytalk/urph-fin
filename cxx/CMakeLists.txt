cmake_minimum_required (VERSION 3.22.0)
project(urph-fin)
set (CMAKE_CXX_STANDARD 17)

include(CMake/internal_utils.cmake)

add_subdirectory (urph-fin-core)
add_subdirectory (cli)
