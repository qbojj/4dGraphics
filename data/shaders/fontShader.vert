#version 450
#include "include/GLSLInit.glsl"
layout (location = 0) in vec4 vertex; // <vec2 pos, vec2 tex>
out vec2 TexCoords;

uniform mat4 mProjection;

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
