R"(
struct scene_uniforms
{
    view: mat4x4<f32>,
    projection: mat4x4<f32>,
    time: f32,
    gamma: f32,
};

struct object_uniforms
{
    transform: mat4x4<f32>
};

@group(0) @binding(0) var<uniform> s: scene_uniforms;
// TODO: move object_uniforms to another binding GROUP not BINDING.
@group(0) @binding(1) var<uniform> o: object_uniforms;

struct VertIn
{
    @location(0) position: vec3f,
    @location(1) color: vec3f,
    @location(2) normal: vec3f,
};

struct FragIn
{
    @builtin(position) position: vec4f,

    @location(0) color: vec3f,
    @location(1) normal: vec3f,
};

@vertex
fn vert(in: VertIn) -> FragIn
{
    var out: FragIn;
    out.position = s.projection * s.view * o.transform * vec4f(in.position, 1.0);
    out.color    = in.color;
    out.normal   = in.normal;
    return out;
}

@fragment
fn frag(in: FragIn) -> @location(0) vec4f
{
    let color = in.normal * 0.5 + 0.5;
    let linear_color = pow(color, vec3f(s.gamma));
    return vec4f(linear_color, 1.0);
}
)"