#version 330 core

// Fullscreen triangle generated entirely from gl_VertexID -- no VBO needed.
// Vertex IDs 0,1,2 produce UVs (0,0), (2,0), (0,2), giving a triangle that
// covers the [0..1] x [0..1] window, which is then expanded to NDC.
out vec2 vUV;

void main()
{
    vec2 uv = vec2(float((gl_VertexID << 1) & 2),
                   float( gl_VertexID       & 2));
    vUV = uv;
    gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
}
