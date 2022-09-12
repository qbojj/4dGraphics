#version 450
//! #extension GL_GOOGLE_include_directive : enable

#include "include/GLSLInit.glsl"
layout(location = 0) out vec4 vFragColor;

uniform sampler2D tTexture;

layout(location = 0) in VS_OUT
{
    vec3 vWorldPos;
    vec2 vTex;
    vec3 vNorm;
    vec4 vCol;

    mat3 mTBN;
} fs_in;

void main()
{    
    vFragColor = vec4( texture( tTexture, fs_in.vTex ).rgb, 1 );
    if( vFragColor.a == 0 ) discard;
}