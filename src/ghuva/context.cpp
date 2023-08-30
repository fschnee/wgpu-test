// TODO: reorganize the init_* function internals.
// TODO: make work in wasm.
// TODO: unlock fps.
#include "context.hpp"

#include "utils/chrono.hpp"
#include "utils/cvt.hpp"
#include "utils/m4.hpp"

#include <glfw3webgpu.h>

#include <backends/imgui_impl_wgpu.h>
#include <backends/imgui_impl_glfw.h>

#include <stdio.h>

#ifdef __EMSCRIPTEN__
    #define IS_NATIVE 0
#else
    #define IS_NATIVE 1
#endif

#include <fmt/core.h>

using namespace ghuva::aliases;
namespace cvt = ghuva::cvt;

auto ghuva::context::get() -> context&
{
    static auto ctx = context{};
    return ctx;
}

ghuva::context::~context()
{
    if(window)          glfwDestroyWindow(window);
    if(init_successful) glfwTerminate();
    if(imgui_init_successful)
    {
        ImGui_ImplWGPU_Shutdown();
        ImGui_ImplGlfw_Shutdown();
    }
}

auto ghuva::context::init_all() -> context&
{
    this->init_glfw();
    this->init_instance();
    this->init_surface();
    this->init_device();
    this->init_swapchain();
    this->init_imgui();
    this->init_buffers();
    this->init_bindings();
    this->init_render_pipeline();
    this->init_compute_pipeline();
    this->init_textures();

    return *this;
}

auto ghuva::context::imgui_new_frame() -> void
{
    this->frame++;

    if(new_resolution)
    {
        ImGui_ImplWGPU_CreateDeviceObjects();
        this->new_resolution = false;
    }

    ImGui_ImplWGPU_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

auto ghuva::context::imgui_render(WGPURenderPassEncoder pass) -> void
{
    ImGui::Render();
    ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), pass);
}

auto ghuva::context::init_glfw() -> void
{
    std::cout << "[glfw] Initializing glfw... " << std::flush;

    if (!glfwInit()) throw context_error::failed_to_init_glfw;
    this->init_successful = true;

    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // We'll use webgpu and not whatever it wants to try to set default.
    auto window = glfwCreateWindow(this->w, this->h, "wgpu-test", nullptr, nullptr);
    this->window = window;
    if(!window) throw context_error::failed_to_open_window;

    std::cout << "OK" << std::endl;
}

auto ghuva::context::init_instance() -> void
{
    std::cout << "[wgpu] Requesting instance..." << std::endl;

    if constexpr(IS_NATIVE) { this->instance = wgpu::createInstance({}); }
    else                    { this->instance = wgpu::Instance{nullptr}; }

    std::cout << "\t" << this->instance << std::endl;
    //DONT_FORGET(this->instance.drop());
}

auto ghuva::context::init_surface() -> void
{
    std::cout << "[wgpu][glfw] Creating surface..." << std::endl;

    if constexpr(IS_NATIVE) { this->surface = glfwGetWGPUSurface(this->instance, this->window); }
    else                    { this->surface = {nullptr}; }

    std::cout << "\t" << this->surface << std::endl;
}

auto ghuva::context::init_device() -> void
{
    this->desc.adapter = {
        .nextInChain = nullptr,
        .compatibleSurface = this->surface,
        .powerPreference = wgpu::PowerPreference::HighPerformance,
        .forceFallbackAdapter = false
    };
    std::cout << "[wgpu] Requesting adapter." << std::endl;
    this->adapter = this->instance.requestAdapter(this->desc.adapter);
    std::cout << "\t" << adapter << std::endl;
    if(!this->adapter) throw context_error::failed_to_create_adapter;
    //DONT_FORGET(this->adapter.drop());

    std::cout << "[wgpu] Fetching adapter limits... " << std::flush;
    this->adapter.getLimits(&this->limits.adapter);
    std::cout << "OK" << std::endl;

    this->desc.device_limits = {
        .nextInChain = nullptr,
        .limits = this->limits.adapter.limits, // Request max supported of adapter for everything.
    };

    WGPURequiredLimits* device_limits = desc.device_limits ? &desc.device_limits.value() : nullptr;
    this->desc.device = {
        .nextInChain = nullptr,
        .label = "wgpu-test-device",
        .requiredFeaturesCount = 0,
        .requiredFeatures = nullptr,
        .requiredLimits = device_limits,
        .defaultQueue = { .nextInChain = nullptr, .label = "wgpu-test-default-queue" }
    };
    std::cout << "[wgpu] Requesting device..." << std::endl;
    this->device = this->adapter.requestDevice(this->desc.device);
    std::cout << "\t" << this->device << std::endl;

    wgpuDeviceSetUncapturedErrorCallback(device, [](WGPUErrorType type, char const * message, void * userdata) {
        [[maybe_unused]] auto& ctx = *(cvt::rc<context*> + userdata);
        std::cout <<  "Uncaptured device error: type " << (type * cvt::to<u64>) << ": " << message << std::endl;
    }, this);

    // DONT_FORGET(this->device.drop());
    std::cout << "[wgpu] Fetching device limits... " << std::flush;
    this->device.getLimits(&this->limits.device);
    std::cout << "OK" << std::endl;
}

