#define FOR_RENDERDOC (1)

#ifdef __cplusplus
#define CPP_GLSL_SWITCH( cpp, glsl ) cpp
#else

#include "GLSLInit.glsl"


#define CPP_GLSL_SWITCH( cpp, glsl ) glsl
#endif

#define CPP_STRUCT( def ) CPP_GLSL_SWITCH( struct, def )

#define MAX_LIGHTS 16
#define MAX_SHADOW_MAPS 256

#define UBO_MATS_VP_BINDING 0
#define UBO_LIGHTS_BINDING 1
#define SSBO_MODEL_MATRICES_BINDING 0
#define SSBO_MATERIALS_BINDING 1

struct LightSpec {
    uint bEnabled;
    int iLightType; // 0 - directional, 1 - spot, 2 - point
    int iShadowMapId;

    int p1;

    vec4 vDir; // 0 for point light
    vec4 vPos;

    vec2 vCutoff; // ( inner cutoff, outer cutoff ); (-1,*) for directional
    int p2, p3;

    vec4 vAttenuation; // (constant, linear, quadratic)
    vec4 vColAmb, vColDif, vColSpec;
};

struct MaterialSpec
{
    vec4 vColAmb;
    vec4 vColDif; // <r,g,b, alpha>
    vec4 vColSpec; // <r,g,b, shininess>
    uvec2 iTexDiffuse, iTexSpecular, iTexNormal;
    int p1, p2;
};

struct ModelMatrixSet
{
    mat4 mModel, mModelIT;
};

CPP_STRUCT( layout( std140, binding = UBO_MATS_VP_BINDING ) uniform ) MatsVP
{
    mat4 mView, mViewIT;
    mat4 mProj, mVP;
    vec4 vViewWorldPos;

    float fTime;
    int p1, p2, p3;
};

#define PACKED_BITS_VALUE_IDXS( idx )    [ ( (int)idx ) >> 7][( ( (int)idx ) >> 5 ) & 3]
#define PACKED_BITS_ELEM( idx )             ( 1 << ( ( (int)idx ) & 31 ) ) 
#define PACKED_BITS_GET( bitset, idx )      ( ( bitset PACKED_BITS_VALUE_IDXS( idx ) & PACKED_BITS_ELEM(idx) ) != 0 )
#define PACKED_BITS_SET( bitset, idx )      ( bitset PACKED_BITS_VALUE_IDXS( idx ) |= PACKED_BITS_ELEM(idx) )
#define PACKED_BITS_RESET( bitset, idx )    ( bitset PACKED_BITS_VALUE_IDXS( idx ) &= ~PACKED_BITS_ELEM(idx) )

CPP_STRUCT( layout( std140, binding = UBO_LIGHTS_BINDING ) uniform ) Lights {
    LightSpec aoLights[MAX_LIGHTS];
    mat4 amShadowMatrices[MAX_SHADOW_MAPS];

    // packed bitset (ivec4 to have packed array (because of how arrays work in std140) and int for cpp/glsl compatibility)
    ivec4 baShadowMapActive[(MAX_SHADOW_MAPS + 127) / 128]; 
};

CPP_STRUCT( layout( std140, binding = SSBO_MODEL_MATRICES_BINDING ) readonly restrict buffer ) ModelMatrices
{
    ModelMatrixSet modelMatrices[CPP_GLSL_SWITCH(1,)];
};

CPP_STRUCT( layout( std140, binding = SSBO_MATERIALS_BINDING ) readonly restrict buffer ) Materials
{
    MaterialSpec oMaterials[CPP_GLSL_SWITCH(1,)];
};

#undef CPP_GLSL_SWITCH
#undef CPP_STRUCT
