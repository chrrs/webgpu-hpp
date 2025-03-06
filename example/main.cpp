#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <spdlog/spdlog.h>
#include <webgpu/webgpu-glfw3.hpp>
#include <webgpu/webgpu-platform.hpp>
#include <webgpu/webgpu.hpp>

#include <array>
#include <fstream>

// language=wgsl
constexpr auto WGSL_SHADER_SOURCE = R"_(
struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) color: vec4f,
}

@vertex
fn vs_main(@builtin(vertex_index) index: u32) -> VertexOutput {
    var positions = array(vec2f(-0.5, -0.5), vec2f(0.5, -0.5), vec2f(0.0, 0.5));
    var colors = array(vec3f(1.0, 0.0, 0.0), vec3f(0.0, 1.0, 0.0), vec3f(0.0, 0.0, 1.0));

    var output: VertexOutput;
    output.position = vec4f(positions[index], 0.0, 1.0);
    output.color = vec4f(colors[index], 1.0);
    return output;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    return in.color;
}
)_";

class Engine {
public:
    Engine() {
        createWindow();
        initializeWGPU();
        createPipeline();
    }

    ~Engine() {
        m_pipeline.release();

        m_surface.unconfigure();

        m_queue.release();
        m_device.release();
        m_surface.release();

        glfwDestroyWindow(m_window);
        glfwTerminate();
    }

    void run() {
        while (!glfwWindowShouldClose(m_window)) {
            glfwPollEvents();

            auto surfaceTexture = getSurfaceTexture();
            auto targetView = getSurfaceTextureView(surfaceTexture);

            // Create command encoder
            wgpu::CommandEncoderDescriptor encoderDescriptor {};
            auto encoder = m_device.createCommandEncoder(&encoderDescriptor);

            // Encode render pass
            wgpu::RenderPassColorAttachment colorAttachment {
                .view = targetView,
                .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
                .loadOp = wgpu::LoadOp::Clear,
                .storeOp = wgpu::StoreOp::Store,
                .clearValue = { 0.0f, 0.0f, 0.0f, 1.0f },
            };

            wgpu::RenderPassDescriptor renderPassDescriptor {
                .colorAttachments = { 1, &colorAttachment },
            };

            // Tell the renderer what we want to draw
            auto renderPass = encoder.beginRenderPass(renderPassDescriptor);

            renderPass.setPipeline(m_pipeline);
            renderPass.draw(3, 1, 0, 0);

            renderPass.end();
            renderPass.release();

            // Submit encoder
            wgpu::CommandBufferDescriptor commandBufferDescriptor {};
            auto commandBuffer = encoder.finish(&commandBufferDescriptor);
            encoder.release();

            m_queue.submit({ 1, &commandBuffer });
            commandBuffer.release();
            targetView.release();

            m_surface.present();
            surfaceTexture.release();

            wgpu::platform::tickDevice(m_device);
        }
    }

    Engine(Engine&) = delete;
    Engine(Engine&&) = delete;
    Engine& operator=(Engine&) = delete;
    Engine& operator=(Engine&&) = delete;

private:
    void createWindow() {
        assert(glfwInit());

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        m_window = glfwCreateWindow(800, 600, "webgpu-hpp-example", nullptr, nullptr);
        assert(m_window);
    }

    void initializeWGPU() {
        // Set up WGPU platform logging
        wgpu::platform::setLogLevel(wgpu::platform::LogLevel::Info);
        wgpu::platform::setLogCallback(
            [](wgpu::platform::LogLevel level, wgpu::StringView message, void*) {
                spdlog::level::level_enum spdLevel = level == wgpu::platform::LogLevel::Trace
                    ? spdlog::level::trace
                    : level == wgpu::platform::LogLevel::Debug
                    ? spdlog::level::debug
                    : level == wgpu::platform::LogLevel::Info
                    ? spdlog::level::info
                    : level == wgpu::platform::LogLevel::Warn
                    ? spdlog::level::warn
                    : spdlog::level::err;
                spdlog::log(spdLevel, "wgpu log: {}", static_cast<std::string_view>(message));
            },
            nullptr);

        // Create WGPU instance
        auto instance = wgpu::createInstance(nullptr);
        assert(instance);

        // Request adapter
        m_surface = wgpu::glfw3::createWindowSurface(instance, m_window);
        assert(m_surface);

        wgpu::RequestAdapterOptions adapterOptions {
            .powerPreference = wgpu::PowerPreference::HighPerformance,
            .compatibleSurface = m_surface,
        };

        wgpu::Adapter adapter;

        bool success = false;
        wgpu::RequestAdapterCallbackInfo adapterCallbackInfo {
            .callback = [](wgpu::RequestAdapterStatus status, wgpu::Adapter adapter, wgpu::StringView message, void* adapterPtr, void* success) {
                if (status == wgpu::RequestAdapterStatus::Success) {
                    *static_cast<wgpu::Adapter*>(adapterPtr) = adapter;
                    *static_cast<bool*>(success) = true;
                } else {
                    spdlog::error("failed to request adapter: {}", static_cast<std::string_view>(message));
                    *static_cast<bool*>(success) = false;
                } },
            .userdata1 = &adapter,
            .userdata2 = &success,
        };

        instance.requestAdapter(&adapterOptions, adapterCallbackInfo);
        assert(success);

        instance.release();

        wgpu::AdapterInfo adapterInfo;
        adapter.getInfo(adapterInfo);
        std::array backends { "Undefined", "Null", "WebGPU", "DirectX 11",
            "DirectX 12", "Metal", "Vulkan", "OpenGL", "OpenGL ES" };
        spdlog::info("using {} ({})", static_cast<std::string_view>(adapterInfo.device), backends[static_cast<uint32_t>(adapterInfo.backendType)]);

        // Request device
        wgpu::DeviceDescriptor deviceDescriptor {
            .deviceLostCallbackInfo = {
                .callback = [](wgpu::Device const&, wgpu::DeviceLostReason, wgpu::StringView message, void*, void*) {
                    spdlog::error("device lost: {}", static_cast<std::string_view>(message));
                },
            },
            .uncapturedErrorCallbackInfo = {
                .callback = [](wgpu::Device const&, wgpu::ErrorType, wgpu::StringView message, void*, void*) {
                    spdlog::error("uncaptured WGPU error: {}", static_cast<std::string_view>(message));
                },
            },
        };

        success = false;
        wgpu::RequestDeviceCallbackInfo deviceCallbackInfo {
            .callback = [](wgpu::RequestDeviceStatus status, wgpu::Device device, wgpu::StringView message, void* devicePtr, void* success) {
                if (status == wgpu::RequestDeviceStatus::Success) {
                    *static_cast<wgpu::Device*>(devicePtr) = device;
                    *static_cast<bool*>(success) = true;
                } else {
                    spdlog::error("failed to request device: {}", static_cast<std::string_view>(message));
                    *static_cast<bool*>(success) = false;
                } },
            .userdata1 = &m_device,
            .userdata2 = &success,
        };

        adapter.requestDevice(&deviceDescriptor, deviceCallbackInfo);
        assert(success);

        // Fetch queue
        m_queue = m_device.getQueue();

        // Configure surface
        wgpu::SurfaceCapabilities surfaceCapabilities {};
        auto status = m_surface.getCapabilities(adapter, surfaceCapabilities);
        assert(status == wgpu::Status::Success);

        m_surfaceFormat = surfaceCapabilities.formats[0];

        wgpu::SurfaceConfiguration surfaceConfig {
            .device = m_device,
            .format = m_surfaceFormat,
            .usage = wgpu::TextureUsage::RenderAttachment,
            .width = 800,
            .height = 600,
            .alphaMode = wgpu::CompositeAlphaMode::Auto,
            .presentMode = wgpu::PresentMode::Fifo,
        };

        m_surface.configure(surfaceConfig);

        adapter.release();
    }

