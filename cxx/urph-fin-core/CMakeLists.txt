set(lib_target urph-fin-core)

include(FetchContent)

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
else()
    if(NOT ANDROID)
        message("use aixlog")
        FetchContent_Declare(
            aixlog
            GIT_REPOSITORY https://github.com/badaix/aixlog.git
            GIT_TAG v1.5.0
        )
        FetchContent_MakeAvailable(aixlog)
        include_directories(${aixlog_SOURCE_DIR}/include)
        add_definitions(-DAIX_LOG)
    endif()
endif()

if(USE_MONGODB)
    set(DB_NAME "atlas")

    # read conn str and generate the code
    set(finance_yaml_file "$ENV{HOME}/.finance-credentials/finance-scraper.yaml")
    if(NOT EXISTS ${finance_yaml_file})
        message(FATAL_ERROR "${finance_yaml_file}does not exist")
    endif()
    file(READ ${finance_yaml_file} FILE_CONTENTS)
    # replace newline with ; so it is treated by CMake as list
    string(REPLACE "\n" ";" TEXT_LINES "${FILE_CONTENTS}")
    foreach(_line ${TEXT_LINES})
        string(FIND ${_line} ${DB_NAME} IDX)
        if(${IDX} GREATER -1)
            string(REGEX REPLACE "${DB_NAME}: *(.*)" "\\1" TARGET_DATA "${_line}")
            string(STRIP "${TARGET_DATA}" TARGET_DATA)
            message("Found ${DB_NAME} conn str: ${TARGET_DATA}")

            set(conn_str_src ${CMAKE_CURRENT_LIST_DIR}/generated-code/mongodb.cc)
            message("Generated code ${conn_str_src}")
            file(WRITE ${conn_str_src} "const char mongodb_conn_str [] = \"${TARGET_DATA}\";\n")
            break()
        endif()
    endforeach()
   


    option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
    set(ENABLE_STATIC ON)
    set(ENABLE_TESTS OFF)
    set(ENABLE_EXAMPLES OFF)
    set(MONGOC_ENABLE_MONGODB_AWS_AUTH OFF)
    set(ENABLE_SSL OPENSSL)
    set(ENABLE_ZLIB BUNDLED)
    set(ENABLE_ZSTD OFF)
    set(MONGOCXX_ENABLE_SSL ON)

    FetchContent_Declare(    
        mongodb-c-driver
        GIT_REPOSITORY https://github.com/mongodb/mongo-c-driver
        GIT_TAG 1.23.2
    )
    FetchContent_MakeAvailable(mongodb-c-driver)

    FetchContent_Declare(    
        mongodb-cxx-driver
        GIT_REPOSITORY https://github.com/mongodb/mongo-cxx-driver
        GIT_TAG r3.7.0
    )
    FetchContent_MakeAvailable(mongodb-cxx-driver)

    add_definitions(-DUSE_MONGODB)
    set(cloud_src "cloud-src/mongodb.cc")
    set(cloud_libs mongocxx_static bsoncxx_static)
endif()

if(USE_YAHOO_FINANCE)
    add_definitions(-DYAHOO_FINANCE)

    add_src_libs_ (mkt-data-src/yahoo-finance mkt_src)

    set(BUILD_SHARED_LIBS OFF)
    set(CURL_STATICLIB ON)
    set(HTTP_ONLY ON)
    set(ENABLE_IPV6 OFF)
    FetchContent_Declare(
        curl-project
        URL https://curl.se/download/curl-7.88.1.tar.gz
    )
    FetchContent_MakeAvailable(curl-project)
    
    list(APPEND extra_libs libcurl)
endif()

if(DUMMY_LOG)
    add_definitions(-DDUMMY_LOG)
endif()

add_src_libs_ (src the_SRC)
list(APPEND the_SRC ${cloud_src})
list(APPEND the_SRC ${mkt_src})

add_library (${lib_target} STATIC ${the_SRC})

set(BUILD_SHARED_LIBS OFF)
set_target_properties(${lib_target} PROPERTIES POSITION_INDEPENDENT_CODE ON)

# If the debug configuration pass the DEBUG define to the compiler
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DDEBUG_BUILD)
endif()

message("cloud_src is ${cloud_src}")
message("mkt_src is ${mkt_src}")
message("cloud_libs is ${cloud_libs}")
message("lib_target is ${lib_target}")


target_link_libraries(${lib_target} PRIVATE ${extra_libs} ${cloud_libs})

# populate include to project that uses this target
target_include_directories (${lib_target}
  PUBLIC src
)

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
  ${lib_target}
)

include(GoogleTest)
gtest_discover_tests(test)