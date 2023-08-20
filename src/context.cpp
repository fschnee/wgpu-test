#include "context.hpp"

#include "standalone/chrono.hpp"
#include "standalone/cvt.hpp"

#include <glfw3webgpu.h>

#include <backends/imgui_impl_wgpu.h>
#include <backends/imgui_impl_glfw.h>

#include "m.hpp"

#include <stdio.h>

#ifdef __EMSCRIPTEN__
    #define IS_NATIVE 0
#else
    #define IS_NATIVE 1
#endif

#include <fmt/core.h>

auto context::get() -> context&
{
    static auto ctx = context{};
    return ctx;
}

context::~context()
{
    if(window)          glfwDestroyWindow(window);
    if(init_successful) glfwTerminate();
    if(imgui_init_successful)
    {
        ImGui_ImplWGPU_Shutdown();
        ImGui_ImplGlfw_Shutdown();
    }
}

auto context::init_all() -> context&
{
    namespace cvt = standalone::cvt;
    using namespace standalone::integer_aliases;

    std::cout << "[glfw] Initializing glfw... " << std::flush;
    this->init_glfw();
    if     (!this->init_successful) throw context_error::failed_to_init_glfw;
    else if(!this->window)          throw context_error::failed_to_open_window;
    std::cout << "OK" << std::endl;

    std::cout << "[wgpu] Requesting instance..." << std::endl;
    if constexpr(IS_NATIVE) { this->init_instance({{.nextInChain = nullptr}}); }
    else                    { this->init_instance(std::nullopt); }
    std::cout << "\t" << this->instance << std::endl;
    //DONT_FORGET(this->instance.drop());

    std::cout << "[wgpu][glfw] Creating surface..." << std::endl;
    if constexpr(IS_NATIVE) { this->init_surface(glfwGetWGPUSurface(this->instance, this->window)); }
    else                    { this->init_surface(); }
    std::cout << "\t" << this->surface << std::endl;

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
        .limits = {
            // Max of laptop used for testing.
            .maxTextureDimension1D = 8192,
            .maxTextureDimension2D = 8192,
            .maxTextureDimension3D = 2048,
            .maxTextureArrayLayers = 256,
	        .maxBindGroups = 4,
            .maxDynamicUniformBuffersPerPipelineLayout = 8,
            .maxDynamicStorageBuffersPerPipelineLayout = 4,
            .maxSampledTexturesPerShaderStage = 16,
            .maxSamplersPerShaderStage = 16,
            .maxStorageBuffersPerShaderStage = 8,
            .maxStorageTexturesPerShaderStage = 4,
	        .maxUniformBuffersPerShaderStage = 12,
	        .maxUniformBufferBindingSize = 65536,
	        .maxStorageBufferBindingSize = 134217728,
            .minUniformBufferOffsetAlignment = 256,
	        .minStorageBufferOffsetAlignment = 256,
	        .maxVertexBuffers = 8,
	        .maxBufferSize = 1024*1024*256, // 256Mib.
            .maxVertexAttributes = 16,
	        .maxVertexBufferArrayStride = 2048,
	        .maxInterStageShaderComponents = 116
        }
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
        auto& ctx = *(cvt::rc<context*> + userdata);
        std::cout <<  "Uncaptured device error: type " << (type * cvt::to<u64>) << ": " << message << std::endl;
    }, this);

    // DONT_FORGET(this->device.drop());
    std::cout << "[wgpu] Fetching device limits... " << std::flush;
    this->device.getLimits(&this->limits.device);
    std::cout << "OK" << std::endl;

    auto swapchainformat = this->surface.getPreferredFormat(this->adapter);
    this->desc.swapchain = {
        .nextInChain = nullptr,
        .label = "wgpu-test-swapchain",
        .usage = wgpu::TextureUsage::RenderAttachment,
        .format = swapchainformat,
        .width = this->w,
        .height = this->h,
        .presentMode = wgpu::PresentMode::Fifo,
    };
    std::cout << "[wgpu] Creating swapchain..." << std::endl;
    this->swapchain = this->device.createSwapChain(this->surface, this->desc.swapchain);
    std::cout << "\t" << this->swapchain << std::endl;
    //DONT_FORGET(this->swapchain.drop());

    std::cout << "[imgui] Initializing imgui... " << std::flush;
    this->desc.depth_stencil_format = wgpu::TextureFormat::Depth24Plus;
    this->init_imgui(this->device, swapchainformat, this->desc.depth_stencil_format);
    std::cout << (this->imgui_init_successful ? "OK" : "ERROR") << std::endl;
    if(!this->imgui_init_successful) throw context_error::failed_to_init_imgui;

    this->desc.shader_wgsl = {
        .chain = {.next = nullptr, .sType = wgpu::SType::ShaderModuleWGSLDescriptor},
        .code = this->desc.shader_code.c_str()
    };

    this->desc.shader = WGPUShaderModuleDescriptor{
        .nextInChain = &this->desc.shader_wgsl.chain, // Need to fallback to wgpu-native since wgpu-cpp sets nextInChain to nullptr.
        .label = "wgpu-test-shader",
        .hintCount = 0,
        .hints = nullptr
    };

    std::cout << "[wgpu] Creating shader module..." << std::endl;
    this->shader = wgpu::ShaderModule{ wgpuDeviceCreateShaderModule(this->device, &this->desc.shader) };
    std::cout << "\t" << this->shader << std::endl;

    this->desc.blend_state = {
        .color = {
            .operation = wgpu::BlendOperation::Add,
            .srcFactor = wgpu::BlendFactor::SrcAlpha,
            .dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha
        },
        .alpha = {
            .operation = wgpu::BlendOperation::Add,
            .srcFactor = wgpu::BlendFactor::Zero,
            .dstFactor = wgpu::BlendFactor::One
        }
    };

    this->desc.color_target = {
        .nextInChain = nullptr,
        .format = swapchainformat,
        .blend = &this->desc.blend_state,
        .writeMask = wgpu::ColorWriteMask::All
    };

    this->desc.fragment_state = {
        .nextInChain = nullptr,
        .module = shader,
        .entryPoint = "frag",
        .constantCount = 0,
        .constants = nullptr,
        .targetCount = 1,
        .targets = &this->desc.color_target
    };

    // The binding index as used in the @binding attribute in the shader
    this->desc.binding_layouts[0] = {
        .binding = 0,
        // The stage that needs to access this resource
        .visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment,
        .buffer = {
            .type = wgpu::BufferBindingType::Uniform,
            .minBindingSize = sizeof(uniforms)
        }
    };

    // The new binding layout, for the sampler
    //this->desc.binding_layouts[1] = {
    //    .binding = 1,
    //    .visibility = wgpu::ShaderStage::Fragment,
    //    .sampler = { .type = wgpu::SamplerBindingType::Filtering }
    //};

    // Create a bind group layout
    this->desc.binding_descriptor = {
        .entryCount = 1,
        .entries = this->desc.binding_layouts
    };
    std::cout << "[wgpu] Creating uniform bind group " << this->desc.binding_layouts[0].binding << " layout ..." << std::endl;
    this->bind_group_layout = device.createBindGroupLayout(this->desc.binding_descriptor);
    std::cout << "\t" << this->bind_group_layout << std::endl;
    this->desc.pipeline_layout = {
        .bindGroupLayoutCount = 1,
        .bindGroupLayouts = (WGPUBindGroupLayout*)&this->bind_group_layout
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
    this->desc.vertex_buffer_layouts.push_back({
        .arrayStride = 3 * sizeof(float), // xyz
        .stepMode = wgpu::VertexStepMode::Vertex,
        .attributeCount = 1,
        .attributes = &this->desc.vertex_buffer_attributes[0],
    });
    this->desc.vertex_buffer_layouts.push_back({
        .arrayStride = 3 * sizeof(float), // rgb
        .stepMode = wgpu::VertexStepMode::Vertex,
        .attributeCount = 1,
        .attributes = &this->desc.vertex_buffer_attributes[1],
    });

    auto depth_stencil_state_helper = wgpu::DepthStencilState{};
    depth_stencil_state_helper.setDefault();
    depth_stencil_state_helper.format = this->desc.depth_stencil_format;
    depth_stencil_state_helper.depthWriteEnabled = true;
    depth_stencil_state_helper.depthCompare = wgpu::CompareFunction::Less;
    depth_stencil_state_helper.stencilReadMask = 0;
    depth_stencil_state_helper.stencilWriteMask = 0;
    this->desc.depth_stencil_state = depth_stencil_state_helper;

    this->desc.pipeline = {
        .nextInChain = nullptr,
        .label = "wgpu-test-pipeline",
        .layout = this->pipeline_layout,
        .vertex = {
            .nextInChain = nullptr,
            .module = this->shader,
            .entryPoint = "vert",
            .constantCount = 0,
            .constants = nullptr,
            .bufferCount = cvt::toe * this->desc.vertex_buffer_layouts.size(),
            .buffers = this->desc.vertex_buffer_layouts.data()
        },
        .primitive = {
            .nextInChain = nullptr,
            .topology = wgpu::PrimitiveTopology::TriangleList,
            .stripIndexFormat = wgpu::IndexFormat::Undefined,
            .frontFace = wgpu::FrontFace::CCW,
            .cullMode = wgpu::CullMode::None
        },
        .depthStencil = &this->desc.depth_stencil_state,
        .multisample = {
            .nextInChain = nullptr,
            .count = 1,
            .mask = ~0u,
            .alphaToCoverageEnabled = false
        },
        .fragment = &this->desc.fragment_state
    };
    std::cout << "[wgpu] Creating render pipeline..." << std::endl;
    this->pipeline = this->device.createRenderPipeline(this->desc.pipeline);
    std::cout << "\t" << this->pipeline << std::endl;

    std::cout << "[wgpu] Creating vertex buffer..." << std::endl;
    this->desc.vertex_buffer = {
        .usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex,
        .size  = 10000 * sizeof(float),
        .mappedAtCreation = false,
    };
    this->vertex_buffer = device.createBuffer(this->desc.vertex_buffer);
    std::cout << "\t" << this->vertex_buffer << std::endl;

    std::cout << "[wgpu] Creating color buffer..." << std::endl;
    this->desc.color_buffer = {
        .usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex,
        .size  = this->desc.vertex_buffer.size,
        .mappedAtCreation = false,
    };
    this->color_buffer = device.createBuffer(this->desc.color_buffer);
    std::cout << "\t" << this->color_buffer << std::endl;

    std::cout << "[wgpu] Creating index buffer..." << std::endl;
    this->desc.index_buffer = {
        .nextInChain = nullptr,
        .label = "Index buffer",
        .usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Index,
        .size = this->limits.device.limits.maxBufferSize
    };
    this->index_buffer = device.createBuffer(this->desc.index_buffer);
    std::cout << "\t" << this->index_buffer << std::endl;

    std::cout << "[wgpu] Creating uniform buffer..." << std::endl;
	this->desc.uniform_buffer = {
        .nextInChain = nullptr,
        .label = "uniform buffer",
        .usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform,
        .size = this->desc.device_limits->limits.maxUniformBufferBindingSize,
        .mappedAtCreation = false
    };
    this->uniform_buffer = device.createBuffer(this->desc.uniform_buffer);
    std::cout << "\t" << this->uniform_buffer << std::endl;

    // Uniform.
	this->desc.bindings[0] = {
	    .binding = 0,
	    .buffer = this->uniform_buffer,
    	.offset = 0,
    	.size = this->desc.device_limits->limits.maxUniformBufferBindingSize
    };

    //this->desc.sampler = {
	//    .addressModeU = wgpu::AddressMode::Repeat,
	//    .addressModeV = wgpu::AddressMode::Repeat,
	//    .addressModeW = wgpu::AddressMode::Repeat,
	//    .magFilter = wgpu::FilterMode::Linear,
	//    .minFilter = wgpu::FilterMode::Linear,
	//    .mipmapFilter = wgpu::MipmapFilterMode::Linear,
	//    .lodMinClamp = 0.0f,
	//    .lodMaxClamp = 32.0f,
	//    .compare = wgpu::CompareFunction::Undefined,
	//    .maxAnisotropy = 1
    //};
	//this->sampler = this->device.createSampler(this->desc.sampler);
    //this->desc.bindings[1] = {
    //    .binding = 1,
    //    .sampler = this->sampler
    //};

    this->desc.bind_group_descriptor = {
        .layout = this->bind_group_layout,
        .entryCount = this->desc.binding_descriptor.entryCount,
        .entries = this->desc.bindings
    };
    std::cout << "[wgpu] Creating Bind Group..." << std::endl;
    this->bind_group = device.createBindGroup(this->desc.bind_group_descriptor);
    std::cout << "\t" << this->bind_group << std::endl;

    std::cout << "[wgpu] Creating Depth Texture..." << std::endl;
    this->desc.depth_texture = {
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

    return *this;
}

auto context::imgui_new_frame() -> void
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

auto context::imgui_render(WGPURenderPassEncoder pass) -> void
{
    ImGui::Render();
    ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), pass);
}

