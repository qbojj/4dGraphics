#ifndef MATERIALS_GLSL_
#define MATERIALS_GLSL_

#include "CommonCppGLSL.h"

#define MAX_MATERIAL_TEXTURES (gl_MaxTextureImageUnits-2)

#if defined(GL_ARB_bindless_texture) && !FOR_RENDERDOC
vec4 SampleTexture( in uvec2 tex, in vec2 pos ) { return texture( sampler2D(tex), pos ); }//{ return texture( sampler2D( uvec2( ( tex >> 32 ) & 0xFFFF, tex & 0xFFFF ) ), pos ); }
#else
layout(binding=0) uniform sampler2D ahMaterialTextures[MAX_MATERIAL_TEXTURES];
vec4 SampleTexture( in uvec2 tex, in vec2 pos ) { return tex.x >= uint(MAX_MATERIAL_TEXTURES) ? vec4(1) : texture( ahMaterialTextures[ tex.x ], pos ); }
#endif

#endif // MATERIALS_GLSL_