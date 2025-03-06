#pragma once

#include <webgpu/webgpu.hpp>

struct GLFWwindow;

namespace wgpu::glfw3 {

Surface createWindowSurface(Instance instance, GLFWwindow* window);

}
