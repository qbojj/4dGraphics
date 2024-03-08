#version 460 core
#extension GL_GOOGLE_include_directive : require
#include "simple_interface.glsl"

layout(location = 0) out vec4 fragColor;
layout(location = 0) in VertexOutput frag_in;


layout(constant_id = 0) const int debug = 0;

void main() {
    if (debug == 0)
        fragColor = frag_in.color;
    else
        fragColor = vec4(frag_in.normal * 0.5 + 0.5, 1);
}