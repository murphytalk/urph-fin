cmake_minimum_required (VERSION 3.22.0)
project(urph-fin)
set (CMAKE_CXX_STANDARD 17)

set(project_root ${CMAKE_CURRENT_SOURCE_DIR})

if(MSVC)
	set(ENV{TRACEDESIGNTIME}, true)
	set(MSVC_RUNTIME_MODE "MT")
endif()

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