auto ghuva::context::init_swapchain() -> void
{
    this->desc.swapchain_format = this->surface.getPreferredFormat(this->adapter);
    this->desc.swapchain = {
        .nextInChain = nullptr,
        .label = "wgpu-test-swapchain",
        .usage = wgpu::TextureUsage::RenderAttachment,
        .format = this->desc.swapchain_format,
        .width = this->w,
        .height = this->h,
        .presentMode = wgpu::PresentMode::Fifo,
    };
    std::cout << "[wgpu] Creating swapchain..." << std::endl;
    this->swapchain = this->device.createSwapChain(this->surface, this->desc.swapchain);
    std::cout << "\t" << this->swapchain << std::endl;
    //DONT_FORGET(this->swapchain.drop());

    this->desc.depth_stencil_format = wgpu::TextureFormat::Depth24Plus;
}

auto ghuva::context::init_imgui() -> void
{
    std::cout << "[imgui] Initializing imgui... " << std::flush;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.IniFilename = nullptr;
    ImGui::StyleColorsDark();

    this->imgui_init_successful =
        ImGui_ImplGlfw_InitForOther(window, true);
    this->imgui_init_successful =
        this->imgui_init_successful &&
        ImGui_ImplWGPU_Init(this->device, 3, this->desc.swapchain_format, this->desc.depth_stencil_format);

    std::cout << (this->imgui_init_successful ? "OK" : "ERROR") << std::endl;
    if(!this->imgui_init_successful) throw context_error::failed_to_init_imgui;
}

