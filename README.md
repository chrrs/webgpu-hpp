# webgpu-hpp

> [!WARNING]
> Only Windows has been tested at this time. While the project does include support for other platforms, the code is
> untested, and it's unknown whether it will even compile.

Automatically generated opinionated idiomatic C++20 wrapper and utilities for WebGPU, adding features such as
type-safety for enums
and bitflags and interop with STL containers, while not introducing any runtime overhead.

This project also serves as a simple way to use wgpu-native with CMake.

---

*Note that the actual `webgpu.hpp` header file is **not** checked into this repository, as it is generated directly from
the spec when CMake configures the project.*

## Quick example

This library turns the verbose C-style code from this:

```c++
WGPURenderPassColorAttachment colorAttachment;
colorAttachment.view = targetView;
colorAttachment.loadOp = WGPULoadOp_Clear;
colorAttachment.storeOp = WGPUStoreOp_Store;
colorAttachment.clearValue = { 0.0f, 0.0f, 0.0f, 1.0f };

WGPURenderPassDescriptor descriptor;
descriptor.colorAttachmentCount = 1;
descriptor.colorAttachments = &colorAttachment;

auto renderPass = wgpuCommandEncoderBeginRenderPass(encoder, &descriptor);

wgpuRenderPassEncoderSetPipeline(renderPass, pipeline);
wgpuRenderPassEncoderDraw(renderPass, 3, 1, 0, 0);

wgpuRenderPassEncoderEnd(renderPass);
wgpuRenderPassEncoderRelease(renderPass);
```

into idiomatic C++-style code such as this:

```c++
wgpu::RenderPassColorAttachment colorAttachment {
    .view = targetView,
    .loadOp = wgpu::LoadOp::Clear,
    .storeOp = wgpu::StoreOp::Store,
    .clearValue = { 0.0f, 0.0f, 0.0f, 1.0f },
};

wgpu::RenderPassDescriptor descriptor {
    .colorAttachments = &colorAttachment,
};

auto renderPass = encoder.beginRenderPass(descriptor);

renderPass.setPipeline(pipeline);
renderPass.draw(3, 1, 0, 0);

renderPass.end();
renderPass.release();
```

---

For a more thorough example, see the project in the `example` folder, which renders a basic triangle to the screen.

## Installation

> [!IMPORTANT]
> Make sure you have some version of Python 3 installed on your system with the `pyyaml` package installed.

Clone this repository into a subdirectory in your CMake project, and add it to your project:

```cmake
add_subdirectory(webgpu-hpp)

target_link_libraries(MyProject PRIVATE webgpu-hpp)
target_copy_webgpu_binaries(MyProject)
```

---

You can also use `FetchContent`:

```cmake
include(FetchContent)

FetchContent_Declare(webgpu-hpp GIT_REPOSITORY https://github.com/chrrs/webgpu-hpp GIT_TAG main)
FetchContent_MakeAvailable(webgpu-hpp)

target_link_libraries(MyProject PRIVATE webgpu-hpp)
target_copy_webgpu_binaries(MyProject)
```

---

Note the call to the `target_copy_webgpu_binaries` function. This copies the runtime binaries to the target folder at
build time, which are needed to run your application.

---

After this, you can include the header:

```c++
#include <webgpu/webgpu.hpp>
```

## Usage

> [!TIP]
> The code generator is designed to output human-readable code! If you're ever unsure about something, looking at the
> header might give you extra information.

### `wgpu` namespace

All WebGPU-related functions, structs and objects are located in the `wgpu` namespace. Their name is the same as their C
equivalent, without the `wgpu` prefix.

For example, `wgpuCreateInstance` becomes `wgpu::createInstance`, and the type
`WGPUDeviceDescriptor` becomes `wgpu::DeviceDescriptor`.

### Objects

Objects in WebGPU are reference-counted, and often have operations associated with them.

A common pattern you'll see is functions like `wgpuInstanceRequestAdapter(instance, ...)`. These functions have been
converted to member functions. The previous example, for example, will become `instance.requestAdapter(...)`.

This also extends to ownership management. Each object has two methods, which are called `addRef` and `release`. All
objects still have to be manually released as you would do in C.

Currently, webgpu-hpp does not support RAII handles, but this may change in the future.

```c++
auto renderPass = encoder.beginRenderPass(renderPassDescriptor);

renderPass.setPipeline(pipeline);
renderPass.draw(3, 1, 0, 0);

renderPass.end();
renderPass.release();
```

### Structures

Structs in webgpu-hpp are a mostly 1-to-1 conversion from their C-equivalents. An important difference is that a lot
of the structs have their defaults set as recommended by the spec, or zero otherwise. This means that in a lot of
cases, if you don't care about a field, you can safely leave it out.

```c++
// Note that RequestAdapterOptions has a lot more fields that are left out.
wgpu::RequestAdapterOptions adapterOptions {
    .powerPreference = wgpu::PowerPreference::HighPerformance,
};
```

### Extensions

Some structures in WebGPU support extension chains. In webgpu-hpp, any struct supporting this has a
`ChainedStruct* next` member. Every extension has a `ChainedStruct chain` member, which can be assigned to the `next`
pointer of the struct you wish to extend.

Note that for any extension, `chain.sType` is set automatically!

```c++
// Extension: Shader source
wgpu::ShaderSourceWGSL shaderSource {
    .code = MY_WGSL_SHADER_SOURCE,
};

// Extendable: Shader module
wgpu::ShaderModuleDescriptor shaderModuleDescriptor {
    .next = &shaderSource.chain,
    .label = "Triangle Shader",
};
```

### Enums

Enums are represented by enum classes in order to achieve type-safety. They have no type prefix, but are scoped.

