set(lib_target urph-fin-core)

# flutter only supports shared FFI lib now
# it looks when building shared lib -fPIC is always being used, but the prebuilt firebase SDK is not built with -fPIC
# causing link error.
# this is to use -fPIC when build firebase
set (CMAKE_POSITION_INDEPENDENT_CODE ON)

set(FIREBASE_CPP_SDK_DIR "$ENV{HOME}/work/firebase-cpp-sdk")
set(BUILD_TESTING OFF)
set(FIREBASE_CPP_BUILD_TESTS OFF)
set(FIREBASE_LINUX_USE_CXX11_ABI ON)
add_subdirectory(${FIREBASE_CPP_SDK_DIR} firebase-bin/ EXCLUDE_FROM_ALL)

add_src_libs_ (src the_SRC)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../3rd-party/yaml-cpp yaml-bin/ EXCLUDE_FROM_ALL)


add_library (${lib_target} ${the_SRC})

add_log_header(${lib_target})

set(firebase_libs firebase_auth firebase_firestore firebase_app)

if(APPLE)
    set(ADDITIONAL_LIBS
      gssapi_krb5
      pthread
      "-framework CoreFoundation"
      "-framework Foundation"
      "-framework GSS"
      "-framework Security"
      "-framework SystemConfiguration"
)
elseif(MSVC)
    set(ADDITIONAL_LIBS advapi32 ws2_32 crypt32 iphlpapi psapi userenv)
#else()
#    set(ADDITIONAL_LIBS pthread)
endif()

set_target_properties(${lib_target} PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(${lib_target} yaml-cpp ${extra_libs} ${firebase_libs})


# populate include to project that uses this target
target_include_directories (${lib_target} 
  PUBLIC src
)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
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