auto ghuva::context::init_bindings() -> void
{
    std::cout << "[wgpu] Creating uniform bind group 0 (scene) layout ..." << std::endl;
    this->bind_group_layouts[0] = this->create_bind_group_layout({{
        .nextInChain = nullptr,
        .binding = 0,
        .visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment,
        .buffer = {
            .nextInChain = nullptr,
            .type = wgpu::BufferBindingType::Uniform,
            .hasDynamicOffset = false,
            .minBindingSize = sizeof(scene_uniforms)
        },
        .sampler        = {},
        .texture        = {},
        .storageTexture = {},
    }});
    std::cout << "\t" << this->bind_group_layouts[0] << std::endl;

    //std::cout << "[wgpu] Creating uniform bind group 1 (object) layout ..." << std::endl;
    //this->bind_group_layouts[1] = this->create_bind_group_layout({{{
    //    .nextInChain = nullptr,
    //    .binding = 0,
    //    .visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment,
    //    .buffer = {
    //        .nextInChain = nullptr,
    //        .type = wgpu::BufferBindingType::Uniform,
    //        .hasDynamicOffset = true,
    //        .minBindingSize = sizeof(object_uniforms),
    //    },
    //    .sampler        = {},
    //    .texture        = {},
    //    .storageTexture = {},
    //}}});
    //std::cout << "\t" << this->bind_group_layouts[1] << std::endl;

     // Uniform.
	this->desc.bindings[0] = {
        .nextInChain = nullptr,
	    .binding = 0,
	    .buffer = this->scene_uniform_buffer,
    	.offset = 0,
    	.size = this->desc.scene_uniform_buffer.size,
        .sampler = nullptr,
        .textureView = nullptr,
    };

    //this->desc.bindings[1] = {
    //    .nextInChain = nullptr,
    //    .binding = 0,
    //    .buffer = this->object_uniform_buffer,
    //    .offset = 0,
    //    .size = sizeof(object_uniforms),
    //    .sampler = nullptr,
    //    .textureView = nullptr,
    //};

    this->desc.scene_bind_group_descriptor = {
        .nextInChain = nullptr,
        .label = "Scene bind group",
        .layout = this->bind_group_layouts[0],
        .entryCount = 1,
        .entries = this->desc.bindings
    };
    std::cout << "[wgpu] Creating scene Bind Group..." << std::endl;
    this->scene_bind_group = device.createBindGroup(this->desc.scene_bind_group_descriptor);
    std::cout << "\t" << this->scene_bind_group << std::endl;

    //this->desc.object_bind_group_descriptor = {
    //    .nextInChain = nullptr,
    //    .label = "Object bind group",
    //    .layout = this->bind_group_layouts[1],
    //    .entryCount = 1,
    //    .entries = &this->desc.bindings[1]
    //};
    //std::cout << "[wgpu] Creating object Bind Group..." << std::endl;
    //this->object_bind_group = device.createBindGroup(this->desc.object_bind_group_descriptor);
    //std::cout << "\t" << this->object_bind_group << std::endl;

    // Compute bindings below.
    this->desc.bindings[2] = {
        .nextInChain = nullptr,
        .binding = 0,
        .buffer = this->object_uniform_buffer,
        .offset = 0,
        .size = this->object_uniform_limit * sizeof(compute_object_uniforms),
        .sampler = nullptr,
        .textureView = nullptr,
    };

    std::cout << "[wgpu] Creating compute bind group layout ..." << std::endl;
    this->bind_group_layouts[2] = this->create_bind_group_layout({{{
        .nextInChain = nullptr,
        .binding = 0,
        .visibility = wgpu::ShaderStage::Compute,
        .buffer = {
            .nextInChain = nullptr,
            .type = wgpu::BufferBindingType::Storage,
            .hasDynamicOffset = false,
            .minBindingSize = sizeof(compute_object_uniforms),
        },
        .sampler        = {},
        .texture        = {},
        .storageTexture = {},
    }}});
    std::cout << "\t" << this->bind_group_layouts[2] << std::endl;
    wgpu::BindGroupDescriptor bindGroupDesc;
    bindGroupDesc.layout = this->bind_group_layouts[2];
    bindGroupDesc.entryCount = 1;
    bindGroupDesc.entries = &this->desc.bindings[2];
    this->compute_bind_group = this->device.createBindGroup(bindGroupDesc);
}

