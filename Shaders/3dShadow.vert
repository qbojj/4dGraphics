#version 450
//! #extension GL_GOOGLE_include_directive : enable

#include "include/GLSLInit.glsl"
#include "include/Matrices.glsl"

layout (location = 0) in vec3 vPos;
layout (location = 1) in vec2 vTex;
layout (location = 4) in vec4 vCol;

out gl_PerVertex
{
    vec4 gl_Position;
    float gl_PointSize;
    float gl_ClipDistance[];
};

layout(location = 0) out VS_OUT
{
	vec3 vTexAlpha; // <vec2 tex, float alpha>
    flat uint vMaterialIdx; // material
} vs_out;

void main()
{
    uvec2 vModelPartIdx =  uvec2( 0xffff & (gl_BaseInstanceARB >> 16), 0xffff & gl_BaseInstanceARB );

    //ModelMatrixSet mats = GetModelMatrices( int( vModelPartIdx.x ), true );
    gl_Position = mModel * modelMatrices[vModelPartIdx.x].mModel * vec4(vPos,1);
    vs_out.vTexAlpha = vec3( vTex, vCol.a );
    vs_out.vMaterialIdx = vModelPartIdx.y;
}