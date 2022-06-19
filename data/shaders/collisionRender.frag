#version 450
//! #extension GL_GOOGLE_include_directive : enable

#include "include/GLSLInit.glsl"
out vec4 frag;

in vec4 vCol;

void main()
{
	frag = vCol;
}