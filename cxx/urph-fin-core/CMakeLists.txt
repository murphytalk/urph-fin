set(lib_target urph-fin-core)

add_src_libs_ ()

set(FIREBASE_CPP_SDK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../3rd-party/firebase_cpp_sdk)
add_subdirectory(${FIREBASE_CPP_SDK_DIR} firebase-bin/ EXCLUDE_FROM_ALL)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../3rd-party/yaml-cpp yaml-bin/ EXCLUDE_FROM_ALL)


add_library (${lib_target} ${the_SRC})

add_log_header(${lib_target})

set(firebase_libs firebase_auth firebase_firestore firebase_app)

if(WIN32)
  set(extra_libs wsock32 ws2_32 crypt32)
endif()

target_link_libraries(${lib_target} ${firebase_libs} yaml-cpp ${extra_libs})

# populate include to project that uses this target
target_include_directories (${lib_target} PUBLIC src)