auto context::init_glfw() -> context&
{
    if (!glfwInit()) return *this;
    this->init_successful = true;

    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // We'll use webgpu and not whatever it wants to try to set default.
    auto window = glfwCreateWindow(this->w, this->h, "wgpu-test", nullptr, nullptr);
    this->window = window;
    return *this;
}

auto context::init_imgui(WGPUDevice device, WGPUTextureFormat swapchainformat, WGPUTextureFormat depthtextureformat) -> context&
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.IniFilename = nullptr;
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOther(window, true);
    ImGui_ImplWGPU_Init(device, 3, swapchainformat, depthtextureformat);
    this->imgui_init_successful = true;
    return *this;
}

auto context::set_resolution(standalone::u32 nw, standalone::u32 nh) -> context&
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

auto context::loop(void* userdata, loop_callback fn) -> void
{
    auto stopwatch = standalone::chrono::stopwatch{};

    #ifndef __EMSCRIPTEN__
        while (!glfwWindowShouldClose(window))
        {
            auto dt = stopwatch.click().last_segment();
            glfwPollEvents();
            if(fn(dt, *this, userdata) == loop_message::do_break) break;
        }
    #else
        while(true) fn(); // TODO: implement loop.
    #endif
}

// ImGui Backend stuff below.

#include <backends/imgui_impl_glfw.cpp>
// Code below adapted from imgui/backends/imgui_impl_wgpu.cpp

