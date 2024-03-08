#version 460 core
#extension GL_GOOGLE_include_directive : require
#include "simple_interface.glsl"

layout(location = 0) out VertexOutput vert_out;

void main() {
    VertexData vd = vertex[gl_VertexIndex];
    gl_Position = uniforms.VP * model * vec4(vertex.position, 1.0);
    vert_out.normal = vd.normal;
    vert_out.texcoord = vd.texcoord;
    vert_out.color = vd.color;
}