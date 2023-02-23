find_package(Threads REQUIRED)
find_package(Boost COMPONENTS date_time REQUIRED)
INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS})	

add_src_libs_(src the_SRC)

set(target urph-fin-cli)

include(FetchContent)
FetchContent_Declare(
  cli
  GIT_REPOSITORY https://github.com/daniele77/cli
  GIT_TAG v2.0.2
)
FetchContent_MakeAvailable(cli)

include(FetchContent)
FetchContent_Declare(
  tabulate
  GIT_REPOSITORY https://github.com/p-ranav/tabulate.git
  GIT_TAG v1.5
)
FetchContent_MakeAvailable(tabulate)

add_executable(${target} ${the_SRC})

target_include_directories(${target} 
    PRIVATE ${cli_SOURCE_DIR}/include
    PRIVATE ${tabulate_SOURCE_DIR}/include
)

target_link_libraries(${target} urph-fin-core ${CMAKE_THREAD_LIBS_INIT})
