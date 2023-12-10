cmake_minimum_required (VERSION 3.12)

cmake_policy(SET CMP0048 NEW)

project(urph-fin VERSION 1.0)

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++latest /Zc:__cplusplus")
    add_compile_definitions(NOMINMAX)
else(MSVC)
    set (CMAKE_CXX_STANDARD 17)
endif(MSVC)

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

execute_process(COMMAND uname -a OUTPUT_VARIABLE OS_TYPE OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT OS_TYPE MATCHES "microsoft")
    add_subdirectory (urph-fin-core-dart)
endif()
