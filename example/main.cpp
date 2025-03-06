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
    Engine()
    {
        create_window();
        initialize_wgpu();
        create_pipeline();
    }

    ~Engine()
    {
        m_pipeline.release();

        m_surface.unconfigure();

        m_queue.release();
        m_device.release();
        m_surface.release();

        glfwDestroyWindow(m_window);
        glfwTerminate();
    }

    void run()
    {
        while (!glfwWindowShouldClose(m_window)) {
            glfwPollEvents();

            auto surface_texture = get_surface_texture();
            auto target_view = get_surface_texture_view(surface_texture);

            // Create command encoder
            wgpu::CommandEncoderDescriptor encoder_descriptor {};
            auto encoder = m_device.createCommandEncoder(&encoder_descriptor);

            // Encode render pass
            wgpu::RenderPassColorAttachment color_attachment {
                .view = target_view,
                .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
                .loadOp = wgpu::LoadOp::Clear,
                .storeOp = wgpu::StoreOp::Store,
                .clearValue = { 0.0f, 0.0f, 0.0f, 1.0f },
            };

            wgpu::RenderPassDescriptor render_pass_descriptor {
                .colorAttachments = { 1, &color_attachment },
            };

            // Tell the renderer what we want to draw
            auto render_pass = encoder.beginRenderPass(render_pass_descriptor);

            render_pass.setPipeline(m_pipeline);
            render_pass.draw(3, 1, 0, 0);

            render_pass.end();
            render_pass.release();

            // Submit encoder
            wgpu::CommandBufferDescriptor command_buffer_descriptor {};
            auto command_buffer = encoder.finish(&command_buffer_descriptor);
            encoder.release();

            m_queue.submit({ 1, &command_buffer });
            command_buffer.release();
            target_view.release();

            m_surface.present();
            surface_texture.release();

            wgpu::platform::tickDevice(m_device);
        }
    }

    Engine(Engine&) = delete;
    Engine(Engine&&) = delete;
    Engine& operator=(Engine&) = delete;
    Engine& operator=(Engine&&) = delete;

