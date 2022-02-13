add_src_libs_()

set(target urph-fin-cli)
add_executable(${target} ${the_SRC})

target_link_libraries(${target} urph-fin-core)
