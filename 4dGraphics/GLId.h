#pragma once

//#pragma warning(push)
//#pragma warning( disable: 4251 )
//#include <glbinding/gl46core/gl.h>
//#pragma warning(pop)
//using namespace gl;
//using namespace gl46core;
#include "glad/gl.h"
#include <stdarg.h>

#include "Debug.h"
#include "CommonUtility.h"

struct GLParameters_t
{
	GLint
		MaxLabelLength,		// GL_MAX_LABEL_LENGTH
		UniformAlignment,	// GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT
		SSBOAlignment,		// GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT
		MaxAnisotropy,		// GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
		MaxFragmentImageUnits;//GL_MAX_TEXTURE_IMAGE_UNITS

	void Query();
};

//struct GLExtensions_t
//{
//	std::bitset<1024> avaiable;
//	using reference = std::bitset<1024>::reference;
//
//	void Query();
//	reference operator[]( unsigned int e ) { return avaiable[e]; }
//	reference operator[]( GLextension e ) { return avaiable[(unsigned int)e]; }
//};

extern GLParameters_t GLParameters;
//extern GLExtensions_t GLExtensions;

#define EXT_AVAIABLE( ext ) ( GLAD_ ## ext )//GLExtensions[ GLextension:: ## ext ]

void SetLabelAnyv( GLenum identifier, const void *id, const char *fmt, va_list va );
void SetLabelAny( GLenum identifier, const void *id, const char *fmt, ... );
const char *GetLabelAny( GLenum identifier, const void *id );

// simple GL name container
// you are only able to move (copy constructors/assign unavailable)
template< typename GLOBJInfo >  // doesn't add any size and makes sure that Id types don't mix
struct GLId
{
	GLuint id;

	GLId() : id( 0 ) {};
	GLId( GLuint &&id ) : id( id ) { id = 0; };

	GLId( const GLId & ) = delete;
	GLId &operator=( const GLId & ) = delete;

	GLId( GLId &&o ) : id( o.id ) { o.id = 0; };
	GLId &operator=( GLId &&o ) { if( id != o.id ) { clear(); id = o.id; o.id = 0; } return *this; }

	~GLId() { clear(); }

	operator const GLuint &() const { return id; }
	operator GLuint &() { return id; }

	const GLuint *operator &() const { return &id; }
	GLuint *operator &() { return &id; }

	void SetLabel( const char *fmt, ... )
	{
		if( id )
		{
			va_list va;
			va_start( va, fmt );
			SetLabelAnyv( GLOBJInfo::identifier(), &id, fmt, va );
			va_end( va );
		}
	}

	const char *GetLabel() { return GetLabelAny( GLOBJInfo::identifier(), &id ); }

	void clear()
	{
		if( id )
		{
			if( LogLevel <= DebugLevel::Debug )
			{
				const char *label = GetLabel();

				if( label[0] != '\0' )	TRACE( DebugLevel::Debug, "deleting %s\n", label );
				else					TRACE( DebugLevel::Debug, "deleting %s\n", GLOBJInfo::name() );
			}
			GLOBJInfo::del( id );
		}
		id = 0;
	}
};

#define __GLID__CREATE_ID_DESCR(TypeName, Del, dbgName, id)	\
struct TypeName																	\
{ 																				\
	inline static void del( GLuint v ) { Del ; };								\
	constexpr inline static const char *name() { return dbgName; }				\
	constexpr inline static GLenum identifier() { return id; }					\
}

#define __GLID__DEFID_SIZ_ID(sId, sNm, dNm, dID)									\
__GLID__CREATE_ID_DESCR(GLIdTypeInfo_ ## sId, glDelete ## sNm ( 1, &v ), dNm, dID);	\
using GL ## sId ## Id = GLId<GLIdTypeInfo_ ## sId>;

#define __GLID__DEFID_ID(sId, sNm, dNm, dID)										\
__GLID__CREATE_ID_DESCR(GLIdTypeInfo_ ## sId, glDelete ## sNm ( v ), dNm, dID);	\
using GL ## sId ## Id = GLId<GLIdTypeInfo_ ## sId>;

__GLID__DEFID_SIZ_ID( Tex, Textures, "texture", GL_TEXTURE )
__GLID__DEFID_SIZ_ID( Renderbuffer, Renderbuffers, "render buffer", GL_RENDERBUFFER )
__GLID__DEFID_SIZ_ID( VAO, VertexArrays, "vertex array", GL_VERTEX_ARRAY )
__GLID__DEFID_SIZ_ID( Buf, Buffers, "buffer", GL_BUFFER )
__GLID__DEFID_SIZ_ID( FBO, Framebuffers, "Framebuffer", GL_FRAMEBUFFER )
__GLID__DEFID_SIZ_ID( Pipeline, ProgramPipelines, "pipeline", GL_PROGRAM_PIPELINE )
__GLID__DEFID_ID( Shader, Shader, "shader", GL_SHADER )
__GLID__DEFID_ID( Program, Program, "program", GL_PROGRAM )

#undef __GLID__DEFID_SIZ_ID
#undef __GLID__DEFID_ID
#undef __GLID__CREATE_DELETER
