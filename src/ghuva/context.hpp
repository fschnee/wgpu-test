#pragma once

#include "webgpu.hpp"

#include <GLFW/glfw3.h>

#include "utils/aliases.hpp"
#include "utils/m4.hpp"

#include <optional>
#include <array>

namespace ghuva
{

    struct wgpudesc
    {
        std::optional<WGPUInstanceDescriptor> instance = std::nullopt;
        WGPURequestAdapterOptions adapter = {};
        WGPUDeviceDescriptor      device = {};
        std::optional<WGPURequiredLimits> device_limits = std::nullopt;

        WGPUBindGroupLayoutEntry binding_layouts[3];
        WGPUBindGroupLayoutDescriptor scene_binding_descriptor;
        WGPUBindGroupLayoutDescriptor object_binding_descriptor;
        WGPUPipelineLayoutDescriptor pipeline_layout;

        WGPUSamplerDescriptor sampler;

        WGPUSwapChainDescriptor   swapchain = {};
        WGPURenderPipelineDescriptor pipeline = {};

        std::string shader_code =
            #include "default_shader.hpp.inc"
        ;
        WGPUShaderModuleWGSLDescriptor shader_wgsl = {};
        WGPUShaderModuleDescriptor shader = {};
        WGPUBlendState blend_state = {};
        WGPUColorTargetState color_target = {};
        WGPUFragmentState fragment_state = {};

        WGPUBufferDescriptor scene_uniform_buffer = {};
        WGPUBufferDescriptor object_uniform_buffer = {};
        WGPUBindGroupEntry bindings[3];
        WGPUBindGroupDescriptor scene_bind_group_descriptor = {};
        WGPUBindGroupDescriptor object_bind_group_descriptor = {};

        WGPUBufferDescriptor index_buffer = {};

        WGPUBufferDescriptor vertex_buffer = {};
        WGPUBufferDescriptor color_buffer = {};
        WGPUBufferDescriptor normal_buffer = {};
        std::vector<WGPUVertexBufferLayout> vertex_buffer_layouts = {};
        std::vector<WGPUVertexAttribute> vertex_buffer_attributes = {};

        WGPUTextureFormat depth_stencil_format;
        WGPUDepthStencilState depth_stencil_state;
        WGPUTextureDescriptor depth_texture;
        WGPUTextureViewDescriptor depth_texture_view;
    };

    struct wgpulimits
    {
        wgpu::SupportedLimits adapter = {};
        wgpu::SupportedLimits device = {};
    };

    struct context
    {
        static auto get() -> context&;

        bool init_successful       = false;
        GLFWwindow* window         = nullptr;
        bool imgui_init_successful = false;

        ghuva::u64 frame = 0;

        ghuva::u32 w = 640;
        ghuva::u32 h = 480;

        using vertex_t = float;
        const ghuva::u64 vertex_count = 300'000;
        using index_t = ghuva::u16;
        const ghuva::u64 index_count = 100'000;
        const ghuva::u16 object_uniform_limit = 200;

        wgpu::Instance instance = {nullptr};
        wgpu::Surface surface = {nullptr};
        wgpu::Adapter adapter = {nullptr};
        wgpu::Device device = {nullptr};
        wgpu::Sampler sampler = {nullptr};
        wgpu::BindGroupLayout bind_group_layouts[2] = {{nullptr}, {nullptr}};
        wgpu::PipelineLayout pipeline_layout = {nullptr};
        wgpu::BindGroup scene_bind_group = {nullptr};
        wgpu::BindGroup object_bind_group = {nullptr};
        wgpu::Buffer scene_uniform_buffer = {nullptr};
        wgpu::Buffer object_uniform_buffer = {nullptr};
        ghuva::u32 object_uniform_stride;
        wgpu::Buffer vertex_buffer = {nullptr};
        wgpu::Buffer color_buffer = {nullptr};
        wgpu::Buffer normal_buffer = {nullptr};
        wgpu::Buffer index_buffer = {nullptr};
        wgpu::SwapChain swapchain = {nullptr};
        wgpu::RenderPipeline pipeline = {nullptr};
        wgpu::ShaderModule shader = {nullptr};

        wgpu::Texture depth_texture = {nullptr};
        wgpu::TextureView depth_texture_view = {nullptr};

        wgpulimits limits = {};

        // https://mehmetoguzderin.github.io/webgpu/wgsl.html -> A structure inherits the worst-case alignment of any of its members.

        // Same for all objects within a scene.
        struct alignas(64) scene_uniforms
        {
            alignas(64) ghuva::m4f view;
            alignas(64) ghuva::m4f projection;
            alignas(16) std::array<float, 3> light_direction;
            alignas(16) std::array<float, 3> light_color;
            alignas(4)  float time;
            alignas(4)  float gamma;
        };

        // Changes per-object.
        struct alignas(64) object_uniforms
        {
            alignas(64) ghuva::m4f transform;
        };

        auto init_instance()
        {
            if(!desc.instance) instance = wgpu::Instance{nullptr};
            else               instance = wgpu::createInstance(desc.instance.value());
        }

        auto init_instance(decltype(wgpudesc::instance) const& i)
        {
            desc.instance = i;
            init_instance();
        }

        auto init_surface(wgpu::Surface const& s = {nullptr}) { surface = s; }

        wgpudesc desc = {};

        //auto wgpu_active_settings() -> wgpudata&;   // You shouldn't modify this.
        //auto wgpu_pending_settings() -> wgpudata&;  // Modify this instead, then call the recreate functions.

    private:
        bool new_resolution = false;

    public:
        ~context();

        enum class context_error
        {
            failed_to_init_glfw,
            failed_to_open_window,
            failed_to_create_adapter,
            failed_to_init_imgui
    };

        // throws context_error on failure.
        auto init_all() -> context&;
        // Calls the callback on failure.
        auto init_all(auto fn) -> context&
        {
            try{ return init_all(); }
            catch(context_error const& e)
            {
                fn(e, *this);
                return *this;
            }
        }

        auto imgui_new_frame() -> void;
        auto imgui_render(WGPURenderPassEncoder) -> void;

        auto init_glfw() -> context&;
        auto init_imgui(WGPUDevice, WGPUTextureFormat swapchainformat, WGPUTextureFormat depthbufferformat = WGPUTextureFormat_Undefined) -> context&;

        auto set_resolution(ghuva::u32 nw, ghuva::u32 nh) -> context&;

        enum class loop_message {do_break, do_continue};

        using loop_callback = loop_message(*)(float dt, context& context, void* userdata);
        auto loop(void* userdata, loop_callback fn) -> void;
    };

}