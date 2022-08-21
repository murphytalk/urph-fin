set(lib_target urph-fin-core)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")

set(FIREBASE_CPP_SDK_DIR "/home/mu/work/firebase-cpp-sdk")
set(BUILD_TESTING OFF)
set(FIREBASE_CPP_BUILD_TESTS OFF)
set(FIREBASE_LINUX_USE_CXX11_ABI ON)
add_subdirectory(${FIREBASE_CPP_SDK_DIR} firebase-bin/ EXCLUDE_FROM_ALL)


add_src_libs_ (src the_SRC)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../3rd-party/yaml-cpp yaml-bin/ EXCLUDE_FROM_ALL)


add_library (${lib_target} ${the_SRC})

add_log_header(${lib_target})

set(firebase_libs firebase_auth firebase_firestore firebase_app)

if(WIN32)
  set(extra_libs wsock32 ws2_32 crypt32)
endif()

target_link_libraries(${lib_target} ${firebase_libs} yaml-cpp ${extra_libs})

# populate include to project that uses this target
target_include_directories (${lib_target} 
  PUBLIC src
)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_src_libs_ (test)
add_executable(
  test
  ${the_SRC} 
)
target_link_libraries(
  test
  gtest_main
  urph-fin-core
)

include(GoogleTest)
gtest_discover_tests(test)