private:
    void create_window()
    {
        assert(glfwInit());

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        m_window = glfwCreateWindow(800, 600, "webgpu-hpp-example", nullptr, nullptr);
        assert(m_window);
    }

    void initialize_wgpu()
    {
        // Set up WGPU platform logging
        wgpu::platform::setLogLevel(wgpu::platform::LogLevel::Info);
        wgpu::platform::setLogCallback(
            [](wgpu::platform::LogLevel level, wgpu::StringView message, void*) {
                spdlog::level::level_enum spd_level = level == wgpu::platform::LogLevel::Trace
                    ? spdlog::level::trace
                    : level == wgpu::platform::LogLevel::Debug
                    ? spdlog::level::debug
                    : level == wgpu::platform::LogLevel::Info
                    ? spdlog::level::info
                    : level == wgpu::platform::LogLevel::Warn
                    ? spdlog::level::warn
                    : spdlog::level::err;
                spdlog::log(spd_level, "wgpu log: {}", static_cast<std::string_view>(message));
            },
            nullptr);

        // Create WGPU instance
        auto instance = wgpu::createInstance(nullptr);
        assert(instance);

        // Request adapter
        m_surface = wgpu::glfw3::createWindowSurface(instance, m_window);
        assert(m_surface);

        wgpu::RequestAdapterOptions adapter_options {
            .powerPreference = wgpu::PowerPreference::HighPerformance,
            .compatibleSurface = m_surface,
        };

        wgpu::Adapter adapter;

        bool success = false;
        wgpu::RequestAdapterCallbackInfo adapter_callback_info {
            .callback = [](wgpu::RequestAdapterStatus status, wgpu::Adapter adapter, wgpu::StringView message, void* adapter_ptr, void* success) {
                if (status == wgpu::RequestAdapterStatus::Success) {
                    *static_cast<wgpu::Adapter*>(adapter_ptr) = adapter;
                    *static_cast<bool*>(success) = true;
                } else {
                    spdlog::error("failed to request adapter: {}", static_cast<std::string_view>(message));
                    *static_cast<bool*>(success) = false;
                } },
            .userdata1 = &adapter,
            .userdata2 = &success,
        };

        instance.requestAdapter(&adapter_options, adapter_callback_info);
        assert(success);

        instance.release();

        wgpu::AdapterInfo adapter_info;
        adapter.getInfo(adapter_info);
        std::array backends { "Undefined", "Null", "WebGPU", "DirectX 11",
            "DirectX 12", "Metal", "Vulkan", "OpenGL", "OpenGL ES" };
        spdlog::info("using {} ({})", static_cast<std::string_view>(adapter_info.device), backends[static_cast<uint32_t>(adapter_info.backendType)]);

        // Request device
        wgpu::DeviceDescriptor device_descriptor {
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
        wgpu::RequestDeviceCallbackInfo device_callback_info {
            .callback = [](wgpu::RequestDeviceStatus status, wgpu::Device device, wgpu::StringView message, void* device_ptr, void* success) {
                if (status == wgpu::RequestDeviceStatus::Success) {
                    *static_cast<wgpu::Device*>(device_ptr) = device;
                    *static_cast<bool*>(success) = true;
                } else {
                    spdlog::error("failed to request device: {}", static_cast<std::string_view>(message));
                    *static_cast<bool*>(success) = false;
                } },
            .userdata1 = &m_device,
            .userdata2 = &success,
        };

        adapter.requestDevice(&device_descriptor, device_callback_info);
        assert(success);

        // Fetch queue
        m_queue = m_device.getQueue();

        // Configure surface
        wgpu::SurfaceCapabilities surface_capabilities {};
        auto status = m_surface.getCapabilities(adapter, surface_capabilities);
        if (status != wgpu::Status::Success)
            throw std::runtime_error("failed to get surface capabilities");
        m_surface_format = surface_capabilities.formats[0];

        wgpu::SurfaceConfiguration surface_config {
            .device = m_device,
            .format = m_surface_format,
            .usage = wgpu::TextureUsage::RenderAttachment,
            .width = 800,
            .height = 600,
            .alphaMode = wgpu::CompositeAlphaMode::Auto,
            .presentMode = wgpu::PresentMode::Fifo,
        };

        m_surface.configure(surface_config);

        adapter.release();
    }

    void create_pipeline()
    {
        // Load shaders
        wgpu::ShaderSourceWGSL shader_source {
            .code = WGSL_SHADER_SOURCE,
        };

        wgpu::ShaderModuleDescriptor shader_module_descriptor {
            .next = &shader_source.chain,
            .label = "Triangle Shader",
        };

        auto shader_module = m_device.createShaderModule(shader_module_descriptor);

        // Create pipeline
        wgpu::BlendState blend_state {
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

        wgpu::ColorTargetState color_target {
            .format = m_surface_format,
            .blend = &blend_state,
            .writeMask = wgpu::ColorWriteMask::All,
        };

        wgpu::FragmentState fragment_state {
            .module = shader_module,
            .entryPoint = "fs_main",
            .targets = { 1, &color_target },
        };

        wgpu::RenderPipelineDescriptor pipeline_descriptor {
            .vertex = {
                .module = shader_module,
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
            .fragment = &fragment_state,
        };

        m_pipeline = m_device.createRenderPipeline(pipeline_descriptor);
        assert(m_pipeline);

        shader_module.release();
    }

    /// Return the texture from the current surface
    [[nodiscard]] wgpu::Texture get_surface_texture()
    {
        wgpu::SurfaceTexture surface_texture {};
        m_surface.getCurrentTexture(surface_texture);

        if (surface_texture.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal
            && surface_texture.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal) {
            return {};
        }

        return surface_texture.texture;
    }

    /// Return a texture view into the current surface texture
    [[nodiscard]] static wgpu::TextureView get_surface_texture_view(wgpu::Texture surface_texture)
    {

        // Create a new texture view
        wgpu::TextureViewDescriptor texture_view_descriptor {
            .label = "Surface Texture View",
            .format = surface_texture.getFormat(),
            .dimension = wgpu::TextureViewDimension::_2D,
            .baseMipLevel = 0,
            .mipLevelCount = 1,
            .baseArrayLayer = 0,
            .arrayLayerCount = 1,
            .aspect = wgpu::TextureAspect::All,
        };

        return surface_texture.createView(&texture_view_descriptor);
    }

    GLFWwindow* m_window {};

    wgpu::Device m_device {};
    wgpu::Queue m_queue {};

    wgpu::TextureFormat m_surface_format {};
    wgpu::Surface m_surface {};
    wgpu::RenderPipeline m_pipeline {};
};

int main()
{
    spdlog::set_level(spdlog::level::trace);

    Engine engine;
    engine.run();

    return 0;
}