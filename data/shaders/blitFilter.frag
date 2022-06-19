#version 450
//! #extension GL_GOOGLE_include_directive : enable

#include "include/GLSLInit.glsl"

out vec4 vFragColor;
in vec2 vPos;

uniform sampler2D tTex;

void main()
{
	vFragColor = texture(tTex, vPos);
}