R"(
fn tran(v: vec4f) -> mat4x4f
{
    return mat4x4f(
        1.0, 0.0, 0.0, v.x,
        0.0, 1.0, 0.0, v.y,
        0.0, 0.0, 1.0, v.z,
        0.0, 0.0, 0.0, 1.0
    );
}

fn xrot(x: f32) -> mat4x4f
{
    let c = cos(x);
    let s = sin(x);
    return mat4x4f(
        1.0, 0.0, 0.0, 0.0,
        0.0, c,   s,   0.0,
        0.0, -s,  c,   0.0,
        0.0, 0.0, 0.0, 1.0
    );
}

fn yrot(y: f32) -> mat4x4f
{
    let c = cos(y);
    let s = sin(y);
    return mat4x4f(
        c,   0.0, -s,  0.0,
        0.0, 1.0, 0.0, 0.0,
        s,   0.0, c,   0.0,
        0.0, 0.0, 0.0, 1.0
    );
}

fn zrot(z: f32) -> mat4x4f
{
    let c = cos(z);
    let s = sin(z);
    return mat4x4f(
        c,   s,   0.0, 0.0,
        -s,  c,   0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0
    );
}

fn scal(s: vec4f) -> mat4x4f
{
    return mat4x4f(
        s.x, 0.0, 0.0, 0.0,
        0.0, s.y, 0.0, 0.0,
        0.0, 0.0, s.z, 0.0,
        0.0, 0.0, 0.0, 1.0
    );
}

@group(0) @binding(0) var<storage, read_write> transform_buffer: array<mat4x4f>;

@compute @workgroup_size(32)
fn compute(@builtin(global_invocation_id) id: vec3<u32>)
{
    let in: mat4x4f = transform_buffer[id.x];
    let transform =
          transpose( tran(in[0]) )
        * transpose( zrot(in[1].x) )
        * transpose( yrot(in[1].y) )
        * transpose( xrot(in[2].x) )
        * scal(in[3]);

    transform_buffer[id.x] = transform;
}
)"