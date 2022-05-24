#version 450
#include "include/GLSLInit.glsl"
out vec4 vFragColor;

uniform sampler2D tTexture;

in VS_OUT
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