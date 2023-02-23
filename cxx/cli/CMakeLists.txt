find_package(Threads REQUIRED)
find_package(Boost COMPONENTS date_time REQUIRED)
INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS})	

add_src_libs_(src the_SRC)

set(target urph-fin-cli)

add_executable(${target} ${the_SRC})

target_include_directories(${target} 
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../3rd-party/cli/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../3rd-party/tabulate/include
)

target_link_libraries(${target} urph-fin-core ${CMAKE_THREAD_LIBS_INIT})
