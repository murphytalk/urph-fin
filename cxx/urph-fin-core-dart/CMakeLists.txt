set(lib_target urph-fin-core-dart)

set(DART_SDK "$ENV{FLUTTER_HOME}/bin/cache/dart-sdk")

set(SRC
    ${DART_SDK}/include/dart_api_dl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/dart.cc
)

add_library(${lib_target} SHARED ${SRC})
set_target_properties(${lib_target} PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(${lib_target} 
    PRIVATE ${DART_SDK}/include
)
target_link_libraries(${lib_target} urph-fin-core)