// dear imgui: Renderer for WebGPU
// This needs to be used along with a Platform Binding (e.g. GLFW)
// (Please note that WebGPU is currently experimental, will not run on non-beta browsers, and may break.)

// Implemented features:
//  [X] Renderer: User texture binding. Use 'WGPUTextureView' as ImTextureID. Read the FAQ about ImTextureID!
//  [X] Renderer: Large meshes support (64k+ vertices) with 16-bit indices.

// You can use unmodified imgui_impl_* files in your project. See examples/ folder for examples of using this.
// Prefer including the entire imgui/ repository into your project (either as a copy or as a submodule), and only build the backends you need.
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs

// CHANGELOG
// (minor and older changes stripped away, please see git history for details)
//  2023-04-11: Align buffer sizes. Use WGSL shaders instead of precompiled SPIR-V.
//  2023-04-11: Reorganized backend to pull data from a single structure to facilitate usage with multiple-contexts (all g_XXXX access changed to bd->XXXX).
//  2023-01-25: Revert automatic pipeline layout generation (see https://github.com/gpuweb/gpuweb/issues/2470)
//  2022-11-24: Fixed validation error with default depth buffer settings.
//  2022-11-10: Fixed rendering when a depth buffer is enabled. Added 'WGPUTextureFormat depth_format' parameter to ImGui_ImplWGPU_Init().
//  2022-10-11: Using 'nullptr' instead of 'NULL' as per our switch to C++11.
//  2021-11-29: Passing explicit buffer sizes to wgpuRenderPassEncoderSetVertexBuffer()/wgpuRenderPassEncoderSetIndexBuffer().
//  2021-08-24: Fixed for latest specs.
//  2021-05-24: Add support for draw_data->FramebufferScale.
//  2021-05-19: Replaced direct access to ImDrawCmd::TextureId with a call to ImDrawCmd::GetTexID(). (will become a requirement)
//  2021-05-16: Update to latest WebGPU specs (compatible with Emscripten 2.0.20 and Chrome Canary 92).
//  2021-02-18: Change blending equation to preserve alpha in output buffer.
//  2021-01-28: Initial version.

#include "webgpu.hpp"

#include <imgui.h>
#include <limits.h>

// Dear ImGui prototypes from imgui_internal.h
extern ImGuiID ImHashData(const void* data_p, size_t data_size, ImU32 seed = 0);
#define MEMALIGN(_SIZE,_ALIGN)        (((_SIZE) + ((_ALIGN) - 1)) & ~((_ALIGN) - 1))    // Memory align (copied from IM_ALIGN() macro).

// WebGPU data
struct RenderResources
{
    WGPUTexture         FontTexture = nullptr;          // Font texture
    WGPUTextureView     FontTextureView = nullptr;      // Texture view for font texture
    WGPUSampler         Sampler = nullptr;              // Sampler for the font texture
    WGPUBuffer          Uniforms = nullptr;             // Shader uniforms
    WGPUBindGroup       CommonBindGroup = nullptr;      // Resources bind-group to bind the common resources to pipeline
    ImGuiStorage        ImageBindGroups;                // Resources bind-group to bind the font/image resources to pipeline (this is a key->value map)
    WGPUBindGroup       ImageBindGroup = nullptr;       // Default font-resource of Dear ImGui
    WGPUBindGroupLayout ImageBindGroupLayout = nullptr; // Cache layout used for the image bind group. Avoids allocating unnecessary JS objects when working with WebASM
};

struct FrameResources
{
    WGPUBuffer  IndexBuffer;
    WGPUBuffer  VertexBuffer;
    ImDrawIdx*  IndexBufferHost;
    ImDrawVert* VertexBufferHost;
    int         IndexBufferSize;
    int         VertexBufferSize;
};

struct Uniforms
{
    float MVP[4][4];
    float Gamma;
};

struct ImGui_ImplWGPU_Data
{
    WGPUDevice          wgpuDevice = nullptr;
    WGPUQueue           defaultQueue = nullptr;
    WGPUTextureFormat   renderTargetFormat = WGPUTextureFormat_Undefined;
    WGPUTextureFormat   depthStencilFormat = WGPUTextureFormat_Undefined;
    WGPURenderPipeline  pipelineState = nullptr;

    RenderResources     renderResources;
    FrameResources*     pFrameResources = nullptr;
    unsigned int        numFramesInFlight = 0;
    unsigned int        frameIndex = UINT_MAX;
};

