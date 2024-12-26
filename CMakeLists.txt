cmake_minimum_required(VERSION 3.13)
project(Benchmark)

#set(CMAKE_VERBOSE_MAKEFILE ON)
include(./third_party/vendor_tools/vendor.cmake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
cmake_policy(SET CMP0063 NEW)

list(APPEND BENCH_INCLUDES src)
file(GLOB_RECURSE SRC_FILES 
        src/*.*
        include/*.*)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  list(APPEND BENCH_COMPILE_OPTIONS -Werror -Wall -Wextra -Weffc++ -Wconversion -pedantic -Werror=return-type -Wno-unused-command-line-argument)
endif ()

if (NOT TGFX_DIR)
  set(TGFX_DIR third_party/tgfx)
else ()
  set(HAS_CUSTOM_TGFX_DIR ON)
endif ()
get_filename_component(TGFX_DIR "${TGFX_DIR}" REALPATH BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

list(APPEND BENCH_INCLUDES ${TGFX_DIR}/include)
add_subdirectory(${TGFX_DIR})

add_library(${PROJECT_NAME} STATIC ${SRC_FILES}
        src/benchmark/SolidRectBench.cpp
        src/benchmark/SolidRectBench.h)
merge_libraries_into(${PROJECT_NAME} $<TARGET_FILE:tgfx>)

target_include_directories(${PROJECT_NAME} PUBLIC include PRIVATE ${BENCH_INCLUDES})
target_compile_options(${PROJECT_NAME} PUBLIC ${BENCH_COMPILE_OPTIONS})
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME libbenchmark)
