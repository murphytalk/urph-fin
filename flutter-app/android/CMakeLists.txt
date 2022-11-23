cmake_minimum_required (VERSION 3.18)
project(urph-fin-core)
set (CMAKE_CXX_STANDARD 17)
set(lib_target urph-fin-core)

set(FIREBASE_CPP_SDK_DIR "$ENV{HOME}/work/firebase_cpp_sdk")
set(BUILD_TESTING OFF)
set(FIREBASE_CPP_BUILD_TESTS OFF)
#set(FIREBASE_LINUX_USE_CXX11_ABI ON)
add_subdirectory(${FIREBASE_CPP_SDK_DIR} firebase-bin/ EXCLUDE_FROM_ALL)


include(${CMAKE_CURRENT_SOURCE_DIR}/../../cxx/CMake/internal_utils.cmake)
add_src_libs_ (${CMAKE_CURRENT_SOURCE_DIR}/../../cxx/urph-fin-core/src the_SRC)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../cxx/3rd-party/yaml-cpp yaml-bin/ EXCLUDE_FROM_ALL)


add_library (${lib_target} SHARED ${the_SRC})

add_log_header(${lib_target})

set(firebase_libs firebase_auth firebase_firestore firebase_app)


target_link_libraries(${lib_target} ${firebase_libs} yaml-cpp ${extra_libs})
