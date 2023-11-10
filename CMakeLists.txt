cmake_minimum_required(VERSION 2.6)

project(THREAD_POOL_0)

aux_source_directory(./ SRC_LISTS)

add_executable(pool main.c ${SRC_LISTS})

target_link_libraries(pool pthread)