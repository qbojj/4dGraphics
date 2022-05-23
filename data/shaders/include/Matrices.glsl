#include "/include/CommonCppGLSL.glsl" //! #include "./CommonCppGLSL.glsl"

layout(location = 0) uniform mat4 mModel;
layout(location = 1) uniform mat4 mModelIT; // inverse(transpose(mModel)) ( or mModel if mModel orthogonal )

/*
uniform sampler2DRect tModelMatrices;
ModelMatrixSet GetModelMatrices( int idx, bool withoutIT )
{
    ModelMatrixSet dat;

    dat.mModel = 
    mat4(
        texelFetch( tModelMatrices, ivec2( 0, idx ) ),
        texelFetch( tModelMatrices, ivec2( 1, idx ) ),
        texelFetch( tModelMatrices, ivec2( 2, idx ) ),
        texelFetch( tModelMatrices, ivec2( 3, idx ) )
    );

    if( !withoutIT )
    {
        dat.mModelIT = 
        mat4(
            texelFetch( tModelMatrices, ivec2( 4, idx ) ),
            texelFetch( tModelMatrices, ivec2( 5, idx ) ),
            texelFetch( tModelMatrices, ivec2( 6, idx ) ),
            texelFetch( tModelMatrices, ivec2( 7, idx ) )
        );
    }

    return dat;
}
*/
