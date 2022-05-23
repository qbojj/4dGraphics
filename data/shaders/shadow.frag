#include "/include/GLSLInit.glsl"
#include "/include/Materials.glsl" //! #include "./include/Materials.glsl"
in GS_OUT
{
    vec3 vTexAlpha; // <vec2 tex, float alpha>
    flat uint vMaterialIdx; //  material
} fs_in;

void main()
{
    MaterialSpec material = oMaterials[fs_in.vMaterialIdx];
    if( SampleTexture( material.iTexDiffuse, fs_in.vTexAlpha.xy ).a * material.vColDif.a * fs_in.vTexAlpha.z < 0.5 ) discard;
}