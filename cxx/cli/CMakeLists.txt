find_package(Threads REQUIRED)
set(Boost_USE_STATIC_LIBS ON)
find_package(Boost COMPONENTS date_time REQUIRED)
if (Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
    link_directories(${Boost_LIBRARY_DIRS})
endif ()

add_src_libs_(src)

set(target urph-fin-cli)

add_executable(${target} ${the_SRC})

target_include_directories(${target} 
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../3rd-party/cli/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../3rd-party/tabulate/include
)

target_link_libraries(${target} urph-fin-core ${CMAKE_THREAD_LIBS_INIT})