auto ghuva::context::init_render_pipeline() -> void
{
    // TODO: pass code via param ?
    this->shader = this->create_wgsl_shader(
        #include "default_shader.hpp.inc"
    );

    this->desc.pipeline_layout = {
        .nextInChain = nullptr,
        .label = "Pipeline layout",
        .bindGroupLayoutCount = 1,
        .bindGroupLayouts = (WGPUBindGroupLayout*)this->bind_group_layouts
    };
    std::cout << "[wgpu] Creating pipeline layout..." << std::endl;
    this->pipeline_layout = device.createPipelineLayout(this->desc.pipeline_layout);
    std::cout << "\t" << this->pipeline_layout << std::endl;

    this->desc.vertex_buffer_attributes.push_back({
        .format = wgpu::VertexFormat::Float32x3, // xyz.
        .offset = 0,
        .shaderLocation = 0, // @location(0).
    });
    this->desc.vertex_buffer_attributes.push_back({
        .format = wgpu::VertexFormat::Float32x3, // rgb.
        .offset = 0,
        .shaderLocation = 1, // @location(1).
    });
    this->desc.vertex_buffer_attributes.push_back({
        .format = wgpu::VertexFormat::Float32x3,  // nx,ny,z
        .offset = 0,
        .shaderLocation = 2, // @location(2)
    });
    this->desc.vertex_buffer_attributes.push_back({
        .format = wgpu::VertexFormat::Float32x4,
        .offset = 0,
        .shaderLocation = 3,
    });
    this->desc.vertex_buffer_attributes.push_back({
        .format = wgpu::VertexFormat::Float32x4,
        .offset = 4 * sizeof(f32),
        .shaderLocation = 4,
    });
    this->desc.vertex_buffer_attributes.push_back({
        .format = wgpu::VertexFormat::Float32x4,
        .offset = 8 * sizeof(f32),
        .shaderLocation = 5,
    });
    this->desc.vertex_buffer_attributes.push_back({
        .format = wgpu::VertexFormat::Float32x4,
        .offset = 12 * sizeof(f32),
        .shaderLocation = 6,
    });
    this->desc.vertex_buffer_layouts.push_back({
        .arrayStride = 3 * sizeof(context::vertex_t), // xyz
        .stepMode = wgpu::VertexStepMode::Vertex,
        .attributeCount = 1,
        .attributes = &this->desc.vertex_buffer_attributes[0],
    });
    this->desc.vertex_buffer_layouts.push_back({
        .arrayStride = 3 * sizeof(context::vertex_t), // rgb
        .stepMode = wgpu::VertexStepMode::Vertex,
        .attributeCount = 1,
        .attributes = &this->desc.vertex_buffer_attributes[1],
    });
    this->desc.vertex_buffer_layouts.push_back({
        .arrayStride = 3 * sizeof(context::vertex_t), // nx, ny, nz
        .stepMode = wgpu::VertexStepMode::Vertex,
        .attributeCount = 1,
        .attributes = &this->desc.vertex_buffer_attributes[2]
    });
    this->desc.vertex_buffer_layouts.push_back({
        .arrayStride = sizeof(context::object_uniforms),
        .stepMode = wgpu::VertexStepMode::Instance,
        .attributeCount = 4,
        .attributes = &this->desc.vertex_buffer_attributes[3]
    });

    auto depth_stencil_state = wgpu::DepthStencilState{};
    depth_stencil_state.setDefault();
    depth_stencil_state.format = this->desc.depth_stencil_format;
    depth_stencil_state.depthWriteEnabled = true;
    depth_stencil_state.depthCompare = wgpu::CompareFunction::Less;
    depth_stencil_state.stencilReadMask = 0;
    depth_stencil_state.stencilWriteMask = 0;

    auto blend_state = wgpu::BlendState{};
    blend_state.color.operation = wgpu::BlendOperation::Add;
    blend_state.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
    blend_state.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
    blend_state.alpha.operation = wgpu::BlendOperation::Add;
    blend_state.alpha.srcFactor = wgpu::BlendFactor::Zero;
    blend_state.alpha.dstFactor = wgpu::BlendFactor::One;

    auto color_target = wgpu::ColorTargetState{};
    color_target.nextInChain = nullptr;
    color_target.format = this->desc.swapchain_format;
    color_target.blend = &blend_state;
    color_target.writeMask = wgpu::ColorWriteMask::All;

    auto fragment_state = wgpu::FragmentState{};
    fragment_state.nextInChain = nullptr;
    fragment_state.module = shader;
    fragment_state.entryPoint = "frag";
    fragment_state.constantCount = 0;
    fragment_state.constants = nullptr;
    fragment_state.targetCount = 1;
    fragment_state.targets = &color_target;

    auto pipeline_desc = wgpu::RenderPipelineDescriptor{};
    pipeline_desc.layout = this->pipeline_layout;
    pipeline_desc.vertex = {
        .nextInChain = nullptr,
        .module = this->shader,
        .entryPoint = "vert",
        .constantCount = 0,
        .constants = nullptr,
        .bufferCount = cvt::toe * this->desc.vertex_buffer_layouts.size(),
        .buffers = this->desc.vertex_buffer_layouts.data(),
    };
    pipeline_desc.primitive = {
        .nextInChain = nullptr,
        .topology = wgpu::PrimitiveTopology::TriangleList,
        .stripIndexFormat = wgpu::IndexFormat::Undefined,
        .frontFace = wgpu::FrontFace::CCW,
        .cullMode = wgpu::CullMode::None,
    };
    pipeline_desc.depthStencil = &depth_stencil_state,
    pipeline_desc.multisample = {
        .nextInChain = nullptr,
        .count = 1,
        .mask = ~0u,
        .alphaToCoverageEnabled = false,
    };
    pipeline_desc.fragment = &fragment_state;
    std::cout << "[wgpu] Creating render pipeline..." << std::endl;
    this->pipeline = this->device.createRenderPipeline(pipeline_desc);
    std::cout << "\t" << this->pipeline << std::endl;
}

