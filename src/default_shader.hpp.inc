R"(
@group(0) @binding(0) var<uniform> transform: mat4x4<f32>;

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
    var o: FragIn;
    o.position = vec4f(in.position, 1.0);
    o.color    = in.color;
    return o;

}

@fragment
fn frag(in: FragIn) -> @location(0) vec4f { return vec4f(in.color, 1.0); }
)"