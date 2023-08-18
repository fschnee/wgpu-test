R"(
struct VertexOut {
    @builtin(position) position : vec4<f32>,
    @location(0) color : vec4<f32>,
};

@vertex
fn vert(@builtin(vertex_index) in_vertex_index: u32) -> VertexOut
{
    var p : VertexOut;
    if      (in_vertex_index == 0u) { p.position = vec4f(-0.5, -0.5, 0.0, 1.0); p.color = vec4f(1.0, 0.0, 0.0, 1.0); }
    else if (in_vertex_index == 1u) { p.position = vec4f(0.5, -0.5, 0.0, 1.0);  p.color = vec4f(0.0, 1.0, 0.0, 1.0); }
    else                            { p.position = vec4f(0.0, 0.5, 0.0, 1.0);   p.color = vec4f(0.0, 0.0, 1.0, 1.0); }
    return p;
}

@fragment
fn frag(fragData: VertexOut) -> @location(0) vec4<f32>
{
    return fragData.color;
}
)"