cmake_minimum_required (VERSION 3.22.0)
project(urph-fin)
set (CMAKE_CXX_STANDARD 17)
#set(CMAKE_VERBOSE_MAKEFILE ON CACHE BOOL "ON" FORCE)
set(project_root ${CMAKE_CURRENT_SOURCE_DIR})

#set(CMAKE_DEBUG_TARGET_PROPERTIES
#  INCLUDE_DIRECTORIES
#  COMPILE_DEFINITIONS
#  POSITION_INDEPENDENT_CODE
#  CONTAINER_SIZE_REQUIRED
#  LIB_VERSION
#)

include(CMake/internal_utils.cmake)


add_subdirectory (urph-fin-core)
add_subdirectory (cli)
add_subdirectory (urph-fin-core-dart)
