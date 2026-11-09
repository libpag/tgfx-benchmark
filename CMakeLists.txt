cmake_minimum_required(VERSION 3.13)
project(TGFX-Benchmark)

if (APPLE)
    enable_language(OBJC)
    enable_language(OBJCXX)
endif ()

#set(CMAKE_VERBOSE_MAKEFILE ON)
include(./third_party/vendor_tools/vendor.cmake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
cmake_policy(SET CMP0063 NEW)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/BenchmarkBackend.cmake)
configure_benchmark_backend()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    list(APPEND BENCH_COMPILE_OPTIONS -Werror -Wall -Wextra -Weffc++ -Wconversion -pedantic -Werror=return-type -Wno-unused-command-line-argument)
endif ()

if (MSVC)
    add_compile_options("/utf-8")
    string(TOLOWER ${MSVC_C_ARCHITECTURE_ID} ARCH)
endif (MSVC)

if (NOT TGFX_DIR)
    set(TGFX_DIR third_party/tgfx)
else ()
    set(HAS_CUSTOM_TGFX_DIR ON)
endif ()
get_filename_component(TGFX_DIR "${TGFX_DIR}" REALPATH BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
if (HAS_CUSTOM_TGFX_DIR)
    message("TGFX_DIR: ${TGFX_DIR}")
endif ()
list(APPEND BENCH_INCLUDES ${TGFX_DIR}/include)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

# The benchmark suite covers the layers and svg modules, so they are compiled into the tgfx static
# library. These are module switches, not backend switches; the backend is still selected only
# through BENCHMARK_BACKEND.
set(TGFX_BUILD_LAYERS ON)
set(TGFX_BUILD_SVG ON)
add_subdirectory(${TGFX_DIR} tgfx EXCLUDE_FROM_ALL)

list(APPEND BENCH_INCLUDES src)
file(GLOB_RECURSE SRC_FILES
        src/base/*.cpp
        src/benchmark/*.cpp)
file(GLOB_RECURSE CORE_FILES
        src/core/*.cpp
        src/scenes/*.cpp)

if (IOS)
    list(APPEND SRC_FILES
            src/platform/ios/AppDelegate.mm
            src/platform/ios/main.mm
            src/platform/ios/TGFXWindow.mm)
    list(APPEND CORE_FILES
            src/platform/apple/SystemInfo.mm
            src/platform/apple/GLTimerQuery.mm)
    if (BENCHMARK_BACKEND_RESOLVED STREQUAL "METAL")
        list(APPEND SRC_FILES src/platform/ios/MetalWindow.mm)
        list(APPEND CORE_FILES src/platform/ios/HeadlessDeviceMetal.mm)
    endif ()
elseif (ANDROID)
    list(APPEND SRC_FILES
            src/platform/android/main.cpp
            src/platform/android/TGFXWindow.cpp)
    list(APPEND CORE_FILES
            src/platform/android/SystemInfo.cpp
            src/platform/android/HeadlessDeviceEGL.cpp
            src/platform/android/GpuTimer.cpp)
    add_library(native_app_glue STATIC
            ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)
    target_include_directories(native_app_glue PUBLIC
            ${ANDROID_NDK}/sources/android/native_app_glue)
    list(APPEND BENCHMARK_LIBS native_app_glue android log EGL GLESv3)
    set(BENCH_LINK_OPTIONS -u ANativeActivity_onCreate)
elseif (MACOS)
    list(APPEND SRC_FILES
            src/platform/mac/AppDelegate.mm
            src/platform/mac/main.mm
            src/platform/mac/TGFXWindow.mm)
    list(APPEND CORE_FILES
            src/platform/apple/SystemInfo.mm
            src/platform/apple/GLTimerQuery.mm)
    if (BENCHMARK_BACKEND_RESOLVED STREQUAL "METAL")
        list(APPEND SRC_FILES src/platform/mac/MetalWindow.mm)
        list(APPEND CORE_FILES src/platform/mac/HeadlessDeviceMetal.mm)
        find_library(METALKIT_LIB MetalKit)
        if (NOT METALKIT_LIB)
            message(FATAL_ERROR "MetalKit framework was not found.")
        endif ()
        list(APPEND BENCHMARK_LIBS ${METALKIT_LIB})
    else ()
        list(APPEND SRC_FILES src/platform/mac/CGLWindow.mm)
        list(APPEND CORE_FILES src/platform/mac/HeadlessDeviceCGL.mm)
    endif ()
elseif (WIN32)
    list(APPEND SRC_FILES
            src/platform/win/main.cpp
            src/platform/win/TGFXWindow.cpp)
    if (BENCHMARK_BACKEND_RESOLVED STREQUAL "ANGLE")
        list(APPEND SRC_FILES src/platform/win/EGLWindow.cpp)
    elseif (BENCHMARK_BACKEND_RESOLVED STREQUAL "VULKAN")
        list(APPEND SRC_FILES src/platform/win/VulkanWindow.cpp)
    elseif (BENCHMARK_BACKEND_RESOLVED STREQUAL "D3D12")
        list(APPEND SRC_FILES src/platform/win/D3D12Window.cpp)
    else ()
        list(APPEND SRC_FILES src/platform/win/WGLWindow.cpp)
    endif ()
    add_compile_definitions(NOMINMAX)
    add_compile_definitions(_USE_MATH_DEFINES)

    if (${CMAKE_SYSTEM_VERSION} VERSION_GREATER_EQUAL 6.3) # Windows 8.1 or later
        add_definitions(-D_WIN32_WINNT=0x0603)
        find_library(SHCORE_LIB SHCore)
        list(APPEND BENCHMARK_LIBS ${SHCORE_LIB})
    else ()
        add_definitions(-D_WIN32_WINNT=0x0601) # Windows 7
        find_library(USER32 user32)
        list(APPEND BENCHMARK_LIBS ${USER32})
    endif ()

    if (BENCHMARK_BACKEND_RESOLVED STREQUAL "ANGLE")
        list(APPEND BENCH_INCLUDES ${TGFX_DIR}/vendor/angle/include)
    endif ()
endif ()

execute_process(COMMAND git -C "${TGFX_DIR}" rev-parse HEAD
        OUTPUT_VARIABLE BENCH_TGFX_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
if (NOT BENCH_TGFX_COMMIT)
    set(BENCH_TGFX_COMMIT "unknown")
endif ()
execute_process(COMMAND git -C "${CMAKE_CURRENT_SOURCE_DIR}" rev-parse HEAD
        OUTPUT_VARIABLE BENCH_BENCHMARK_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
if (NOT BENCH_BENCHMARK_COMMIT)
    set(BENCH_BENCHMARK_COMMIT "unknown")
endif ()
if (NOT CMAKE_BUILD_TYPE)
    set(BENCH_BUILD_TYPE "unknown")
else ()
    set(BENCH_BUILD_TYPE "${CMAKE_BUILD_TYPE}")
endif ()

list(APPEND BENCHMARK_LIBS ${TGFX_SHARED_LIBS})
if (ANDROID)
    add_library(Benchmark SHARED ${SRC_FILES})
elseif (IOS)
    add_executable(Benchmark MACOSX_BUNDLE ${SRC_FILES})
    set_target_properties(Benchmark PROPERTIES
            MACOSX_BUNDLE_BUNDLE_NAME "TGFXBenchmark"
            MACOSX_BUNDLE_GUI_IDENTIFIER "org.tgfx.benchmark"
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ios/Info.plist")
elseif (WIN32)
    add_executable(Benchmark WIN32 ${SRC_FILES})
else ()
    add_executable(Benchmark ${SRC_FILES})
endif ()
add_dependencies(Benchmark tgfx)
target_include_directories(Benchmark PRIVATE ${BENCH_INCLUDES})
target_compile_options(Benchmark PUBLIC ${BENCH_COMPILE_OPTIONS})
target_link_libraries(Benchmark tgfx ${BENCHMARK_LIBS})
if (BENCH_LINK_OPTIONS)
    target_link_options(Benchmark PRIVATE ${BENCH_LINK_OPTIONS})
endif ()

# The platform independent benchmark runtime and the scene catalogue. Linking an OBJECT library
# keeps every scene translation unit, and therefore every static registration, in the executable.
if (CORE_FILES)
    add_library(BenchmarkCore OBJECT ${CORE_FILES})
    add_dependencies(BenchmarkCore tgfx)
    target_include_directories(BenchmarkCore PUBLIC ${BENCH_INCLUDES})
    target_compile_options(BenchmarkCore PUBLIC ${BENCH_COMPILE_OPTIONS})
    target_compile_definitions(BenchmarkCore PUBLIC
            BENCH_RESOURCES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/resources"
            BENCH_TGFX_COMMIT="${BENCH_TGFX_COMMIT}"
            BENCH_BENCHMARK_COMMIT="${BENCH_BENCHMARK_COMMIT}"
            BENCH_BACKEND_NAME="${BENCHMARK_BACKEND_RESOLVED}"
            BENCH_BUILD_TYPE="${BENCH_BUILD_TYPE}")
    if (BENCHMARK_BACKEND_RESOLVED STREQUAL "OPENGL")
        target_compile_definitions(BenchmarkCore PUBLIC BENCH_BACKEND_GL=1)
    endif ()
    target_link_libraries(Benchmark BenchmarkCore)
endif ()

if (WIN32 AND BENCHMARK_BACKEND_RESOLVED STREQUAL "ANGLE")
    add_custom_command(TARGET Benchmark POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${TGFX_DIR}/vendor/angle/win/${ARCH}/libEGL.dll
            ${TGFX_DIR}/vendor/angle/win/${ARCH}/libGLESv2.dll
            $<TARGET_FILE_DIR:Benchmark>)
endif ()
