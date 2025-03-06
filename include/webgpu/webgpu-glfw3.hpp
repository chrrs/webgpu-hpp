#pragma once

#include <GLFW/glfw3.h>
#include <webgpu/webgpu.hpp>

#ifdef __EMSCRIPTEN__
#    define GLFW_EXPOSE_NATIVE_EMSCRIPTEN
#elifdef _WIN32
#    define GLFW_EXPOSE_NATIVE_WIN32
#elifdef __APPLE__
#    define GLFW_EXPOSE_NATIVE_COCOA
#else
#    define GLFW_EXPOSE_NATIVE_X11
#    define GLFW_EXPOSE_NATIVE_WAYLAND
#endif

#ifndef __EMSCRIPTEN__
#    include <GLFW/glfw3native.h>
#endif

#if defined(GLFW_EXPOSE_NATIVE_COCOA)
#    include <QuartzCore/CAMetalLayer.hpp>
#endif

namespace wgpu::glfw3 {

inline Surface createWindowSurface(Instance instance, GLFWwindow* window)
{
    switch (glfwGetPlatform()) {
#ifdef GLFW_EXPOSE_NATIVE_X11
    case GLFW_PLATFORM_X11: {
        SurfaceSourceXlibWindow surfaceSource {
            .display = glfwGetX11Display(),
            .window = glfwGetX11Window(window),
        };

        SurfaceDescriptor surfaceDescriptor {
            .next = &surfaceSource.chain,
        };

        return instance.createSurface(surfaceDescriptor);
    }
#endif

#ifdef GLFW_EXPOSE_NATIVE_WAYLAND
    case GLFW_PLATFORM_WAYLAND: {
        SurfaceSourceWaylandSurface surfaceSource {
            .display = glfwGetWaylandDisplay(),
            .surface = glfwGetWaylandWindow(window),
        };

        SurfaceDescriptor surfaceDescriptor {
            .next = &surfaceSource.chain,
        };

        return instance.createSurface(surfaceDescriptor);
    }
#endif

#ifdef GLFW_EXPOSE_NATIVE_COCOA
    case GLFW_PLATFORM_COCOA: {
        auto metalLayer = CA::MetalLayer::layer();
        NSWindow* nsWindow = glfwGetCocoaWindow(window);

        CAMetalLayer* layer = (__bridge CAMetalLayer*)metalLayer;
        nsWindow.contentView.wantsLayer = YES;
        nsWindow.contentView.layer = layer;

        SurfaceSourceMetalLayer surfaceSource {
            .layer = layer,
        };

        SurfaceDescriptor surfaceDescriptor {
            .next = &surfaceSource.chain,
        };

        return instance.createSurface(surfaceDescriptor);
    }
#endif

#ifdef GLFW_EXPOSE_NATIVE_WIN32
    case GLFW_PLATFORM_WIN32: {
        SurfaceSourceWindowsHWND surfaceSource {
            .hinstance = GetModuleHandle(nullptr),
            .hwnd = glfwGetWin32Window(window),
        };

        SurfaceDescriptor surfaceDescriptor {
            .next = &surfaceSource.chain,
        };

        return instance.createSurface(surfaceDescriptor);
    }
#endif

    default:
        return {};
    }
}

}
