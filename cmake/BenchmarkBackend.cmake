macro(configure_benchmark_backend)
    set(BENCHMARK_BACKEND "AUTO" CACHE STRING
            "Benchmark GPU backend: AUTO, OPENGL, ANGLE, METAL, VULKAN, D3D12, WEBGL, WEBGPU")
    set_property(CACHE BENCHMARK_BACKEND PROPERTY STRINGS
            AUTO OPENGL ANGLE METAL VULKAN D3D12 WEBGL WEBGPU)

    set(_benchmark_tgfx_backend_options
            TGFX_USE_OPENGL
            TGFX_USE_ANGLE
            TGFX_USE_METAL
            TGFX_USE_VULKAN
            TGFX_USE_D3D12
            TGFX_USE_WEBGPU
            TGFX_USE_QT
            TGFX_USE_SWIFTSHADER)
    foreach (_option IN LISTS _benchmark_tgfx_backend_options)
        get_property(_option_is_cached CACHE ${_option} PROPERTY TYPE SET)
        if (_option_is_cached)
            message(FATAL_ERROR
                    "${_option} is not a supported Benchmark configuration entry. "
                    "Use -DBENCHMARK_BACKEND=<backend> instead.")
        endif ()
    endforeach ()
    get_property(_warp_is_cached CACHE TGFX_D3D12_USE_WARP PROPERTY TYPE SET)
    if (_warp_is_cached)
        get_property(_warp_value CACHE TGFX_D3D12_USE_WARP PROPERTY VALUE)
        if (_warp_value)
            message(FATAL_ERROR
                    "TGFX_D3D12_USE_WARP is not a supported Benchmark configuration entry. "
                    "Use -DBENCHMARK_BACKEND=D3D12 instead.")
        endif ()
    endif ()

    string(TOUPPER "${BENCHMARK_BACKEND}" _benchmark_backend)
    set(BENCHMARK_BACKEND "${_benchmark_backend}" CACHE STRING
            "Benchmark GPU backend: AUTO, OPENGL, ANGLE, METAL, VULKAN, D3D12, WEBGL, WEBGPU" FORCE)
    set_property(CACHE BENCHMARK_BACKEND PROPERTY STRINGS
            AUTO OPENGL ANGLE METAL VULKAN D3D12 WEBGL WEBGPU)

    if (_benchmark_backend STREQUAL "AUTO")
        if (EMSCRIPTEN)
            set(_benchmark_backend WEBGL)
        elseif (WIN32)
            set(_benchmark_backend OPENGL)
        elseif (APPLE AND NOT IOS)
            set(_benchmark_backend OPENGL)
        else ()
            message(FATAL_ERROR "BENCHMARK_BACKEND=AUTO is not available on ${CMAKE_SYSTEM_NAME}.")
        endif ()
    endif ()

    if (EMSCRIPTEN)
        set(_benchmark_supported_backends WEBGL WEBGPU)
    elseif (WIN32)
        set(_benchmark_supported_backends OPENGL ANGLE VULKAN D3D12)
    elseif (APPLE AND NOT IOS)
        set(_benchmark_supported_backends OPENGL METAL)
    else ()
        message(FATAL_ERROR
                "TGFX Benchmark does not provide a platform window for ${CMAKE_SYSTEM_NAME}.")
    endif ()

    if (NOT _benchmark_backend IN_LIST _benchmark_supported_backends)
        list(JOIN _benchmark_supported_backends ", " _benchmark_supported_text)
        message(FATAL_ERROR
                "BENCHMARK_BACKEND=${_benchmark_backend} is not supported on ${CMAKE_SYSTEM_NAME}. "
                "Supported backends: ${_benchmark_supported_text}.")
    endif ()

    set(TGFX_USE_OPENGL OFF)
    set(TGFX_USE_ANGLE OFF)
    set(TGFX_USE_METAL OFF)
    set(TGFX_USE_VULKAN OFF)
    set(TGFX_USE_D3D12 OFF)
    set(TGFX_D3D12_USE_WARP OFF)
    set(TGFX_USE_WEBGPU OFF)
    set(TGFX_USE_QT OFF)
    set(TGFX_USE_SWIFTSHADER OFF)

    if (_benchmark_backend STREQUAL "OPENGL" OR _benchmark_backend STREQUAL "WEBGL")
        set(TGFX_USE_OPENGL ON)
    elseif (_benchmark_backend STREQUAL "ANGLE")
        set(TGFX_USE_OPENGL ON)
        set(TGFX_USE_ANGLE ON)
    elseif (_benchmark_backend STREQUAL "METAL")
        set(TGFX_USE_METAL ON)
    elseif (_benchmark_backend STREQUAL "VULKAN")
        set(TGFX_USE_VULKAN ON)
    elseif (_benchmark_backend STREQUAL "D3D12")
        set(TGFX_USE_D3D12 ON)
    elseif (_benchmark_backend STREQUAL "WEBGPU")
        set(TGFX_USE_WEBGPU ON)
    endif ()

    set(BENCHMARK_BACKEND_RESOLVED "${_benchmark_backend}")
    message(STATUS "Benchmark GPU backend: ${BENCHMARK_BACKEND_RESOLVED}")
endmacro()