// Backend data stored in io.BackendRendererUserData to allow support for multiple Dear ImGui contexts
// It is STRONGLY preferred that you use docking branch with multi-viewports (== single Dear ImGui context + multiple windows) instead of multiple Dear ImGui contexts.
static ImGui_ImplWGPU_Data* ImGui_ImplWGPU_GetBackendData()
{
    return ImGui::GetCurrentContext() ? (ImGui_ImplWGPU_Data*)ImGui::GetIO().BackendRendererUserData : nullptr;
}

//-----------------------------------------------------------------------------
// SHADERS
//-----------------------------------------------------------------------------

static const char __shader_vert_wgsl[] = R"(
struct VertexInput {
    @location(0) position: vec2<f32>,
    @location(1) uv: vec2<f32>,
    @location(2) color: vec4<f32>,
};

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) color: vec4<f32>,
    @location(1) uv: vec2<f32>,
};

struct Uniforms {
    mvp: mat4x4<f32>,
    gamma: f32,
};

@group(0) @binding(0) var<uniform> uniforms: Uniforms;

@vertex
fn main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    out.position = uniforms.mvp * vec4<f32>(in.position, 0.0, 1.0);
    out.color = in.color;
    out.uv = in.uv;
    return out;
}
)";

static const char __shader_frag_wgsl[] = R"(
struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) color: vec4<f32>,
    @location(1) uv: vec2<f32>,
};

struct Uniforms {
    mvp: mat4x4<f32>,
    gamma: f32,
};

@group(0) @binding(0) var<uniform> uniforms: Uniforms;
@group(0) @binding(1) var s: sampler;
@group(1) @binding(0) var t: texture_2d<f32>;

@fragment
fn main(in: VertexOutput) -> @location(0) vec4<f32> {
    let color = in.color * textureSample(t, s, in.uv);
    let corrected_color = pow(color.rgb, vec3<f32>(uniforms.gamma));
    return vec4<f32>(corrected_color, color.a);
}
)";

static void SafeRelease(ImDrawIdx*& res)
{
    if (res)
        delete[] res;
    res = nullptr;
}
static void SafeRelease(ImDrawVert*& res)
{
    if (res)
        delete[] res;
    res = nullptr;
}
static void SafeRelease(WGPUBindGroupLayout& res)
{
    if (res)
        wgpuBindGroupLayoutDrop(res); //wgpuBindGroupLayoutRelease(res);
    res = nullptr;
}
static void SafeRelease(WGPUBindGroup& res)
{
    if (res)
        wgpuBindGroupDrop(res); //wgpuBindGroupRelease(res);
    res = nullptr;
}
static void SafeRelease(WGPUBuffer& res)
{
    if (res)
        wgpuBufferDrop(res); // wgpuBufferRelease(res);
    res = nullptr;
}
static void SafeRelease(WGPURenderPipeline& res)
{
    if (res)
        wgpuRenderPipelineDrop(res); //wgpuRenderPipelineRelease(res);
    res = nullptr;
}
static void SafeRelease(WGPUSampler& res)
{
    if (res)
        wgpuSamplerDrop(res); //wgpuSamplerRelease(res);
    res = nullptr;
}
static void SafeRelease(WGPUShaderModule& res)
{
    if (res)
        wgpuShaderModuleDrop(res); //wgpuShaderModuleRelease(res);
    res = nullptr;
}
static void SafeRelease(WGPUTextureView& res)
{
    if (res)
        wgpuTextureViewDrop(res); //wgpuTextureViewRelease(res);
    res = nullptr;
}
static void SafeRelease(WGPUTexture& res)
{
    if (res)
        wgpuTextureDrop(res); //wgpuTextureRelease(res);
    res = nullptr;
}

static void SafeRelease(RenderResources& res)
{
    SafeRelease(res.FontTexture);
    SafeRelease(res.FontTextureView);
    SafeRelease(res.Sampler);
    SafeRelease(res.Uniforms);
    SafeRelease(res.CommonBindGroup);
    SafeRelease(res.ImageBindGroup);
    SafeRelease(res.ImageBindGroupLayout);
};

static void SafeRelease(FrameResources& res)
{
    SafeRelease(res.IndexBuffer);
    SafeRelease(res.VertexBuffer);
    SafeRelease(res.IndexBufferHost);
    SafeRelease(res.VertexBufferHost);
}

static WGPUProgrammableStageDescriptor ImGui_ImplWGPU_CreateShaderModule(const char* wgsl_source)
{
    ImGui_ImplWGPU_Data* bd = ImGui_ImplWGPU_GetBackendData();

    WGPUShaderModuleWGSLDescriptor wgsl_desc = {};
    wgsl_desc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
    wgsl_desc.code = wgsl_source;

    WGPUShaderModuleDescriptor desc = {};
    desc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&wgsl_desc);

    WGPUProgrammableStageDescriptor stage_desc = {};
    stage_desc.module = wgpuDeviceCreateShaderModule(bd->wgpuDevice, &desc);
    stage_desc.entryPoint = "main";
    return stage_desc;
}

static WGPUBindGroup ImGui_ImplWGPU_CreateImageBindGroup(WGPUBindGroupLayout layout, WGPUTextureView texture)
{
    ImGui_ImplWGPU_Data* bd = ImGui_ImplWGPU_GetBackendData();
    WGPUBindGroupEntry image_bg_entries[] = { { nullptr, 0, 0, 0, 0, 0, texture } };

    WGPUBindGroupDescriptor image_bg_descriptor = {};
    image_bg_descriptor.layout = layout;
    image_bg_descriptor.entryCount = sizeof(image_bg_entries) / sizeof(WGPUBindGroupEntry);
    image_bg_descriptor.entries = image_bg_entries;
    return wgpuDeviceCreateBindGroup(bd->wgpuDevice, &image_bg_descriptor);
}