Some variants start with a number, which C++ disallows. In those cases, they are prefixed with `_` (underscore). For
example, `WGPUTextureViewDimension_2D` becomes `wgpu::TextureViewDimension::_2D`.

```c++
wgpu::RenderPipelineDescriptor pipelineDescriptor {
    .primitive = {
        .topology = wgpu::PrimitiveTopology::TriangleList,
        .stripIndexFormat = wgpu::IndexFormat::Undefined,
        .frontFace = wgpu::FrontFace::CCW,
        .cullMode = wgpu::CullMode::Back,
    },
    ...
};
```

### Bitflags

To achieve type-safety, bitflags are handled by the `wgpu::Flags<>` wrapper.

The individual bits are defined as an enum class, identically to normal enums. Relevant operators, such as `|` (Bitwise
OR), `&` (Bitwise AND) and `^` (Bitwise XOR) have been overloaded to automatically create the wrapper class.

This class will function identically to normal bitflags, with the added benefit of not allowing mismatched flags.

```c++
wgpu::ColorTargetState colorTarget {
    .writeMask = wgpu::ColorWriteMask::Red | wgpu::ColorWriteMask::Green
        | wgpu::ColorWriteMask::Blue | wgpu::ColorWriteMask::Alpha,
    ...
};
```

### String views

Strings in WebGPU are not NUL-terminated, and have an additional length attached to them.

To make this easier, webgpu-hpp uses `wgpu::StringView` when handling strings. This type defines implicit conversions
from a lot of commonly-used types such as `const char*`, `std::string_view` and `std::string`. It can also be converted
back to `std::string_view` and `std::string` using `static_cast`.

You can manually create a string view by using the `StringView(size_t length, const char* data)` constructor.

```c++
// Creating string views:
wgpu::DeviceDescriptor deviceDescriptor {
    .label = "my-device",
};

// Using string views:
auto deviceLostCallback = [](wgpu::Device const&, wgpu::DeviceLostReason, wgpu::StringView message, void*, void*) {
    spdlog::error("device lost: {}", static_cast<std::string_view>(message));
};
```

### Arrays

Similarly to strings, arrays in WebGPU are a combination of their size, and a pointer to their first element.

The C header defines the count and pointer separately in all cases. webgpu-hpp, however, combines them into a single
`wgpu::Array<>`. This type defines implicit conversions from a lot of commonly-used types such as `std::array<>`,
`std::vector<>` and `std::span<>`.

No conversion is defined for raw pointers, as they have no size information attached. These can be used with the
`Array(size_t count, T* data)` constructor.

```c++
wgpu::RenderPassDescriptor renderPassDescriptor {
    .colorAttachments = { 1, &colorAttachment },
};

auto buffers = std::array { buffer1, buffer2, buffer3 };
m_queue.submit(buffers);
```

### Callbacks

Callbacks are defined using their corresponding CallbackInfo struct, which defines the function pointer, callback mode
and user data.

Every callback function takes two user data `void*` arguments, which are directly passed from the CallbackInfo struct.
Some callbacks also take a callback mode, in which case a `mode` member is present in the info struct. When not
specified, this is defaulted to `wgpu::CallbackMode::AllowSpontaneous`.

Callbacks in webgpu-hpp are somewhat limited. Notably, *lambda captures are not supported!*

```c++
// Simple callback.
wgpu::DeviceLostCallbackInfo deviceLostCallbackInfo {
    .callback = [](wgpu::Device const&, wgpu::DeviceLostReason, wgpu::StringView message, void*, void*) {
        spdlog::error("device lost: {}", static_cast<std::string_view>(message));
    },
};

// More complex callback using user data pointers.
bool success = false;
wgpu::RequestDeviceCallbackInfo deviceCallbackInfo {
    .callback = [](wgpu::RequestDeviceStatus status, wgpu::Device device, wgpu::StringView message, void* devicePtr, void* success) {
        if (status == wgpu::RequestDeviceStatus::Success) {
            *static_cast<wgpu::Device*>(devicePtr) = device;
            *static_cast<bool*>(success) = true;
        } else {
            spdlog::error("failed to request device: {}", static_cast<std::string_view>(message));
            *static_cast<bool*>(success) = false;
        } },
    .userdata1 = &device,
    .userdata2 = &success,
};
```

### C Interoperability

Object handles in webgpu-hpp define implicit conversations from and to their C equivalents, allowing them to be used
interchangeably with functions expecting C-style handles. Enums and structs do not offer this implicit conversion, but
can often be cast, as their memory layout is the same.

## Addional headers

### `<webgpu/webgpu-glfw3.hpp>`

The WebGPU standard and webgpu-native both do not provide an easy way to create a `wgpu::Surface` from a GLFW window.
This header contains a single function `wgpu::glfw3::createWindowSurface(wgpu::Instance, GLFWwindow*)`, which will
create the window surface for the given GLFW window.

---

As this header has some source files attached, you'll need to link it to your project:

```cmake
target_link_libraries(MyProject PRIVATE glfw webgpu-hpp webgpu-glfw3-hpp)
```

For this to work, *make sure that the webgpu-cpp CMake file is evaluated **after** GLFW!* This is so webgpu-glfw3-hpp
can link against GLFW.

### `<webgpu/webgpu-platform.hpp>`

> [!WARNING]
> `webgpu-platform.hpp` is currently very incomplete!

This header defines some non-standard WebGPU functions that are found in webgpu-native.

## Credits

- Huge thanks to the excellent [Learn WebGPU for C++](https://eliemichel.github.io/LearnWebGPU/) series by Élie Michel,
  and the projects surrounding it for inspiring this project!
- The [wgpu-native](https://github.com/gfx-rs/wgpu-native) team for creating and documenting the spec files used to
  generate the header.