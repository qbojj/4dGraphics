#pragma once

static_assert(std::endian::native == std::endian::little);


#include "GlmHeaders.h"
namespace glsl 
{
	using namespace glm; 
	#include "CommonCppGLSL.h"
} // namespace glsl