auto ghuva::context::init_compute_pipeline() -> void
{
    this->compute_shader = this->create_wgsl_shader(
        #include "default_compute_shader.hpp.inc"
        , "Compute shader"
    );

    std::cout << "[wgpu] Creating compute bind group layout ..." << std::endl;
    this->bind_group_layouts[2] = this->create_bind_group_layout({{{
        .nextInChain = nullptr,
        .binding = 0,
        .visibility = wgpu::ShaderStage::Compute,
        .buffer = {
            .nextInChain = nullptr,
            .type = wgpu::BufferBindingType::Storage,
            .hasDynamicOffset = false,
            .minBindingSize = sizeof(object_uniforms),
        },
        .sampler        = {},
        .texture        = {},
        .storageTexture = {},
    }}});
    std::cout << "\t" << this->bind_group_layouts[2] << std::endl;

    this->desc.compute_pipeline_layout = {
        .nextInChain = nullptr,
        .label = "Compute pipeline layout",
        .bindGroupLayoutCount = 1,
        .bindGroupLayouts = (WGPUBindGroupLayout*)&this->bind_group_layouts[2]
    };
    this->compute_pipeline_layout = this->device.createPipelineLayout(this->desc.compute_pipeline_layout);

    std::cout << "[wgpu] Creating compute pipeline..." << std::endl;
    auto temp1 = wgpu::ComputePipelineDescriptor{};
    temp1.setDefault();
    this->desc.compute_pipeline = temp1;
    this->desc.compute_pipeline.layout = this->compute_pipeline_layout;
    this->desc.compute_pipeline.compute.entryPoint = "compute";
    this->desc.compute_pipeline.compute.module = this->compute_shader;
    this->compute_pipeline = this->device.createComputePipeline(this->desc.compute_pipeline);
    std::cout << "\t" << this->compute_pipeline << std::endl;
}

auto ghuva::context::init_buffers() -> void
{
    /**
     * Round 'value' up to the next multiplier of 'step'.
        */
    constexpr auto ceil_to_next_multiple = [](u32 value, u32 step) -> u32 {
        uint32_t divide_and_ceil = value / step + (value % step == 0 ? 0 : 1);
        return step * divide_and_ceil;
    };
    this->object_uniform_stride = ceil_to_next_multiple(
        sizeof(ghuva::context::object_uniforms),
        this->limits.device.limits.minUniformBufferOffsetAlignment
    );
    this->compute_uniform_stride = ceil_to_next_multiple(
        sizeof(ghuva::context::compute_object_uniforms),
        this->limits.device.limits.minStorageBufferOffsetAlignment
    );
    assert(this->object_uniform_stride == this->compute_uniform_stride);
    assert(this->object_uniform_stride == sizeof(ghuva::context::compute_object_uniforms));
    assert(this->object_uniform_stride == sizeof(ghuva::context::object_uniforms));

    std::cout << "[wgpu] Creating vertex buffer..." << std::endl;
    this->desc.vertex_buffer = {
        .nextInChain = nullptr,
        .label = "Vertex (position) buffer",
        .usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex,
        .size  = this->vertex_count * 3 * sizeof(vertex_t),
        .mappedAtCreation = false,
    };
    this->vertex_buffer = device.createBuffer(this->desc.vertex_buffer);
    std::cout << "\t" << this->vertex_buffer << std::endl;

    std::cout << "[wgpu] Creating color buffer..." << std::endl;
    this->desc.color_buffer = {
        .nextInChain = nullptr,
        .label = "Vertex (color) buffer",
        .usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex,
        .size  = this->desc.vertex_buffer.size,
        .mappedAtCreation = false,
    };
    this->color_buffer = device.createBuffer(this->desc.color_buffer);
    std::cout << "\t" << this->color_buffer << std::endl;

    std::cout << "[wgpu] Creating normal buffer..." << std::endl;
    this->desc.normal_buffer = {
        .nextInChain = nullptr,
        .label = "Vertex (normal) buffer",
        .usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex,
        .size  = this->desc.vertex_buffer.size,
        .mappedAtCreation = false,
    };
    this->normal_buffer = device.createBuffer(this->desc.normal_buffer);
    std::cout << "\t" << this->normal_buffer << std::endl;

    std::cout << "[wgpu] Creating index buffer..." << std::endl;
    this->desc.index_buffer = {
        .nextInChain = nullptr,
        .label = "Index buffer",
        .usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Index,
        .size = this->index_count * sizeof(index_t),
        .mappedAtCreation = false,
    };
    this->index_buffer = device.createBuffer(this->desc.index_buffer);
    std::cout << "\t" << this->index_buffer << std::endl;

    std::cout << "[wgpu] Creating scene uniform buffer..." << std::endl;
	this->desc.scene_uniform_buffer = {
        .nextInChain = nullptr,
        .label = "Scene uniform buffer",
        .usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform,
        .size = sizeof(ghuva::context::scene_uniforms),
        .mappedAtCreation = false,
    };
    this->scene_uniform_buffer = device.createBuffer(this->desc.scene_uniform_buffer);
    std::cout << "\t" << this->scene_uniform_buffer << std::endl;

    std::cout << "[wgpu] Creating object vertex buffer..." << std::endl;
	this->desc.object_uniform_buffer = {
        .nextInChain = nullptr,
        .label = "Object uniform buffer",
        .usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex | wgpu::BufferUsage::Storage,
        .size = this->object_uniform_limit * sizeof(object_uniforms),
        .mappedAtCreation = false,
    };
    this->object_uniform_buffer = device.createBuffer(this->desc.object_uniform_buffer);
    std::cout << "\t" << this->object_uniform_buffer << std::endl;
}

