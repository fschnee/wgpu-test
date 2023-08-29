R"(
struct scene_uniforms
{
    view: mat4x4f,
    projection: mat4x4f,
    light_direction: vec3f,
    light_color: vec3f,
    time: f32,
    gamma: f32,
};

struct object_uniforms
{
    transform: mat4x4f
};

@group(0) @binding(0) var<uniform> s: scene_uniforms;
@group(1) @binding(0) var<uniform> o: object_uniforms;

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
    out.normal   = (o.transform * vec4f(in.normal, 0.0)).xyz; // Make lighting direction-dependent.
    return out;
}

@fragment
fn frag(in: FragIn) -> @location(0) vec4f
{
    let normal       = normalize(in.normal); // If we normalize in the vertex shader
                                             // it gets interpolated between fragments.
    let shading      = dot(normalize(s.light_direction), normal) * s.light_color;
    let color        = in.color * shading;
    let linear_color = pow(color, vec3f(s.gamma));
    return vec4f(linear_color, 1.0);
}
)"