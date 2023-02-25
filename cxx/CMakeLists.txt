cmake_minimum_required (VERSION 3.12)

cmake_policy(SET CMP0048 NEW)

project(urph-fin VERSION 1.0)
set (CMAKE_CXX_STANDARD 17)

#enable the following to turn on verbose compile 
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

set(USE_MONGODB ON)
set(USE_YAHOO_FINANCE ON)

add_subdirectory (urph-fin-core)
add_subdirectory (cli)
add_subdirectory (urph-fin-core-dart)
