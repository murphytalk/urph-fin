set(lib_target urph-fin-core)

add_src_libs_ ()

set(FIREBASE_CPP_SDK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../3rd-party/firebase_cpp_sdk)
add_subdirectory(${FIREBASE_CPP_SDK_DIR} firebase-bin/ EXCLUDE_FROM_ALL)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../3rd-party/yaml-cpp yaml-bin/ EXCLUDE_FROM_ALL)


add_library (${lib_target} ${the_SRC})

add_log_header(${lib_target})
#target_include_directories(${lib_target} PRIVATE ${project_root}/3rd-party/rapidyaml/src)

set(firebase_libs firebase_auth firebase_app)
target_link_libraries(${lib_target} ${firebase_libs} yaml-cpp)

# populate include to project that uses this target
target_include_directories (${lib_target} PUBLIC src)
