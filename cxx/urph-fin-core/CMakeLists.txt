set(lib_target urph-fin-core)

# flutter only supports shared FFI lib now
# it looks when building shared lib -fPIC is always being used, but the prebuilt firebase SDK is not built with -fPIC
# causing link error.
# this is to use -fPIC when build firebase
set (CMAKE_POSITION_INDEPENDENT_CODE ON)

set(FIREBASE_CPP_SDK_DIR "D:/work/firebase_cpp_sdk")

if (NOT "$ENV{FIREBASE_CPP_SDK_DIR}" STREQUAL "")
  set(DEFAULT_FIREBASE_CPP_SDK_DIR "$ENV{FIREBASE_CPP_SDK_DIR}")
else()
  set(DEFAULT_FIREBASE_CPP_SDK_DIR "firebase_cpp_sdk")
endif()
if ("${FIREBASE_CPP_SDK_DIR}" STREQUAL "")
  set(FIREBASE_CPP_SDK_DIR ${DEFAULT_FIREBASE_CPP_SDK_DIR})
endif()
if(NOT EXISTS ${FIREBASE_CPP_SDK_DIR})
  message(FATAL_ERROR "The Firebase C++ SDK directory does not exist: ${FIREBASE_CPP_SDK_DIR}. See the readme.md for more information")
endif()

set(BUILD_TESTING OFF)
set(FIREBASE_CPP_BUILD_TESTS OFF)
set(FIREBASE_LINUX_USE_CXX11_ABI ON)
add_subdirectory(${FIREBASE_CPP_SDK_DIR} firebase-bin/ EXCLUDE_FROM_ALL)

add_src_libs_ (src the_SRC)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../3rd-party/yaml-cpp yaml-bin/ EXCLUDE_FROM_ALL)


add_library (${lib_target} ${the_SRC})

add_log_header(${lib_target})

set(firebase_libs firebase_auth firebase_firestore firebase_app)

# Windows runtime mode, either MD or MT depending on whether you are using
# /MD or /MT. For more information see:
# https://msdn.microsoft.com/en-us/library/2kzt1wy3.aspx
set(MSVC_RUNTIME_MODE MD)

if(APPLE)
    set(extra_libs
      gssapi_krb5
      pthread
      "-framework CoreFoundation"
      "-framework Foundation"
      "-framework GSS"
      "-framework Security"
      "-framework SystemConfiguration"
)
elseif(MSVC)
    set(extra_libs advapi32 ws2_32 crypt32 iphlpapi psapi userenv)
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