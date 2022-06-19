#version 450
//! #extension GL_GOOGLE_include_directive : enable

#include "include/GLSLInit.glsl"
#include "include/Matrices.glsl"

layout (location = 0) in vec3 vPos;
layout (location = 1) in vec2 vTex;
layout (location = 2) in vec3 vNorm;
layout (location = 3) in vec3 vTangent;
//layout (location = 4) in vec3 vBitangent;
layout (location = 4) in vec4 vCol;

layout(location = 0) out VS_OUT
{
    vec3 vWorldPos;
    vec2 vTex;
    vec3 vNorm;
    vec3 vTang;
    vec4 vCol;

    flat uvec2 vModelPartIdx; // matrix, material
} vs_out;

out gl_PerVertex
{
    vec4 gl_Position;
};

void main()
{
    vs_out.vModelPartIdx = uvec2( 0xffff & (gl_BaseInstanceARB >> 16), 0xffff & gl_BaseInstanceARB ).xy;

    ModelMatrixSet mtx = modelMatrices[vs_out.vModelPartIdx.x];//GetModelMatrices( int(  ), false );

    mat4 model = mModel * mtx.mModel;
    mat3 modelIT = mat3(mModelIT) * mat3(mtx.mModelIT);

    vec4 pos = model * vec4(vPos,1);
    gl_Position = mVP * pos;

    vs_out.vWorldPos = pos.xyz / pos.w;
    vs_out.vTex = vTex;//+vec2( sin(fTime), cos(fTime) );
    vs_out.vNorm = normalize( modelIT * vNorm );
    vs_out.vTang = normalize( modelIT * vTangent );
    vs_out.vCol = vCol;
}