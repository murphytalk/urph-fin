set(lib_target urph-fin-core-dart)

set(DART_SDK "/mnt/extra2/apps/flutter/bin/cache/dart-sdk")

set(SRC
    ${DART_SDK}/include/dart_api_dl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/dart.cc
)
set(target urph-fin-cli-dart)

add_library(${lib_target} ${SRC})

target_include_directories(${lib_target} 
    PRIVATE ${DART_SDK}/include
)

target_link_libraries(${lib_target} urph-fin-core)
