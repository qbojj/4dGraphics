#version 450
//! #extension GL_GOOGLE_include_directive : enable

#include "include/GLSLInit.glsl"
layout (location = 0) in vec4 vertex; // <vec2 pos, vec2 tex>
layout(location = 0) out vec2 TexCoords;

layout(location = 32+0) uniform mat4 mProjection;

out gl_PerVertex
{
    vec4 gl_Position;
    float gl_PointSize;
    float gl_ClipDistance[];
};

void main()
{
    gl_Position = mProjection * vec4(vertex.xy, 0.0, 1.0);
    TexCoords = vertex.zw;
}