    void createPipeline() {
        // Load shaders
        wgpu::ShaderSourceWGSL shaderSource {
            .code = WGSL_SHADER_SOURCE,
        };

        wgpu::ShaderModuleDescriptor shaderModuleDescriptor {
            .next = &shaderSource.chain,
            .label = "Triangle Shader",
        };

        auto shaderModule = m_device.createShaderModule(shaderModuleDescriptor);

        // Create pipeline
        wgpu::BlendState blendState {
            .color = {
                .operation = wgpu::BlendOperation::Add,
                .srcFactor = wgpu::BlendFactor::SrcAlpha,
                .dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha,
            },
            .alpha = {
                .operation = wgpu::BlendOperation::Add,
                .srcFactor = wgpu::BlendFactor::Zero,
                .dstFactor = wgpu::BlendFactor::One,
            },
        };

        wgpu::ColorTargetState colorTarget {
            .format = m_surfaceFormat,
            .blend = &blendState,
            .writeMask = wgpu::ColorWriteMask::All,
        };

        wgpu::FragmentState fragmentState {
            .module = shaderModule,
            .entryPoint = "fs_main",
            .targets = { 1, &colorTarget },
        };

        wgpu::RenderPipelineDescriptor pipelineDescriptor {
            .vertex = {
                .module = shaderModule,
                .entryPoint = "vs_main",
            },
            .primitive = {
                .topology = wgpu::PrimitiveTopology::TriangleList,
                .stripIndexFormat = wgpu::IndexFormat::Undefined,
                .frontFace = wgpu::FrontFace::CCW,
                .cullMode = wgpu::CullMode::Back,
            },
            .multisample = {
                .count = 1,
                .mask = ~static_cast<uint32_t>(0),
            },
            .fragment = &fragmentState,
        };

        m_pipeline = m_device.createRenderPipeline(pipelineDescriptor);
        assert(m_pipeline);

        shaderModule.release();
    }

    /// Return the texture from the current surface
    [[nodiscard]] wgpu::Texture getSurfaceTexture() {
        wgpu::SurfaceTexture surfaceTexture {};
        m_surface.getCurrentTexture(surfaceTexture);

        if (surfaceTexture.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal
            && surfaceTexture.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal) {
            return {};
        }

        return surfaceTexture.texture;
    }

    /// Return a texture view into the current surface texture
    [[nodiscard]] static wgpu::TextureView getSurfaceTextureView(wgpu::Texture surfaceTexture) {

        // Create a new texture view
        wgpu::TextureViewDescriptor textureViewDescriptor {
            .label = "Surface Texture View",
            .format = surfaceTexture.getFormat(),
            .dimension = wgpu::TextureViewDimension::_2D,
            .baseMipLevel = 0,
            .mipLevelCount = 1,
            .baseArrayLayer = 0,
            .arrayLayerCount = 1,
            .aspect = wgpu::TextureAspect::All,
        };

        return surfaceTexture.createView(&textureViewDescriptor);
    }

    GLFWwindow* m_window {};

    wgpu::Device m_device {};
    wgpu::Queue m_queue {};

    wgpu::TextureFormat m_surfaceFormat {};
    wgpu::Surface m_surface {};
    wgpu::RenderPipeline m_pipeline {};
};

int main() {
    spdlog::set_level(spdlog::level::trace);

    Engine engine;
    engine.run();

    return 0;
}