static void ImGui_ImplWGPU_SetupRenderState(ImDrawData* draw_data, WGPURenderPassEncoder ctx, FrameResources* fr)
{
    ImGui_ImplWGPU_Data* bd = ImGui_ImplWGPU_GetBackendData();

    // Setup orthographic projection matrix into our constant buffer
    // Our visible imgui space lies from draw_data->DisplayPos (top left) to draw_data->DisplayPos+data_data->DisplaySize (bottom right).
    {
        float L = draw_data->DisplayPos.x;
        float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
        float T = draw_data->DisplayPos.y;
        float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
        float mvp[4][4] =
        {
            { 2.0f/(R-L),   0.0f,           0.0f,       0.0f },
            { 0.0f,         2.0f/(T-B),     0.0f,       0.0f },
            { 0.0f,         0.0f,           0.5f,       0.0f },
            { (R+L)/(L-R),  (T+B)/(B-T),    0.5f,       1.0f },
        };
        wgpuQueueWriteBuffer(bd->defaultQueue, bd->renderResources.Uniforms, offsetof(Uniforms, MVP), mvp, sizeof(Uniforms::MVP));
        float gamma;
        switch (bd->renderTargetFormat)
        {
        case WGPUTextureFormat_ASTC10x10UnormSrgb:
        case WGPUTextureFormat_ASTC10x5UnormSrgb:
        case WGPUTextureFormat_ASTC10x6UnormSrgb:
        case WGPUTextureFormat_ASTC10x8UnormSrgb:
        case WGPUTextureFormat_ASTC12x10UnormSrgb:
        case WGPUTextureFormat_ASTC12x12UnormSrgb:
        case WGPUTextureFormat_ASTC4x4UnormSrgb:
        case WGPUTextureFormat_ASTC5x5UnormSrgb:
        case WGPUTextureFormat_ASTC6x5UnormSrgb:
        case WGPUTextureFormat_ASTC6x6UnormSrgb:
        case WGPUTextureFormat_ASTC8x5UnormSrgb:
        case WGPUTextureFormat_ASTC8x6UnormSrgb:
        case WGPUTextureFormat_ASTC8x8UnormSrgb:
        case WGPUTextureFormat_BC1RGBAUnormSrgb:
        case WGPUTextureFormat_BC2RGBAUnormSrgb:
        case WGPUTextureFormat_BC3RGBAUnormSrgb:
        case WGPUTextureFormat_BC7RGBAUnormSrgb:
        case WGPUTextureFormat_BGRA8UnormSrgb:
        case WGPUTextureFormat_ETC2RGB8A1UnormSrgb:
        case WGPUTextureFormat_ETC2RGB8UnormSrgb:
        case WGPUTextureFormat_ETC2RGBA8UnormSrgb:
        case WGPUTextureFormat_RGBA8UnormSrgb:
            gamma = 2.2f;
            break;
        default:
            gamma = 1.0f;
        }
        wgpuQueueWriteBuffer(bd->defaultQueue, bd->renderResources.Uniforms, offsetof(Uniforms, Gamma), &gamma, sizeof(Uniforms::Gamma));
    }

    // Setup viewport
    wgpuRenderPassEncoderSetViewport(ctx, 0, 0, draw_data->FramebufferScale.x * draw_data->DisplaySize.x, draw_data->FramebufferScale.y * draw_data->DisplaySize.y, 0, 1);

    // Bind shader and vertex buffers
    wgpuRenderPassEncoderSetVertexBuffer(ctx, 0, fr->VertexBuffer, 0, fr->VertexBufferSize * sizeof(ImDrawVert));
    wgpuRenderPassEncoderSetIndexBuffer(ctx, fr->IndexBuffer, sizeof(ImDrawIdx) == 2 ? WGPUIndexFormat_Uint16 : WGPUIndexFormat_Uint32, 0, fr->IndexBufferSize * sizeof(ImDrawIdx));
    wgpuRenderPassEncoderSetPipeline(ctx, bd->pipelineState);
    wgpuRenderPassEncoderSetBindGroup(ctx, 0, bd->renderResources.CommonBindGroup, 0, nullptr);

    // Setup blend factor
    WGPUColor blend_color = { 0.f, 0.f, 0.f, 0.f };
    wgpuRenderPassEncoderSetBlendConstant(ctx, &blend_color);
}

