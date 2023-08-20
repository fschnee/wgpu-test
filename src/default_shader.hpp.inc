R"(
struct scene_uniforms
{
    view: mat4x4<f32>,
    projection: mat4x4<f32>,
    time: f32,
};

struct object_uniforms
{
    transform: mat4x4<f32>
};

@group(0) @binding(0) var<uniform> s: scene_uniforms;
@group(0) @binding(1) var<uniform> o: object_uniforms;

struct VertIn
{
    @location(0) position: vec3f,
    @location(1) color: vec3f,
};

struct FragIn
{
    @builtin(position) position: vec4f,

    @location(0) color: vec3f,
};

@vertex
fn vert(in: VertIn) -> FragIn
{
    var out: FragIn;
    out.position = s.projection * s.view * o.transform * vec4f(in.position, 1.0);
    out.color    = in.color;
    return out;
}

@fragment
fn frag(in: FragIn) -> @location(0) vec4f { return vec4f(in.color, 1.0); }
)"