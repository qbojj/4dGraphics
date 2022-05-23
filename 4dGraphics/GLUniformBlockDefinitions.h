#pragma once

static_assert(std::endian::native == std::endian::little);


#include "shared/GlmHeaders.h"
namespace glsl 
{
	using namespace glm; 
#pragma warning( push )
#pragma warning( disable: 4200 )

#include "../../data/shaders/include/CommonCppGLSL.glsl"

#pragma warning( pop )
}; // namespace glsl