// Render function
// (this used to be set in io.RenderDrawListsFn and called by ImGui::Render(), but you can now call this directly from your main loop)
void ImGui_ImplWGPU_RenderDrawData(ImDrawData* draw_data, WGPURenderPassEncoder pass_encoder)
{
    // Avoid rendering when minimized
    if (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f)
        return;

    // FIXME: Assuming that this only gets called once per frame!
    // If not, we can't just re-allocate the IB or VB, we'll have to do a proper allocator.
    ImGui_ImplWGPU_Data* bd = ImGui_ImplWGPU_GetBackendData();
    bd->frameIndex = bd->frameIndex + 1;
    FrameResources* fr = &bd->pFrameResources[bd->frameIndex % bd->numFramesInFlight];

    // Create and grow vertex/index buffers if needed
    if (fr->VertexBuffer == nullptr || fr->VertexBufferSize < draw_data->TotalVtxCount)
    {
        if (fr->VertexBuffer)
        {
            wgpuBufferDestroy(fr->VertexBuffer);
            wgpuBufferDrop(fr->VertexBuffer);
            //wgpuBufferRelease(fr->VertexBuffer);
        }
        SafeRelease(fr->VertexBufferHost);
        fr->VertexBufferSize = draw_data->TotalVtxCount + 5000;

        WGPUBufferDescriptor vb_desc =
        {
            nullptr,
            "Dear ImGui Vertex buffer",
            WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex,
            MEMALIGN(fr->VertexBufferSize * sizeof(ImDrawVert), 4),
            false
        };
        fr->VertexBuffer = wgpuDeviceCreateBuffer(bd->wgpuDevice, &vb_desc);
        if (!fr->VertexBuffer)
            return;

        fr->VertexBufferHost = new ImDrawVert[fr->VertexBufferSize];
    }
    if (fr->IndexBuffer == nullptr || fr->IndexBufferSize < draw_data->TotalIdxCount)
    {
        if (fr->IndexBuffer)
        {
            wgpuBufferDestroy(fr->IndexBuffer);
            wgpuBufferDrop(fr->IndexBuffer);
            //wgpuBufferRelease(fr->IndexBuffer);
        }
        SafeRelease(fr->IndexBufferHost);
        fr->IndexBufferSize = draw_data->TotalIdxCount + 10000;

        WGPUBufferDescriptor ib_desc =
        {
            nullptr,
            "Dear ImGui Index buffer",
            WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index,
            MEMALIGN(fr->IndexBufferSize * sizeof(ImDrawIdx), 4),
            false
        };
        fr->IndexBuffer = wgpuDeviceCreateBuffer(bd->wgpuDevice, &ib_desc);
        if (!fr->IndexBuffer)
            return;

        fr->IndexBufferHost = new ImDrawIdx[fr->IndexBufferSize];
    }

    // Upload vertex/index data into a single contiguous GPU buffer
    ImDrawVert* vtx_dst = (ImDrawVert*)fr->VertexBufferHost;
    ImDrawIdx* idx_dst = (ImDrawIdx*)fr->IndexBufferHost;
    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
        vtx_dst += cmd_list->VtxBuffer.Size;
        idx_dst += cmd_list->IdxBuffer.Size;
    }
    int64_t vb_write_size = MEMALIGN((char*)vtx_dst - (char*)fr->VertexBufferHost, 4);
    int64_t ib_write_size = MEMALIGN((char*)idx_dst - (char*)fr->IndexBufferHost, 4);
    wgpuQueueWriteBuffer(bd->defaultQueue, fr->VertexBuffer, 0, fr->VertexBufferHost, vb_write_size);
    wgpuQueueWriteBuffer(bd->defaultQueue, fr->IndexBuffer,  0, fr->IndexBufferHost,  ib_write_size);

    // Setup desired render state
    ImGui_ImplWGPU_SetupRenderState(draw_data, pass_encoder, fr);

    // Render command lists
    // (Because we merged all buffers into a single one, we maintain our own offset into them)
    int global_vtx_offset = 0;
    int global_idx_offset = 0;
    ImVec2 clip_scale = draw_data->FramebufferScale;
    ImVec2 clip_off = draw_data->DisplayPos;
    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
        {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback != nullptr)
            {
                // User callback, registered via ImDrawList::AddCallback()
                // (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
                if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
                    ImGui_ImplWGPU_SetupRenderState(draw_data, pass_encoder, fr);
                else
                    pcmd->UserCallback(cmd_list, pcmd);
            }
            else
            {
                // Bind custom texture
                ImTextureID tex_id = pcmd->GetTexID();
                ImGuiID tex_id_hash = ImHashData(&tex_id, sizeof(tex_id));
                auto bind_group = bd->renderResources.ImageBindGroups.GetVoidPtr(tex_id_hash);
                if (bind_group)
                {
                    wgpuRenderPassEncoderSetBindGroup(pass_encoder, 1, (WGPUBindGroup)bind_group, 0, nullptr);
                }
                else
                {
                    WGPUBindGroup image_bind_group = ImGui_ImplWGPU_CreateImageBindGroup(bd->renderResources.ImageBindGroupLayout, (WGPUTextureView)tex_id);
                    bd->renderResources.ImageBindGroups.SetVoidPtr(tex_id_hash, image_bind_group);
                    wgpuRenderPassEncoderSetBindGroup(pass_encoder, 1, image_bind_group, 0, nullptr);
                }

                // Project scissor/clipping rectangles into framebuffer space
                ImVec2 clip_min((pcmd->ClipRect.x - clip_off.x) * clip_scale.x, (pcmd->ClipRect.y - clip_off.y) * clip_scale.y);
                ImVec2 clip_max((pcmd->ClipRect.z - clip_off.x) * clip_scale.x, (pcmd->ClipRect.w - clip_off.y) * clip_scale.y);
                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                    continue;

                // Apply scissor/clipping rectangle, Draw
                wgpuRenderPassEncoderSetScissorRect(pass_encoder, (uint32_t)clip_min.x, (uint32_t)clip_min.y, (uint32_t)(clip_max.x - clip_min.x), (uint32_t)(clip_max.y - clip_min.y));
                wgpuRenderPassEncoderDrawIndexed(pass_encoder, pcmd->ElemCount, 1, pcmd->IdxOffset + global_idx_offset, pcmd->VtxOffset + global_vtx_offset, 0);
            }
        }
        global_idx_offset += cmd_list->IdxBuffer.Size;
        global_vtx_offset += cmd_list->VtxBuffer.Size;
    }
}

static void ImGui_ImplWGPU_CreateFontsTexture()
{
    // Build texture atlas
    ImGui_ImplWGPU_Data* bd = ImGui_ImplWGPU_GetBackendData();
    ImGuiIO& io = ImGui::GetIO();
    unsigned char* pixels;
    int width, height, size_pp;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height, &size_pp);

    // Upload texture to graphics system
    {
        WGPUTextureDescriptor tex_desc = {};
        tex_desc.label = "Dear ImGui Font Texture";
        tex_desc.dimension = WGPUTextureDimension_2D;
        tex_desc.size.width = width;
        tex_desc.size.height = height;
        tex_desc.size.depthOrArrayLayers = 1;
        tex_desc.sampleCount = 1;
        tex_desc.format = WGPUTextureFormat_RGBA8Unorm;
        tex_desc.mipLevelCount = 1;
        tex_desc.usage = WGPUTextureUsage_CopyDst | WGPUTextureUsage_TextureBinding;
        bd->renderResources.FontTexture = wgpuDeviceCreateTexture(bd->wgpuDevice, &tex_desc);

        WGPUTextureViewDescriptor tex_view_desc = {};
        tex_view_desc.format = WGPUTextureFormat_RGBA8Unorm;
        tex_view_desc.dimension = WGPUTextureViewDimension_2D;
        tex_view_desc.baseMipLevel = 0;
        tex_view_desc.mipLevelCount = 1;
        tex_view_desc.baseArrayLayer = 0;
        tex_view_desc.arrayLayerCount = 1;
        tex_view_desc.aspect = WGPUTextureAspect_All;
        bd->renderResources.FontTextureView = wgpuTextureCreateView(bd->renderResources.FontTexture, &tex_view_desc);
    }

    // Upload texture data
    {
        WGPUImageCopyTexture dst_view = {};
        dst_view.texture = bd->renderResources.FontTexture;
        dst_view.mipLevel = 0;
        dst_view.origin = { 0, 0, 0 };
        dst_view.aspect = WGPUTextureAspect_All;
        WGPUTextureDataLayout layout = {};
        layout.offset = 0;
        layout.bytesPerRow = width * size_pp;
        layout.rowsPerImage = height;
        WGPUExtent3D size = { (uint32_t)width, (uint32_t)height, 1 };
        wgpuQueueWriteTexture(bd->defaultQueue, &dst_view, pixels, (uint32_t)(width * size_pp * height), &layout, &size);
    }

    // Create the associated sampler
    // (Bilinear sampling is required by default. Set 'io.Fonts->Flags |= ImFontAtlasFlags_NoBakedLines' or 'style.AntiAliasedLinesUseTex = false' to allow point/nearest sampling)
    {
        WGPUSamplerDescriptor sampler_desc = {};
        sampler_desc.minFilter = WGPUFilterMode_Linear;
        sampler_desc.magFilter = WGPUFilterMode_Linear;
        sampler_desc.mipmapFilter = WGPUMipmapFilterMode_Linear;
        sampler_desc.addressModeU = WGPUAddressMode_Repeat;
        sampler_desc.addressModeV = WGPUAddressMode_Repeat;
        sampler_desc.addressModeW = WGPUAddressMode_Repeat;
        sampler_desc.maxAnisotropy = 1;
        bd->renderResources.Sampler = wgpuDeviceCreateSampler(bd->wgpuDevice, &sampler_desc);
    }

    // Store our identifier
    static_assert(sizeof(ImTextureID) >= sizeof(bd->renderResources.FontTexture), "Can't pack descriptor handle into TexID, 32-bit not supported yet.");
    io.Fonts->SetTexID((ImTextureID)bd->renderResources.FontTextureView);
}

