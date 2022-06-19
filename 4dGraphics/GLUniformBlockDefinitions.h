#pragma once

static_assert(std::endian::native == std::endian::little);


#include "GlmHeaders.h"
namespace glsl 
{
	using namespace glm; 
#pragma warning( push )
#pragma warning( disable: 4200 )

#include "CommonCppGLSL.h"

#pragma warning( pop )
}; // namespace glsl