#version 450
#include "include/GLSLInit.glsl"
#include "include/Matrices.glsl" //! #include "./include/Matrices.glsl"

layout(location=0) in vec3 pos;
layout(location=1) in vec4 col;

out gl_PerVertex
{
    vec4 gl_Position;
};

out vec4 vCol;
void main()
{
	gl_Position = mVP * vec4(pos,1);
    vCol = col;
}