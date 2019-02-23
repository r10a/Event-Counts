cmake_minimum_required(VERSION 2.8)
project(event-counts)

set(CMAKE_CXX_STANDARD 14)

include_directories (${CMAKE_SOURCE_DIR}/include)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11 -O3 -Wall -Wextra -Wno-unused-parameter -ggdb -g -lpthread")

aux_source_directory (${CMAKE_CURRENT_SOURCE_DIR} SRCS)

foreach (SRC_PATH ${SRCS})
    get_filename_component (SRC ${SRC_PATH} NAME_WE)
    message (${SRC})
    add_executable (${SRC} ${SRCS})
    set_target_properties (${SRC} PROPERTIES COMPILE_FLAGS ${CMAKE_CXX_FLAGS})
endforeach (SRC_PATH)