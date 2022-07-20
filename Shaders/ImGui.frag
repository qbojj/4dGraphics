#version 450
//! #extension GL_GOOGLE_include_directive : enable

#include "include/GLSLInit.glsl"
layout( binding = 0 ) uniform sampler2D Texture;
layout( location = 0 ) out vec4 vOutColor;

layout( location = 0 ) in VERT_OUT 
{
	vec2 UV;
	vec4 Color;
} fs_in;

void main()
{	
	vOutColor = fs_in.Color * texture( Texture, fs_in.UV.st );
}