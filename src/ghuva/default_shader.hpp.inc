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

@group(0) @binding(0) var<uniform> s: scene_uniforms;

struct VertIn
{
    @location(0) position: vec3f,
    @location(1) color: vec3f,
    @location(2) normal: vec3f,

    @location(3) t1: vec4f,
    @location(4) t2: vec4f,
    @location(5) t3: vec4f,
    @location(6) t4: vec4f,
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
    let transform = mat4x4f(in.t1, in.t2, in.t3, in.t4);

    var out: FragIn;
    out.position = s.projection * s.view * transform * vec4f(in.position, 1.0);
    out.color    = in.color;
    out.normal   = (transform * vec4f(in.normal, 0.0)).xyz; // Make lighting direction-dependent.
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