auto ghuva::context::init_textures() -> void
{
    std::cout << "[wgpu] Creating Depth Texture..." << std::endl;
    this->desc.depth_texture = {
        .nextInChain = nullptr,
        .label = "Depth texture",
        .usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding,
        .dimension = wgpu::TextureDimension::_2D,
        .size = {this->w, this->h, 1},
        .format = this->desc.depth_stencil_format,
        .mipLevelCount = 1,
        .sampleCount = 1,
        .viewFormatCount = 1,
        .viewFormats = (WGPUTextureFormat*)&this->desc.depth_stencil_format,
    };
    this->depth_texture = device.createTexture(this->desc.depth_texture);
    std::cout << "\t" << this->depth_texture << std::endl;
    this->desc.depth_texture_view = {
        .nextInChain = nullptr,
        .label = "Depth texture view",
        .format = this->desc.depth_stencil_format,
        .dimension = wgpu::TextureViewDimension::_2D,
        .baseMipLevel = 0,
        .mipLevelCount = 1,
        .baseArrayLayer = 0,
        .arrayLayerCount = 1,
        .aspect = wgpu::TextureAspect::DepthOnly,
    };
    this->depth_texture_view = this->depth_texture.createView(this->desc.depth_texture_view);
    std::cout << "\t" << this->depth_texture_view << std::endl;
}

auto ghuva::context::set_resolution(ghuva::u32 nw, ghuva::u32 nh) -> context&
{
    this->new_resolution = true;
    ImGui_ImplWGPU_InvalidateDeviceObjects();

    glfwSetWindowSize(this->window, nw, nh);

    //this->depth_texture_view.release();
    this->depth_texture.destroy();
    //this->depth_texture.release();

    this->w = nw;
    this->h = nh;

    std::cout << "[wgpu] RESOLUTION CHANGE => Recreating depth texture..." << std::endl;
    this->desc.depth_texture.size = {this->w, this->h, 1};
    this->depth_texture = device.createTexture(this->desc.depth_texture);
    std::cout << "\t" << this->depth_texture << std::endl;
    this->depth_texture_view = this->depth_texture.createView(this->desc.depth_texture_view);
    std::cout << "\t" << this->depth_texture_view << std::endl;

    this->swapchain.drop();
    this->swapchain = this->device.createSwapChain(this->surface, {{
        .nextInChain = nullptr,
        .label = "wgpu-test-swapchain",
        .usage = wgpu::TextureUsage::RenderAttachment,
        .format = this->surface.getPreferredFormat(this->adapter),
        .width = w,
        .height = h,
        .presentMode = wgpu::PresentMode::Fifo,
    }});

    return *this;
}

auto ghuva::context::loop(void* userdata, loop_callback fn) -> void
{
    auto stopwatch = ghuva::chrono::stopwatch{};

    #ifndef __EMSCRIPTEN__
        while (!glfwWindowShouldClose(window))
        {
            auto dt = stopwatch.click().last_segment();
            glfwPollEvents();
            if(fn(*this, dt, userdata) == loop_message::do_break) break;
        }
    #else
        while(true) fn(); // TODO: implement loop.
    #endif
}

