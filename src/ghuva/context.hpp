// TODO: reoder struct member for more readability.
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
        // TODO: delete all the unused descriptors.
        WGPURequestAdapterOptions adapter = {};
        WGPUDeviceDescriptor      device = {};
        std::optional<WGPURequiredLimits> device_limits = std::nullopt;

        WGPUBindGroupLayoutDescriptor scene_binding_descriptor;
        WGPUBindGroupLayoutDescriptor object_binding_descriptor;
        WGPUPipelineLayoutDescriptor pipeline_layout;

        WGPUSwapChainDescriptor swapchain = {};

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

        WGPUTextureFormat swapchain_format;
        WGPUTextureFormat depth_stencil_format;
        WGPUTextureDescriptor depth_texture;
        WGPUTextureViewDescriptor depth_texture_view;

        WGPUBindGroupLayoutDescriptor compute_bind_group_layout_descriptor;
        WGPUBindGroupDescriptor compute_binding_descriptor;
        WGPUPipelineLayoutDescriptor compute_pipeline_layout;
        WGPUComputePipelineDescriptor compute_pipeline;
        WGPUCommandEncoderDescriptor compute_encoder_descriptor;
        WGPUComputePassDescriptor compute_pass;
    };

    struct wgpulimits
    {
        wgpu::SupportedLimits adapter = {};
        wgpu::SupportedLimits device = {};
    };

    struct context
    {
        using vertex_t = f32;
        using index_t  = u16;

        // Gets the singleton for this class.
        static auto get() -> context&;

        enum class context_error
        {
            failed_to_init_glfw,
            failed_to_open_window,
            failed_to_create_adapter,
            failed_to_init_imgui
        };
        // throws context_error on failure.
        auto init_all() -> context&;

        enum class loop_message {do_break, do_continue};
        using loop_callback = loop_message(*)(context& context, float dt, void* userdata);
        auto loop(void* userdata, loop_callback fn) -> void;

        auto set_resolution(ghuva::u32 nw, ghuva::u32 nh) -> context&;

        // Call this on your loop before ImGui functions and after set_resolution().
        auto imgui_new_frame() -> void;

        // https://mehmetoguzderin.github.io/webgpu/wgsl.html -> A structure inherits the worst-case alignment of any of its members.
        // Same for all objects within a scene.
        struct alignas(64) scene_uniforms
        {
            alignas(64) ghuva::m4f view;
            alignas(64) ghuva::m4f projection;
            alignas(16) std::array<f32, 3> light_direction;
            alignas(16) std::array<f32, 3> light_color;
            alignas(4)  f32 time;
            alignas(4)  f32 gamma;
        };
        // There should be only 1 instance of scene_uniforms in this buffer.
        wgpu::Buffer scene_uniform_buffer = {nullptr};

        struct alignas(64) compute_object_uniforms
        {
            alignas(16) std::array<f32, 3> pos;
            alignas(16) std::array<f32, 3> rot;
            alignas(16) std::array<f32, 3> scale;
        };
        wgpu::Buffer compute_object_uniform_buffer = {nullptr};
        // Changes per-object.
        // This is passed to the compute shader to be made into transform matrixes
        // that are then used by the vertex/fragment shaders.
        // Either write the pos, rot, scale on each line of the transform and do the
        // compute pass or do as specified below.
        // If you wanna do the conversion cpu-side then skip the compute pass and
        // write these directly to the object_uniform_buffer.
        struct alignas(64) object_uniforms
        {
            alignas(64) ghuva::m4f transform;
        };
        wgpu::Buffer object_uniform_buffer = {nullptr};
        // You can have this many object_uniforms in object_uniform_buffer.
        const ghuva::u32 object_uniform_limit = 100'000;
        // And this is the stride of the elements.
        ghuva::u32 object_uniform_stride;

        // Write into the object_uniform_buffer, then
        // get a compute_pass from begin_compute().
        auto begin_compute() -> wgpu::ComputePassEncoder;
        // Dispatch the work and then call end_compute().
        auto end_compute(wgpu::ComputePassEncoder compute_pass) -> void;

        wgpu::Buffer vertex_buffer = {nullptr};
        wgpu::Buffer color_buffer = {nullptr};
        wgpu::Buffer normal_buffer = {nullptr};
        wgpu::Buffer index_buffer = {nullptr};
        // The limits for these buffers
        const ghuva::u64 vertex_count = 3'000'000;
        const ghuva::u64 index_count  = 1'000'000;

        // Write into the vertex buffers as needed, then
        // get your render_pass from begin_render().
        // Also, don't forget to pass in the swapchain texture.
        auto begin_render(wgpu::TextureView render_view) -> wgpu::RenderPassEncoder;
        // When you issued all your draw calls, render your imgui
        // stuff into the render as well.
        auto imgui_render(WGPURenderPassEncoder render_pass) -> void;
        // And finally end_render() to present.
        // Don't forget to drop the texture you passed into begin_render().
        auto end_render(wgpu::RenderPassEncoder render_pass) -> void;

        // The current frame.
        ghuva::u64 frame = 0;
        // And the current window width and height.
        ghuva::u32 w = 640;
        ghuva::u32 h = 480;

        wgpu::Instance instance = {nullptr};
        wgpu::Surface surface = {nullptr};
        wgpu::Adapter adapter = {nullptr};
        wgpu::Device device = {nullptr};
        wgpu::Sampler sampler = {nullptr};
        wgpu::BindGroupLayout bind_group_layouts[4] = {{nullptr}, {nullptr}, {nullptr}, {nullptr}};
        wgpu::PipelineLayout pipeline_layout = {nullptr};
        wgpu::BindGroup scene_bind_group = {nullptr};
        wgpu::BindGroup object_bind_group = {nullptr};
        wgpu::BindGroup compute_bind_group = {nullptr};

        wgpu::SwapChain swapchain = {nullptr};
        wgpu::ComputePipeline compute_pipeline = {nullptr};
        wgpu::RenderPipeline pipeline = {nullptr};
        wgpu::ShaderModule shader = {nullptr};
        wgpu::ShaderModule compute_shader = {nullptr};
        wgpu::PipelineLayout compute_pipeline_layout = {nullptr};

        // begin_compute and end_compute stuff.
        wgpu::CommandEncoder compute_encoder = {nullptr};

        // begin_render and end_render stuff.
        wgpu::CommandEncoder encoder = {nullptr};
        wgpu::RenderPassColorAttachment color_attachment = {};
        wgpu::RenderPassDepthStencilAttachment depth_attachment = {};
        wgpu::TextureView render_view = {nullptr};

        wgpu::Texture depth_texture = {nullptr};
        wgpu::TextureView depth_texture_view = {nullptr};

        wgpulimits limits = {};
        wgpudesc desc = {};

        ~context();

    private:
        bool init_successful       = false;
        GLFWwindow* window         = nullptr;
        bool imgui_init_successful = false;

        bool new_resolution = false;

        auto init_glfw() -> void;
        auto init_instance() -> void;
        auto init_surface() -> void;
        auto init_device() -> void;
        auto init_swapchain() -> void;
        auto init_imgui() -> void;
        auto init_buffers() -> void;
        auto init_bindings() -> void;
        auto init_render_pipeline() -> void;
        auto init_compute_pipeline() -> void;
        auto init_textures() -> void;

        // Wrappers around wgpu verbosity.
        auto create_wgsl_shader(std::string code, std::string label = "unnamed shader") -> wgpu::ShaderModule;
        auto create_bind_group_layout(
            std::vector<WGPUBindGroupLayoutEntry> entries,
            std::string label = "unnamed bind group layout"
        ) -> wgpu::BindGroupLayout;
    };

}