#version 450
#extension GL_GOOGLE_include_directive : enable

#include "include/GLSLInit.glsl"
layout(location = 0) in vec2 TexCoords;
layout(location = 0) out vec4 FragColor;

layout(binding = 0) uniform sampler2D tTex;
layout(location = 64+0) uniform vec4 vTextColor;

void main()
{    
    FragColor = vTextColor * texture(tTex, TexCoords);
}