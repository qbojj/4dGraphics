#version 450
//! #extension GL_GOOGLE_include_directive : enable

#include "include/GLSLInit.glsl"
#include "include/Lights.glsl"
#include "include/Matrices.glsl"
#include "include/Materials.glsl"


layout(location = 0) out vec4 vFragColor;
//layout(location = 1) out vec3 vFragNorm;

layout(location = 0) in VS_OUT
{
    vec3 vWorldPos;
    vec2 vTex;
    vec3 vNorm;
    vec3 vTang;
    vec4 vCol;
    flat uvec2 vModelPartIdx; // matrix, material
} fs_in;

struct ColorSpec
{
    vec3 vAmb;
    vec3 vDif;
    vec3 vSpec;
    float fAlpha;
};

layout(location = 64+0) uniform bool bUseToonShading = false;
layout(location = 64+1) uniform uint uToonQuants = 16;

const float fPI = radians(180);
const float fE = exp(1);

float GetShadowStrength( int ShadowMapId, vec3 vPos, float bias )
{
    int id = ShadowMapId;
    vec4 pos = amShadowMatrices[id] * vec4(vPos,1);
    vec3 proj = ( pos.xyz /*+ vec3(0,0,bias)*/ ) / pos.w;

    proj.z *= bias;

    if( proj.z < 0 ) return 1; // z == depth

    vec2 texelSize = 1.0 / textureSize(ShadowMaps, 0).xy;
    vec2 scale = texelSize * 2;

    float sum = 0;
    const int s = 2;
    for(int x = -s; x <= s; ++x)
        for(int y = -s; y <= s; ++y)
            sum += texture(ShadowMaps, vec4( (proj.xy + vec2(x, y) * scale ) * 0.5 + 0.5, id, proj.z ) ); 

    return sum / ((2*s+1)*(2*s+1));//texture( ShadowMaps, vec4( proj.xy * 0.5 + 0.5, id, proj.z + bias ) );
}

float GetShadowStrength_Pointlight( int ShadowMapId, vec3 vPos, float bias, vec3 lightPos )
{
    const vec3 sampleOffsetDirections[20] = vec3[]
    (
       vec3( 1,  1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1,  1,  1), 
       vec3( 1,  1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1,  1, -1),
       vec3( 1,  1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1,  1,  0),
       vec3( 1,  0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1,  0, -1),
       vec3( 0,  1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0,  1, -1)
    );  

    int id = ShadowMapId;
    vec3 relPos = vPos - lightPos;

    int samples = 20;
    float viewDistance = length(vViewWorldPos.xyz - vPos);
    float diskRadius = (1.0 + (viewDistance / 1000/*far*/)) / 10;

    vec3 absRelPos = abs( relPos );
    float depthVal = 0.1 /*near*/ / ( max( max( absRelPos.x, absRelPos.y), absRelPos.z ) /*- bias*/ ) * bias;

    float sum = 0;
    for(int i = 0; i < samples; ++i)
    {
        vec3 pos2 = relPos + sampleOffsetDirections[i] * diskRadius;

        sum += texture(ShadowPointMaps, vec4( pos2, id/6 ), depthVal );
    }

    return sum / float(samples);//texture( ShadowPointMaps, vec4( relPos, id/6 ), depthVal + bias );
}

