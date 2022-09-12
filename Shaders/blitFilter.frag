#version 450
//! #extension GL_GOOGLE_include_directive : enable

#include "include/GLSLInit.glsl"
layout(location = 0) in vec2 vPos;
layout(location = 0) out vec4 vFragColor;

layout(binding = 0) uniform sampler2D tTex;

void main()
{
	vFragColor = texture(tTex, vPos);
}