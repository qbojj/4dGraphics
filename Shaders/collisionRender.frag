#version 450
//! #extension GL_GOOGLE_include_directive : enable

#include "include/GLSLInit.glsl"
layout(location = 0) in vec4 vCol;
layout(location = 0) out vec4 frag;

void main()
{
	frag = vCol;
}