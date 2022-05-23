#include "/include/GLSLInit.glsl"
#include "/include/Materials.glsl" //! #include "./include/Materials.glsl"

layout(location = 0) out vec4 vDiffuse_occlusion; // <vec3, float>
layout(location = 1) out vec4 vSpecular_shininess; // <vec3, float>
layout(location = 2) out vec3 vNormal
// layout(location = 3) out vec3 result
// Depth/Stencil

in VS_OUT
{
    vec3 vWorldPos;
    vec2 vTex;
    vec3 vNorm, vTang;
    vec4 vCol;

    flat uvec2 vModelPartIdx; // matrix, material
} fs_in;

vec3 GetNormal( MaterialSpec mat, vec3 Norm, vec3 Tan, vec2 vTexUV )
{
    vec3 N = normalize( Norm );
    vec3 T = normalize( Tan - N * dot(N, Tan) );
    vec3 B = cross( T, N );

    return mat3( T, B, N ) * ( SampleTexture( mat.iTexNormal, vTexUV ).rgb * 2 - 1 );
}

void main()
{
    MaterialSpec material = oMaterials[fs_in.vModelPartIdx.y];

    vec4 color = fs_in.vCol * material.vColDif * SampleTexture( material.iTexDiffuse, fs_in.vTex );
    if( color.a < 0.5 ) discard;

    vDiffuse_occlusion = vec4( color.rgb, 1 );
    vSpecular_shininess = mat.vColSpec * vec4( SampleTexture( mat.iTexSpecular, vTexUV ).rgb, 1.0 );
    vNormal = GetNormal( material, fs_in.vNorm, fs_in.vTang, fs_in.vTex );
}