#include <webgpu/webgpu-glfw3.hpp>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#if defined(GLFW_EXPOSE_NATIVE_COCOA)
#   include <Foundation/Foundation.h>
#   include <QuartzCore/CAMetalLayer.h>
#endif

wgpu::Surface wgpu::glfw3::createWindowSurface(wgpu::Instance instance, GLFWwindow* window)
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
            id metalLayer = [CAMetalLayer layer];
            NSWindow *nsWindow = glfwGetCocoaWindow(window);
            [ns_window.contentView setWantsLayer:YES];
            [ns_window.contentView setLayer:metalLayer];

            nsWindow.contentView.wantsLayer = YES;
            nsWindow.contentView.layer = metalLayer;

            SurfaceSourceMetalLayer surfaceSource {
                .layer = metalLayer,
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