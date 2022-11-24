# reference: https://github.com/firebase/quickstart-cpp/blob/main/firestore/testapp/CMakeLists.txt
cmake_minimum_required (VERSION 3.18)
project(urph-fin-core)
set (CMAKE_CXX_STANDARD 17)
set(lib_target urph-fin-core)

set(FIREBASE_CPP_SDK_DIR "$ENV{HOME}/work/firebase_cpp_sdk")

add_subdirectory(${FIREBASE_CPP_SDK_DIR} firebase-bin/ EXCLUDE_FROM_ALL)


include(../../../cxx/CMake/internal_utils.cmake)
add_src_libs_ (../../../cxx/urph-fin-core/src the_SRC)

add_subdirectory(../../../cxx/3rd-party/yaml-cpp yaml-bin/ EXCLUDE_FROM_ALL)

add_library (${lib_target} SHARED android.cc ${the_SRC})

add_library(native_app_glue STATIC
    ${MY_ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)


target_link_libraries(${lib_target} yaml-cpp ${extra_libs} android native_app_glue)
target_include_directories(${lib_target} PRIVATE ${MY_ANDROID_NDK}/sources/android/native_app_glue)

set(firebase_libs firebase_auth firebase_firestore firebase_app)
target_link_libraries(${lib_target} ${firebase_libs})