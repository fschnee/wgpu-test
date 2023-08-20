#pragma once

#include "webgpu.hpp"

#include <GLFW/glfw3.h>

#include "standalone/aliases.hpp"

#include "m.hpp"

#include <optional>
#include <array>

struct wgpudesc
{
    std::optional<WGPUInstanceDescriptor> instance = std::nullopt;
    WGPURequestAdapterOptions adapter = {};
    WGPUDeviceDescriptor      device = {};
    std::optional<WGPURequiredLimits> device_limits = std::nullopt;

    WGPUBindGroupLayoutEntry binding_layouts[3];
    WGPUBindGroupLayoutDescriptor binding_descriptor;
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
    WGPUBindGroupDescriptor bind_group_descriptor = {};

    WGPUBufferDescriptor index_buffer = {};

    WGPUBufferDescriptor vertex_buffer = {};
    WGPUBufferDescriptor color_buffer = {};
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

    standalone::u64 frame = 0;

    standalone::u32 w = 640;
    standalone::u32 h = 480;

    wgpu::Instance instance = {nullptr};
    wgpu::Surface surface = {nullptr};
    wgpu::Adapter adapter = {nullptr};
    wgpu::Device device = {nullptr};
    wgpu::Sampler sampler = {nullptr};
    wgpu::BindGroupLayout bind_group_layout = {nullptr};
    wgpu::PipelineLayout pipeline_layout = {nullptr};
    wgpu::BindGroup bind_group = {nullptr};
    wgpu::Buffer scene_uniform_buffer = {nullptr};
    wgpu::Buffer object_uniform_buffer = {nullptr};
    wgpu::Buffer vertex_buffer = {nullptr};
    wgpu::Buffer color_buffer = {nullptr};
    wgpu::Buffer index_buffer = {nullptr};
    wgpu::SwapChain swapchain = {nullptr};
    wgpu::RenderPipeline pipeline = {nullptr};
    wgpu::ShaderModule shader = {nullptr};

    wgpu::Texture depth_texture = {nullptr};
    wgpu::TextureView depth_texture_view = {nullptr};

    wgpulimits limits = {};

    // Same for all objects within a scene.
    struct scene_uniforms
    {
        m4f view;
        m4f projection;
        float time;

        float _pad[3];
    };

    // Changes per-object.
    struct object_uniforms
    {
        m4f transform;
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

    auto set_resolution(standalone::u32 nw, standalone::u32 nh) -> context&;

    enum class loop_message {do_break, do_continue};

    using loop_callback = loop_message(*)(float dt, context& context, void* userdata);
    auto loop(void* userdata, loop_callback fn) -> void;
};
