#ifndef LIGHTS_GLSL_
#define LIGHTS_GLSL_

#include "CommonCppGLSL.h"

layout(binding=gl_MaxTextureImageUnits-2) uniform sampler2DArrayShadow ShadowMaps;
layout(binding=gl_MaxTextureImageUnits-1) uniform samplerCubeArrayShadow ShadowPointMaps;

#endif // LIGHTS_GLSL_