cmake_minimum_required(VERSION 3.30)
project(webgpu-hpp-example)

set(CMAKE_CXX_STANDARD 20)

include(FetchContent)
FetchContent_Declare(spdlog GIT_REPOSITORY https://github.com/gabime/spdlog.git GIT_TAG v1.x)
FetchContent_Declare(glfw GIT_REPOSITORY https://github.com/glfw/glfw.git GIT_TAG master)
FetchContent_MakeAvailable(spdlog glfw)

if (NOT TARGET webgpu-hpp)
    add_subdirectory(.. webgpu-hpp)
endif ()

add_executable(webgpu-hpp-example main.cpp)
target_link_libraries(webgpu-hpp-example PRIVATE spdlog::spdlog glfw webgpu-hpp)
target_copy_webgpu_binaries(webgpu-hpp-example)