auto ghuva::context::begin_compute() -> wgpu::ComputePassEncoder
{
    // Initialize a command encoder
    auto temp = wgpu::CommandEncoderDescriptor{};
    temp.setDefault();
    this->desc.compute_encoder_descriptor = temp;
    this->compute_encoder = this->device.createCommandEncoder(this->desc.compute_encoder_descriptor);

    this->desc.compute_pass.timestampWriteCount = 0;
    this->desc.compute_pass.timestampWrites = nullptr;
    return this->compute_encoder.beginComputePass(this->desc.compute_pass);
}

auto ghuva::context::end_compute(wgpu::ComputePassEncoder compute_pass) -> void
{
    compute_pass.end();
    auto commands = this->compute_encoder.finish({});
    this->device.getQueue().submit(commands);
}

auto ghuva::context::begin_render(wgpu::TextureView render_view) -> wgpu::RenderPassEncoder
{
    this->render_view = render_view;
    this->encoder = this->device.createCommandEncoder({{ .nextInChain = nullptr, .label = "Command Encoder" }});
    this->color_attachment = wgpu::RenderPassColorAttachment{{
        .view = render_view,
        .resolveTarget = nullptr,
        .loadOp = WGPULoadOp_Clear,
        .storeOp = WGPUStoreOp_Store,
        .clearValue = WGPUColor{ 0.05, 0.05, 0.05, 1.0 }
    }};
    this->depth_attachment = wgpu::RenderPassDepthStencilAttachment({
        .view = this->depth_texture_view,
        // Operation settings comparable to the color attachment
        .depthLoadOp = wgpu::LoadOp::Clear,
        .depthStoreOp = wgpu::StoreOp::Store,
        // The initial value of the depth buffer, meaning "far"
        .depthClearValue = 1.0f,
        // we could turn off writing to the depth buffer globally here
        .depthReadOnly = false,
        // Stencil setup, mandatory but unused
        .stencilLoadOp = wgpu::LoadOp::Clear,
        .stencilStoreOp = wgpu::StoreOp::Store,
        .stencilClearValue = 0,
        .stencilReadOnly = true,
    });

    return encoder.beginRenderPass({{
        .nextInChain = nullptr,
        .label = "wgpu-test-render-pass",
        .colorAttachmentCount = 1,
        .colorAttachments = &this->color_attachment,
        .depthStencilAttachment = &this->depth_attachment,
        .occlusionQuerySet = nullptr,
        .timestampWriteCount = 0,
        .timestampWrites = nullptr
    }});
}

auto ghuva::context::end_render(wgpu::RenderPassEncoder render_pass) -> void
{
    this->imgui_render(render_pass);
    render_pass.end();

    auto command = this->encoder.finish({{
        .nextInChain = nullptr,
        .label = "wgpu-test-command-buffer"
    }});
    this->device.getQueue().submit(command);

    this->swapchain.present();
}

auto ghuva::context::create_wgsl_shader(std::string code, std::string label) -> wgpu::ShaderModule
{
    auto shader_wgsl  = wgpu::ShaderModuleWGSLDescriptor{};
    shader_wgsl.chain = {.next = nullptr, .sType = wgpu::SType::ShaderModuleWGSLDescriptor};
    shader_wgsl.code  = code.c_str();

    auto shader_descriptor = wgpu::ShaderModuleDescriptor{};
    shader_descriptor.nextInChain = &shader_wgsl.chain;
    shader_descriptor.label = label.c_str();
    shader_descriptor.hintCount = 0;
    shader_descriptor.hints = nullptr;

    std::cout << "[wgpu] Creating shader module... label = " << label << std::endl;
    auto shader = this->device.createShaderModule(shader_descriptor);
    std::cout << "\t" << shader << std::endl;
    return shader;
}

auto ghuva::context::create_bind_group_layout(std::vector<WGPUBindGroupLayoutEntry> entries, std::string label) -> wgpu::BindGroupLayout
{
    return device.createBindGroupLayout({{
        .nextInChain = nullptr,
        .label = label.c_str(),
        .entryCount = cvt::toe * entries.size(),
        .entries = entries.data()
    }});
}