static void ImGui_ImplWGPU_CreateUniformBuffer()
{
    ImGui_ImplWGPU_Data* bd = ImGui_ImplWGPU_GetBackendData();
    WGPUBufferDescriptor ub_desc =
    {
        nullptr,
        "Dear ImGui Uniform buffer",
        WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform,
        MEMALIGN(sizeof(Uniforms), 16),
        false
    };
    bd->renderResources.Uniforms = wgpuDeviceCreateBuffer(bd->wgpuDevice, &ub_desc);
}

bool ImGui_ImplWGPU_CreateDeviceObjects()
{
    ImGui_ImplWGPU_Data* bd = ImGui_ImplWGPU_GetBackendData();
    if (!bd->wgpuDevice)
        return false;
    if (bd->pipelineState)
        ImGui_ImplWGPU_InvalidateDeviceObjects();

    // Create render pipeline
    WGPURenderPipelineDescriptor graphics_pipeline_desc = {};
    graphics_pipeline_desc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    graphics_pipeline_desc.primitive.stripIndexFormat = WGPUIndexFormat_Undefined;
    graphics_pipeline_desc.primitive.frontFace = WGPUFrontFace_CW;
    graphics_pipeline_desc.primitive.cullMode = WGPUCullMode_None;
    graphics_pipeline_desc.multisample.count = 1;
    graphics_pipeline_desc.multisample.mask = UINT_MAX;
    graphics_pipeline_desc.multisample.alphaToCoverageEnabled = false;

    // Bind group layouts
    WGPUBindGroupLayoutEntry common_bg_layout_entries[2] = {};
    common_bg_layout_entries[0].binding = 0;
    common_bg_layout_entries[0].visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    common_bg_layout_entries[0].buffer.type = WGPUBufferBindingType_Uniform;
    common_bg_layout_entries[1].binding = 1;
    common_bg_layout_entries[1].visibility = WGPUShaderStage_Fragment;
    common_bg_layout_entries[1].sampler.type = WGPUSamplerBindingType_Filtering;

    WGPUBindGroupLayoutEntry image_bg_layout_entries[1] = {};
    image_bg_layout_entries[0].binding = 0;
    image_bg_layout_entries[0].visibility = WGPUShaderStage_Fragment;
    image_bg_layout_entries[0].texture.sampleType = WGPUTextureSampleType_Float;
    image_bg_layout_entries[0].texture.viewDimension = WGPUTextureViewDimension_2D;

    WGPUBindGroupLayoutDescriptor common_bg_layout_desc = {};
    common_bg_layout_desc.entryCount = 2;
    common_bg_layout_desc.entries = common_bg_layout_entries;

    WGPUBindGroupLayoutDescriptor image_bg_layout_desc = {};
    image_bg_layout_desc.entryCount = 1;
    image_bg_layout_desc.entries = image_bg_layout_entries;

    WGPUBindGroupLayout bg_layouts[2];
    bg_layouts[0] = wgpuDeviceCreateBindGroupLayout(bd->wgpuDevice, &common_bg_layout_desc);
    bg_layouts[1] = wgpuDeviceCreateBindGroupLayout(bd->wgpuDevice, &image_bg_layout_desc);

    WGPUPipelineLayoutDescriptor layout_desc = {};
    layout_desc.bindGroupLayoutCount = 2;
    layout_desc.bindGroupLayouts = bg_layouts;
    graphics_pipeline_desc.layout = wgpuDeviceCreatePipelineLayout(bd->wgpuDevice, &layout_desc);

    // Create the vertex shader
    WGPUProgrammableStageDescriptor vertex_shader_desc = ImGui_ImplWGPU_CreateShaderModule(__shader_vert_wgsl);
    graphics_pipeline_desc.vertex.module = vertex_shader_desc.module;
    graphics_pipeline_desc.vertex.entryPoint = vertex_shader_desc.entryPoint;

    // Vertex input configuration
    WGPUVertexAttribute attribute_desc[] =
    {
        { WGPUVertexFormat_Float32x2, (uint64_t)IM_OFFSETOF(ImDrawVert, pos), 0 },
        { WGPUVertexFormat_Float32x2, (uint64_t)IM_OFFSETOF(ImDrawVert, uv),  1 },
        { WGPUVertexFormat_Unorm8x4,  (uint64_t)IM_OFFSETOF(ImDrawVert, col), 2 },
    };

    WGPUVertexBufferLayout buffer_layouts[1];
    buffer_layouts[0].arrayStride = sizeof(ImDrawVert);
    buffer_layouts[0].stepMode = WGPUVertexStepMode_Vertex;
    buffer_layouts[0].attributeCount = 3;
    buffer_layouts[0].attributes = attribute_desc;

    graphics_pipeline_desc.vertex.bufferCount = 1;
    graphics_pipeline_desc.vertex.buffers = buffer_layouts;

    // Create the pixel shader
    WGPUProgrammableStageDescriptor pixel_shader_desc = ImGui_ImplWGPU_CreateShaderModule(__shader_frag_wgsl);

    // Create the blending setup
    WGPUBlendState blend_state = {};
    blend_state.alpha.operation = WGPUBlendOperation_Add;
    blend_state.alpha.srcFactor = WGPUBlendFactor_One;
    blend_state.alpha.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
    blend_state.color.operation = WGPUBlendOperation_Add;
    blend_state.color.srcFactor = WGPUBlendFactor_SrcAlpha;
    blend_state.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;

    WGPUColorTargetState color_state = {};
    color_state.format = bd->renderTargetFormat;
    color_state.blend = &blend_state;
    color_state.writeMask = WGPUColorWriteMask_All;

    WGPUFragmentState fragment_state = {};
    fragment_state.module = pixel_shader_desc.module;
    fragment_state.entryPoint = pixel_shader_desc.entryPoint;
    fragment_state.targetCount = 1;
    fragment_state.targets = &color_state;

    graphics_pipeline_desc.fragment = &fragment_state;

    // Create depth-stencil State
    WGPUDepthStencilState depth_stencil_state = {};
    depth_stencil_state.format = bd->depthStencilFormat;
    depth_stencil_state.depthWriteEnabled = false;
    depth_stencil_state.depthCompare = WGPUCompareFunction_Always;
    depth_stencil_state.stencilFront.compare = WGPUCompareFunction_Always;
    depth_stencil_state.stencilBack.compare = WGPUCompareFunction_Always;

    // Configure disabled depth-stencil state
    graphics_pipeline_desc.depthStencil = (bd->depthStencilFormat == WGPUTextureFormat_Undefined) ? nullptr :  &depth_stencil_state;

    bd->pipelineState = wgpuDeviceCreateRenderPipeline(bd->wgpuDevice, &graphics_pipeline_desc);

    ImGui_ImplWGPU_CreateFontsTexture();
    ImGui_ImplWGPU_CreateUniformBuffer();

    // Create resource bind group
    WGPUBindGroupEntry common_bg_entries[] =
    {
        { nullptr, 0, bd->renderResources.Uniforms, 0, MEMALIGN(sizeof(Uniforms), 16), 0, 0 },
        { nullptr, 1, 0, 0, 0, bd->renderResources.Sampler, 0 },
    };

    WGPUBindGroupDescriptor common_bg_descriptor = {};
    common_bg_descriptor.layout = bg_layouts[0];
    common_bg_descriptor.entryCount = sizeof(common_bg_entries) / sizeof(WGPUBindGroupEntry);
    common_bg_descriptor.entries = common_bg_entries;
    bd->renderResources.CommonBindGroup = wgpuDeviceCreateBindGroup(bd->wgpuDevice, &common_bg_descriptor);

    WGPUBindGroup image_bind_group = ImGui_ImplWGPU_CreateImageBindGroup(bg_layouts[1], bd->renderResources.FontTextureView);
    bd->renderResources.ImageBindGroup = image_bind_group;
    bd->renderResources.ImageBindGroupLayout = bg_layouts[1];
    bd->renderResources.ImageBindGroups.SetVoidPtr(ImHashData(&bd->renderResources.FontTextureView, sizeof(ImTextureID)), image_bind_group);

    SafeRelease(vertex_shader_desc.module);
    SafeRelease(pixel_shader_desc.module);
    SafeRelease(bg_layouts[0]);

    return true;
}

