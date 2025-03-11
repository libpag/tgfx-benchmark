cmake_minimum_required(VERSION 3.13)

project(Benchmark)

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_STANDARD 17)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif ()

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../src)

if (NOT TGFX_DIR)
    set(TGFX_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/tgfx)
else ()
    message("Custom TGFX_DIR: ${TGFX_DIR}")
endif ()
add_subdirectory(${TGFX_DIR} ./tgfx)

file(GLOB_RECURSE BENCHMARK_FILES
        ../../src/base/*.*
        ../../src/benchmark/*.*
        src/*.*)

if (DEFINED EMSCRIPTEN)
    add_executable(benchmark ${BENCHMARK_FILES})
    list(APPEND BENCHMARK_COMPILE_OPTIONS -fno-rtti -DEMSCRIPTEN_HAS_UNBOUND_TYPE_NAMES=0)
    list(APPEND BENCHMARK_LINK_OPTIONS --no-entry -lembind -fno-rtti -DEMSCRIPTEN_HAS_UNBOUND_TYPE_NAMES=0 -sEXPORT_NAME='Benchmark' -sWASM=1 -sASYNCIFY
            -sMAX_WEBGL_VERSION=2 -sEXPORTED_RUNTIME_METHODS=['GL','Asyncify'] -sMODULARIZE=1
            -sENVIRONMENT='web,worker' -sEXPORT_ES6=1 -sUSE_ES6_IMPORT_META=0)
    if (EMSCRIPTEN_PTHREADS)
        list(APPEND BENCHMARK_LINK_OPTIONS -sUSE_PTHREADS=1 -sINITIAL_MEMORY=32MB -sALLOW_MEMORY_GROWTH=1
                -sPTHREAD_POOL_SIZE=navigator.hardwareConcurrency -sPROXY_TO_PTHREAD=1
                -sEXIT_RUNTIME=0 -sINVOKE_RUN=0 -sMALLOC=mimalloc)
        list(APPEND BENCHMARK_COMPILE_OPTIONS -fPIC -pthread)
    else()
        list(APPEND BENCHMARK_LINK_OPTIONS -sALLOW_MEMORY_GROWTH=1)
    endif ()
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        list(APPEND BENCHMARK_COMPILE_OPTIONS -O0 -g3)
        list(APPEND BENCHMARK_LINK_OPTIONS -O0 -g3 -sSAFE_HEAP=1 -Wno-limited-postlink-optimizations)
    else ()
        list(APPEND BENCHMARK_COMPILE_OPTIONS -Oz)
        list(APPEND BENCHMARK_LINK_OPTIONS -Oz)
    endif ()
else ()
    add_library(benchmark SHARED ${BENCHMARK_FILES})
endif ()

target_compile_options(benchmark PUBLIC ${BENCHMARK_COMPILE_OPTIONS})
target_link_options(benchmark PUBLIC ${BENCHMARK_LINK_OPTIONS})
target_link_libraries(benchmark tgfx)
