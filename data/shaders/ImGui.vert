#include "/include/GLSLInit.glsl"
precision mediump float;
layout( location = 0 ) in vec2 Position;
layout( location = 1 ) in vec2 UV;
layout( location = 2 ) in vec4 Color;

layout( std140, binding = 0 ) uniform PerFrameData
{
	mat4 MVP;
};

out VERT_OUT 
{
	vec2 UV;
	vec4 Color;
} vs_out;

void main()
{
	vs_out.Color = Color;
	vs_out.UV = UV;
	
	gl_Position = MVP * vec4( Position, 0, 1 );
}