void ImGui_ImplWGPU_InvalidateDeviceObjects()
{
    ImGui_ImplWGPU_Data* bd = ImGui_ImplWGPU_GetBackendData();
    if (!bd->wgpuDevice)
        return;

    SafeRelease(bd->pipelineState);
    SafeRelease(bd->renderResources);

    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->SetTexID(0); // We copied g_pFontTextureView to io.Fonts->TexID so let's clear that as well.

    for (unsigned int i = 0; i < bd->numFramesInFlight; i++)
        SafeRelease(bd->pFrameResources[i]);
}

bool ImGui_ImplWGPU_Init(WGPUDevice device, int num_frames_in_flight, WGPUTextureFormat rt_format, WGPUTextureFormat depth_format)
{
    ImGuiIO& io = ImGui::GetIO();
    IM_ASSERT(io.BackendRendererUserData == nullptr && "Already initialized a renderer backend!");

    // Setup backend capabilities flags
    ImGui_ImplWGPU_Data* bd = IM_NEW(ImGui_ImplWGPU_Data)();
    io.BackendRendererUserData = (void*)bd;
    io.BackendRendererName = "imgui_impl_webgpu";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;  // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.

    bd->wgpuDevice = device;
    bd->defaultQueue = wgpuDeviceGetQueue(bd->wgpuDevice);
    bd->renderTargetFormat = rt_format;
    bd->depthStencilFormat = depth_format;
    bd->numFramesInFlight = num_frames_in_flight;
    bd->frameIndex = UINT_MAX;

    bd->renderResources.FontTexture = nullptr;
    bd->renderResources.FontTextureView = nullptr;
    bd->renderResources.Sampler = nullptr;
    bd->renderResources.Uniforms = nullptr;
    bd->renderResources.CommonBindGroup = nullptr;
    bd->renderResources.ImageBindGroups.Data.reserve(100);
    bd->renderResources.ImageBindGroup = nullptr;
    bd->renderResources.ImageBindGroupLayout = nullptr;

    // Create buffers with a default size (they will later be grown as needed)
    bd->pFrameResources = new FrameResources[num_frames_in_flight];
    for (int i = 0; i < num_frames_in_flight; i++)
    {
        FrameResources* fr = &bd->pFrameResources[i];
        fr->IndexBuffer = nullptr;
        fr->VertexBuffer = nullptr;
        fr->IndexBufferHost = nullptr;
        fr->VertexBufferHost = nullptr;
        fr->IndexBufferSize = 10000;
        fr->VertexBufferSize = 5000;
    }

    return true;
}

void ImGui_ImplWGPU_Shutdown()
{
    ImGui_ImplWGPU_Data* bd = ImGui_ImplWGPU_GetBackendData();
    IM_ASSERT(bd != nullptr && "No renderer backend to shutdown, or already shutdown?");
    ImGuiIO& io = ImGui::GetIO();

    ImGui_ImplWGPU_InvalidateDeviceObjects();
    delete[] bd->pFrameResources;
    bd->pFrameResources = nullptr;
    //wgpuQueueRelease(bd->defaultQueue);
    bd->wgpuDevice = nullptr;
    bd->numFramesInFlight = 0;
    bd->frameIndex = UINT_MAX;

    io.BackendRendererName = nullptr;
    io.BackendRendererUserData = nullptr;
    io.BackendFlags &= ~ImGuiBackendFlags_RendererHasVtxOffset;
    IM_DELETE(bd);
}

void ImGui_ImplWGPU_NewFrame()
{
    ImGui_ImplWGPU_Data* bd = ImGui_ImplWGPU_GetBackendData();
    if (!bd->pipelineState)
        ImGui_ImplWGPU_CreateDeviceObjects();
}
