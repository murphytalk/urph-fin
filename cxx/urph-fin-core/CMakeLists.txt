set(lib_target urph-fin-core)

include(ExternalProject)

if(USE_FIREBASE)
	# flutter only supports shared FFI lib now
	# it looks when building shared lib -fPIC is always being used, but the prebuilt firebase SDK is not built with -fPIC
	# causing link error.
	# this is to use -fPIC when build firebase
	set (CMAKE_POSITION_INDEPENDENT_CODE ON)


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

    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../3rd-party/yaml-cpp yaml-bin/ EXCLUDE_FROM_ALL)

    add_compile_definitions(USE_FIREBASE)
    set(cloud_src "cloud-src/firestore.cc")

    set(cloud_libs firebase_auth firebase_firestore firebase_app)
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
    list(APPEND extra_libs yaml-cpp)
    set_target_properties(${lib_target} PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

if(USE_AWS)
    # use AWS
	if (NOT "$ENV{AWS_CPP_SDK_DIR}" STREQUAL "")
	  set(AWS_CPP_SDK_DIR "$ENV{AWS_CPP_SDK_DIR}")
	else()
	  set(AWS_CPP_SDK_DIR "/mnt/extra/apps/aws-sdk")
	endif()

    set(SERVICE_COMPONENTS dynamodb)

    # Find the AWS SDK for C++ package.
    if(MSVC)
        string(REPLACE ";" "/aws-cpp-sdk-all;" SYSTEM_MODULE_PATH "${CMAKE_SYSTEM_PREFIX_PATH}/aws-cpp-sdk-all")
        list(APPEND CMAKE_PREFIX_PATH ${SYSTEM_MODULE_PATH})
    else()
        list(APPEND CMAKE_PREFIX_PATH "${AWS_CPP_SDK_DIR}/lib64")
        list(APPEND CMAKE_PREFIX_PATH "${AWS_CPP_SDK_DIR}/lib64/cmake")
    endif()

    find_package(AWSSDK REQUIRED COMPONENTS ${SERVICE_COMPONENTS})

    set(cloud_src "cloud-src/dynamodb.cc")
    add_definitions(-DAWS)
    set(cloud_libs  ${AWSSDK_LINK_LIBRARIES} ${AWSSDK_PLATFORM_DEPS})

    target_include_directories(${lib_target} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        ${AWSSDK_INCLUDE_DIR}/aws
    )
endif()

if(USE_MONGODB)
    ExternalProject_Add(
        mongodb-c-driver
        GIT_REPOSITORY https://github.com/mongodb/mongo-c-driver
        GIT_TAG 1.23.2
        PREFIX ${CMAKE_CURRENT_BINARY_DIR}/libmongoc
        CMAKE_ARGS
            -DCMAKE_BUILD_TYPE=Release
            -DENABLE_STATIC=ON
            -DENABLE_TESTS=OFF
            -DENABLE_EXAMPLES=OFF
            -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/mongo-c-driver-install
    )
    include_directories(${CMAKE_CURRENT_BINARY_DIR}/mongo-c-driver-install/include)
    find_library(
        mongo_libs
        NAMES bson-static-1.0.a mongoc-static-1.0
        PATHS ${CMAKE_CURRENT_BINARY_DIR}/mongo-c-driver-install/lib64
    )
    message("mongodb-c-driver libs ${mongo_libs}")
    list(APPEND extra_libs ${mongo_libs})

    set(cloud_src "cloud-src/mongodb.cc")
endif()

if(USE_YAHOO_FINANCE)
    add_definitions(-DYAHOO_FINANCE)
    add_src_libs_ (mkt-data-src/yahoo-finance mkt_src)
endif()

if(DUMMY_LOG)
    add_definitions(-DDUMMY_LOG)
endif()

add_src_libs_ (src the_SRC)
list(APPEND the_SRC ${cloud_src})
list(APPEND the_SRC ${mkt_src})

add_library (${lib_target} STATIC ${the_SRC})

if(USE_MONGODB)
    add_dependencies(${lib_target} mongodb-c-driver)
endif()

ExternalProject_Add(
    curl-project
    URL https://curl.se/download/curl-7.88.1.tar.gz
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/libcurl
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    CMAKE_ARGS
        -DBUILD_SHARED_LIBS=OFF
        -DCURL_STATICLIB=ON
        -DHTTP_ONLY=ON
        -DENABLE_IPV6=OFF
        -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/libcurl-install
)

include_directories(${CMAKE_CURRENT_BINARY_DIR}/libcurl-install/include)

find_library(
    curl_lib
    NAMES curl
    PATHS ${CMAKE_CURRENT_BINARY_DIR}/libcurl-install/lib64
)
list(APPEND extra_libs ${curl_lib})

set(BUILD_SHARED_LIBS OFF)
set_target_properties(${lib_target} PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_dependencies(${lib_target} curl-project)

# If the debug configuration pass the DEBUG define to the compiler
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DDEBUG_BUILD)
endif()

message("cloud_src is ${cloud_src}")
message("mkt_src is ${mkt_src}")

message("lib_target is ${lib_target}")
message("extra_libs is ${extra_libs}")
message("cloud_libs is ${cloud_libs}")
target_link_libraries(${lib_target} ${extra_libs} ${cloud_libs})


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

add_src_libs_ (test test_SRC)
add_executable(
  test
  ${test_SRC}
)
target_link_libraries(
  test
  gtest_main
  urph-fin-core
)

include(GoogleTest)
gtest_discover_tests(test)