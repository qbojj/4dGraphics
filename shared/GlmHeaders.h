#pragma once
#ifndef _GLM_HEADERS_H_
#define _GLM_HEADERS_H_

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
//#define GLM_FORCE_INTRINSICS //Oh... Did you see that? It was FAST.
//#define GLM_FORCE_INLINE
//#define GLM_FORCE_ALIGNED_GENTYPES
//#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_SWIZZLE
#include <glm/glm.hpp>
#include <glm/ext.hpp>
//#include <glm/gtc/quaternion.hpp>
//#include <glm/gtc/constants.hpp>
//#include <glm/gtc/integer.hpp>
//#include <glm/gtc/matrix_inverse.hpp>
//#include <glm/gtc/matrix_transform.hpp>
//#include <glm/gtc/packing.hpp>
//#include <glm/gtc/type_aligned.hpp>
//#include <glm/gtc/type_precision.hpp>
//#include <glm/gtc/type_ptr.hpp>
//#include <glm/gtc/random.hpp>

/*
#define __GLID__DEF_ALIGN_PACK( t )	\
using t ## a = aligned_ ## t;		\
using t ## p = packed_ ## t;

#define __GLID__DEF_ALIGN_PACK_VEC( ty )	\
__GLID__DEF_ALIGN_PACK( ty ## 1 )			\
__GLID__DEF_ALIGN_PACK( ty ## 2 )			\
__GLID__DEF_ALIGN_PACK( ty ## 3 )			\
__GLID__DEF_ALIGN_PACK( ty ## 4 )

#define __GLID__DEF_ALIGN_PACK_MAT( ty )	\
__GLID__DEF_ALIGN_PACK( ty ## 2x2 )			\
__GLID__DEF_ALIGN_PACK( ty ## 2x3 )			\
__GLID__DEF_ALIGN_PACK( ty ## 2x4 )			\
__GLID__DEF_ALIGN_PACK( ty ## 3x2 )			\
__GLID__DEF_ALIGN_PACK( ty ## 3x3 )			\
__GLID__DEF_ALIGN_PACK( ty ## 3x4 )			\
__GLID__DEF_ALIGN_PACK( ty ## 4x2 )			\
__GLID__DEF_ALIGN_PACK( ty ## 4x3 )			\
__GLID__DEF_ALIGN_PACK( ty ## 4x4 )			\
__GLID__DEF_ALIGN_PACK( ty ## 2 )			\
__GLID__DEF_ALIGN_PACK( ty ## 3 )			\
__GLID__DEF_ALIGN_PACK( ty ## 4 )

namespace glm
{
	__GLID__DEF_ALIGN_PACK_VEC( vec );
	__GLID__DEF_ALIGN_PACK_MAT( mat );

	__GLID__DEF_ALIGN_PACK_VEC( dvec );
	__GLID__DEF_ALIGN_PACK_MAT( dmat );

	__GLID__DEF_ALIGN_PACK_VEC( uvec );
	__GLID__DEF_ALIGN_PACK_VEC( ivec );

	__GLID__DEF_ALIGN_PACK_VEC( bvec );

	template< typename T > using tvec1a = tvec1<T, aligned_highp>;
	template< typename T > using tvec1p = tvec1<T, packed_highp>;

	template< typename T > using tvec2a = tvec2<T, aligned_highp>;
	template< typename T > using tvec2p = tvec2<T, packed_highp>;

	template< typename T > using tvec3a = tvec3<T, aligned_highp>;
	template< typename T > using tvec3p = tvec3<T, packed_highp>;

	template< typename T > using tvec4a = tvec4<T, aligned_highp>;
	template< typename T > using tvec4p = tvec4<T, packed_highp>;
};

#undef __GLID__DEF_ALIGN_PACK_MAT
#undef __GLID__DEF_ALIGN_PACK_VEC
#undef __GLID__DEF_ALIGN_PACK

*/

#endif