ColorSpec CalculateLigtning( MaterialSpec mat, vec3 vPos, vec3 vNorm, vec3 vViewDir, vec2 vTexUV )
{
    ColorSpec res = {vec3(0),vec3(0),vec3(0),1};
    
    const float fSpecularEnergyConservation = 
    //    mat.iShadingModel == ShadingPhong ? 
    //    ( 2.0 + mat.fShininess ) / ( 2.0 * fPI ) : 
        ( 8.0 + mat.vColSpec.a ) / ( 8.0 * fPI ); 

    for( int i = 0; i < aoLights.length(); i++ )
    {
        if( aoLights[i].bEnabled == 0 ) continue;
        
        res.vAmb += clamp(aoLights[i].vColAmb.rgb,0,1);

        vec3 vLightDir; // direction from fragment to light

        float LightForce = 1;

        if( aoLights[i].iLightType == 1 || aoLights[i].iLightType == 2 ) 
        {
            // point/spot light
            vLightDir = normalize( aoLights[i].vPos.xyz - vPos );

            // apply attenuation
            float d = distance( vPos, aoLights[i].vPos.xyz ); 
            LightForce = 1. / ( 
                aoLights[i].vAttenuation[0] +           // constant
                aoLights[i].vAttenuation[1] * d +       // linear
                aoLights[i].vAttenuation[2] * d * d );  // quadratic

            if( aoLights[i].iLightType == 1 )
            {
                // spot light: check if in light bounds and apply dimming at edges

                float theta = -dot(vLightDir, aoLights[i].vDir.xyz);
    
                if(theta < aoLights[i].vCutoff.y) continue;

                float epsilon   = aoLights[i].vCutoff.x - aoLights[i].vCutoff.y;
                float intensity = max((theta - aoLights[i].vCutoff.y) / epsilon, 0);

                LightForce *= intensity;
            }
        }
        else
        {
            // directional light
            vLightDir = -aoLights[i].vDir.xyz;
        }
        
        
        if( aoLights[i].iShadowMapId != -1 )
        {
            int id = aoLights[i].iShadowMapId;
            vec3 lightdir2;
            switch( aoLights[i].iLightType )
            {
                case 0: case 1: lightdir2 = vLightDir; break;
                case 2: 
                {
                    vec3 k = abs(vLightDir);
                    lightdir2 = 
                    ( k.z > k.y && k.z > k.x ) ? vec3( 0, 0, sign(vLightDir.z) ) :
                    ( k.y > k.z && k.y > k.x ) ? vec3( 0, sign(vLightDir.y), 0 ) :
                                                 vec3( sign(vLightDir.x), 0, 0 );
                }
            }
                
            //if( i == 1 ) res.vDif += dot(vNorm, lightdir2);//dot(vNorm, lightdir2)/2;

            // we use floating point depth textures so bias should be multiplicative
            float bias = min( 1 / abs(dot(vNorm, lightdir2)), 1.02 );// max(1.0 - abs(dot(vNorm, lightdir2)), 0.1);// * 0.0005;

            switch( aoLights[i].iLightType )
            {
                case 0: case 1:
                    LightForce *= GetShadowStrength( id, vPos, bias ); 
                    break;
                case 2:
                    LightForce *= GetShadowStrength_Pointlight( id, vPos, bias, aoLights[i].vPos.xyz ); 
                    break;
            }

        }

        res.vDif += max(LightForce * aoLights[i].vColDif.rgb * max(dot(vNorm, vLightDir),0),0);

        if( mat.vColSpec.a > 0 )
        {
            float spec;
            
            //switch( mat.iShadingModel )
            //{
            //case ShadingFlat: default:
            //    spec = 0;
            //    break;
            //
            //case ShadingPhong: 
            //    vec3 vReflectDir = reflect(-vLightDir, vNorm);
            //    spec = pow(max(dot(vViewDir, vReflectDir), 0.0), mat.fShininess);
            //    break;
            //
            //case ShadingBlin:
            //
                vec3 vHalfwayDir = normalize(vLightDir + vViewDir); 
                spec = pow(max(dot(vNorm, vHalfwayDir), 0.0), mat.vColSpec.a);
            //    break;
            //}

            res.vSpec += spec * LightForce * fSpecularEnergyConservation * aoLights[i].vColSpec.rgb;
        }
    }

    if( bUseToonShading )
    {
        res.vAmb = round(res.vAmb*uToonQuants)/uToonQuants;
        res.vDif = round(res.vDif*uToonQuants)/uToonQuants;
        res.vSpec = round(res.vSpec*uToonQuants)/uToonQuants;
    }

    vec4 dif = SampleTexture( mat.iTexDiffuse, vTexUV );//texture( ahMaterialTextures[mat.iTexDiffuse], vTexUV );
    vec3 spec = SampleTexture( mat.iTexSpecular, vTexUV ).rgb;//texture( ahMaterialTextures[mat.iTexSpecular], vTexUV ).rgb;

    res.vAmb *= mat.vColAmb.rgb * dif.rgb;
    res.vDif *= mat.vColDif.rgb * dif.rgb;
    res.vSpec *= mat.vColSpec.rgb * spec;
    res.fAlpha = mat.vColDif.a * dif.a;
    
    return res;
}

vec3 GetNormal( MaterialSpec mat, vec3 Norm, vec3 Tan, vec2 vTexUV )
{
    vec3 N = normalize( Norm );
    vec3 T = normalize( Tan - N * dot(N, Tan) );
    vec3 B = cross( T, N );

    return mat3( T, B, N ) * ( SampleTexture( mat.iTexNormal, vTexUV ).rgb * 2 - 1 );
}

//const vec3 vFogColor = vec3(0.25);

void main()
{
    //vFragColor = vec4( fs_in.vNorm, 1 ); return;
    MaterialSpec material = oMaterials[fs_in.vModelPartIdx.y];

    vec3 vNorm = GetNormal( material, fs_in.vNorm, fs_in.vTang, fs_in.vTex );    
    vec3 vViewDir = normalize( vViewWorldPos.xyz - fs_in.vWorldPos );

    //float fViewDist = distance( vViewWorldPos, fs_in.vWorldPos );

    //vFragColor = vec4( vec3( dot( vViewDir, vNorm ) ), 1 ); return;
    ColorSpec cs = CalculateLigtning( material, fs_in.vWorldPos, vNorm, vViewDir, fs_in.vTex );

    vFragColor = vec4( ( cs.vAmb + cs.vDif ) * fs_in.vCol.rgb + cs.vSpec, cs.fAlpha * fs_in.vCol.a );

    if( vFragColor.a < 0.5 ) discard;
    vFragColor.a = 1;
    //vFragColor = clamp( vFragColor, 0, 1 );
    //vFragNorm = ( mat3(mViewIT) * vNorm + 1 ) * 0.5;
    //vFragColor.xyz = mix( vFragColor.xyz, vFogColor, 1-1/(1+0.1*fViewDist+0.001*fViewDist*fViewDist) );
    //if( vFragColor.a < 0.5 ) { discard; }
}
