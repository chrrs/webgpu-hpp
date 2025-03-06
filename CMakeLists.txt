cmake_minimum_required(VERSION 3.24)
project(webgpu-hpp)

find_package(Python3 COMPONENTS Interpreter)

add_library(webgpu-hpp SHARED IMPORTED GLOBAL)

# Detect our architecture.
if (NOT ARCH)
    set(SYSTEM_PROCESSOR ${CMAKE_SYSTEM_PROCESSOR})
    if (SYSTEM_PROCESSOR STREQUAL "AMD64" OR SYSTEM_PROCESSOR STREQUAL "x86_64")
        if (CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(ARCH "x86_64")
        elseif (CMAKE_SIZEOF_VOID_P EQUAL 4)
            set(ARCH "i686")
        endif ()
    elseif (SYSTEM_PROCESSOR STREQUAL "arm64")
        set(ARCH "aarch64")
    endif ()
endif ()

set(WGPU ${CMAKE_CURRENT_BINARY_DIR}/wgpu)

# Find the target we want to download.
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(WGPU_TARGET_NAME wgpu-windows-${ARCH}-msvc-release)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(WGPU_TARGET_NAME wgpu-linux-${ARCH}-release)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(WGPU_TARGET_NAME wgpu-macos-${ARCH}-release)
else ()
    message(FATAL_ERROR "platform not supported by wgpu-hpp.")
endif ()

# Use fetch.py to download the sources.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        fetch.py
        gen/__init__.py
        gen/cpp_structs.py
        gen/cpp_types.py
        gen/cpp_util.py
        gen/cpp_values.py)
execute_process(
        COMMAND ${Python3_EXECUTABLE} fetch.py --bin-dir "${WGPU}/bin" --target ${WGPU_TARGET_NAME}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMAND_ERROR_IS_FATAL ANY
)

# Link to the downloaded libraries.
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(WGPU_RUNTIME_LIB ${WGPU}/bin/${WGPU_TARGET_NAME}/lib/wgpu_native.dll)
    set_target_properties(webgpu-hpp PROPERTIES
            IMPORTED_LOCATION "${WGPU_RUNTIME_LIB}"
            IMPORTED_IMPLIB "${WGPU_RUNTIME_LIB}.lib"
            INTERFACE_INCLUDE_DIRECTORIES "${WGPU}/bin/${WGPU_TARGET_NAME}/include"
    )
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(WGPU_RUNTIME_LIB ${WGPU}/bin/${WGPU_TARGET_NAME}/lib/libwgpu_native.so)
    set_target_properties(webgpu-hpp PROPERTIES
            IMPORTED_LOCATION "${WGPU_RUNTIME_LIB}"
            IMPORTED_NO_SONAME TRUE
            INTERFACE_INCLUDE_DIRECTORIES "${WGPU}/bin/${WGPU_TARGET_NAME}/include"
    )
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(WGPU_RUNTIME_LIB ${WGPU}/bin/${WGPU_TARGET_NAME}/lib/libwgpu_native.dylib)
    set_target_properties(webgpu-hpp PROPERTIES
            IMPORTED_LOCATION "${WGPU_RUNTIME_LIB}"
            INTERFACE_INCLUDE_DIRECTORIES "${WGPU}/bin/${WGPU_TARGET_NAME}/include"
    )
endif ()

message(STATUS "using WebGPU runtime from '${WGPU_RUNTIME_LIB}'")
set(WGPU_RUNTIME_LIB ${WGPU_RUNTIME_LIB} CACHE INTERNAL "path to WGPU runtime binary")

target_include_directories(webgpu-hpp INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_definitions(webgpu-hpp INTERFACE WEBGPU_BACKEND_WGPU)

# Copy WebGPU runtime binaries to the target directory.
function(target_copy_webgpu_binaries Target)
    add_custom_command(
            TARGET ${Target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${WGPU_RUNTIME_LIB}"
            "$<TARGET_FILE_DIR:${Target}>"
            COMMENT "copying '${WGPU_RUNTIME_LIB}' to '$<TARGET_FILE_DIR:${Target}>'"
    )
endfunction()

# If we're the top level project, also detect the example project.
if (PROJECT_IS_TOP_LEVEL)
    add_subdirectory(example)
endif ()