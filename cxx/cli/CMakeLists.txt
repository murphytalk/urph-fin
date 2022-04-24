find_package(Threads REQUIRED)

add_src_libs_()

set(target urph-fin-cli)

add_executable(${target} ${the_SRC})

target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../3rd-party/cli/include)
target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../3rd-party/tabulate/include)

target_link_libraries(${target} urph-fin-core ${CMAKE_THREAD_LIBS_INIT})
