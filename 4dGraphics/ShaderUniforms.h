#pragma once

#include "GLId.h"

namespace uniformDetail
{
	template< typename T >
	inline void SetUniform( GLint location, const T &value ) = delete;
	template< typename T >
	inline void SetUniformDSA( GLuint program, GLint location, const T &value ) = delete;

	// c - types suffix; suf - extension suffix
#define SET_UNIFORM_MATRIX_T( col, row, T, c, suf ) \
template<> inline void SetUniform( GLint loc, const glm::mat<col,row,T, glm::packed_highp> &v )\
{ glUniformMatrix ## col ## x ## row ## c ## v ## suf( loc, 1, GL_FALSE, glm::value_ptr( v ) ); }\
template<> inline void SetUniformDSA( GLuint prog, GLint loc, const glm::mat<col,row,T, glm::packed_highp> &v )\
{ glProgramUniformMatrix ## col ## x ## row ## c ## v ## suf( prog, loc, 1, GL_FALSE, glm::value_ptr( v ) ); }

#define SET_UNIFORM_SQUARE_MATRIX_T( ext, T, c, suf ) \
template<> inline void SetUniform( GLint loc, const glm::mat<ext,ext,T, glm::packed_highp> &v )\
{ glUniformMatrix ## ext ## c ## v ## suf( loc, 1, GL_FALSE, glm::value_ptr( v ) ); }\
template<> inline void SetUniformDSA( GLuint prog, GLint loc, const glm::mat<ext, ext, T, glm::packed_highp> &v )\
{ glProgramUniformMatrix ## ext ## c ## v ## suf( prog, loc, 1, GL_FALSE, glm::value_ptr( v ) ); }

#define SET_UNIFORM_VECTOR_T( L, T, c, suf ) \
template<> inline void SetUniform( GLint loc, const glm::vec<L,T,glm::packed_highp> &v )\
{ glUniform ## L ## c ## v ## suf( loc, 1, glm::value_ptr(v) ); }\
template<> inline void SetUniformDSA( GLuint prog, GLint loc, const glm::vec<L,T,glm::packed_highp> &v )\
{ glProgramUniform ## L ## c ## v ## suf( prog, loc, 1, glm::value_ptr(v) ); }

#define SET_UNIFORM_SCALAR_T( T, c, suf ) \
template<> inline void SetUniform( GLint loc, const T &v ) \
{ glUniform1 ## c ## suf ( loc, v ); }\
template<> inline void SetUniformDSA( GLuint prog, GLint loc, const T &v ) \
{ glProgramUniform1 ## c ## suf ( prog, loc, v ); }


#define SET_UNIFORMS_FOR_FP_TYPE_T( T, c, suf ) \
SET_UNIFORM_SQUARE_MATRIX_T( 2, T, c, suf ) \
SET_UNIFORM_SQUARE_MATRIX_T( 3, T, c, suf ) \
SET_UNIFORM_SQUARE_MATRIX_T( 4, T, c, suf ) \
SET_UNIFORM_VECTOR_T( 4, T, c, suf )        \
SET_UNIFORM_VECTOR_T( 3, T, c, suf )        \
SET_UNIFORM_VECTOR_T( 2, T, c, suf )        \
SET_UNIFORM_SCALAR_T( T, c, suf )           \
SET_UNIFORM_MATRIX_T( 4, 2, T, c, suf )     \
SET_UNIFORM_MATRIX_T( 3, 4, T, c, suf )     \
SET_UNIFORM_MATRIX_T( 4, 3, T, c, suf )     \
SET_UNIFORM_MATRIX_T( 2, 3, T, c, suf )     \
SET_UNIFORM_MATRIX_T( 2, 4, T, c, suf )     \
SET_UNIFORM_MATRIX_T( 3, 2, T, c, suf )     


#define SET_UNIFORMS_FOR_INT_TYPE_T( T, c, suf ) \
SET_UNIFORM_VECTOR_T( 4, T, c, suf ) \
SET_UNIFORM_VECTOR_T( 3, T, c, suf ) \
SET_UNIFORM_VECTOR_T( 2, T, c, suf ) \
SET_UNIFORM_SCALAR_T( T, c, suf )

	SET_UNIFORMS_FOR_FP_TYPE_T( float, f, );
	SET_UNIFORMS_FOR_FP_TYPE_T( double, d, );
	SET_UNIFORMS_FOR_INT_TYPE_T( int32_t, i, );
	//SET_UNIFORMS_FOR_INT_TYPE_T( int64_t, i64, ARB );
	SET_UNIFORMS_FOR_INT_TYPE_T( uint32_t, ui, );
	//SET_UNIFORMS_FOR_INT_TYPE_T( uint64_t, ui64, ARB );
	
	struct GLBindlessTextureHandle {
		unsigned long long handle;
		operator unsigned long long &() { return handle; }
		operator const unsigned long long &() const { return handle; }
	};
	template<> inline void SetUniform( GLint loc, const GLBindlessTextureHandle &v ) { glUniformHandleui64ARB( loc, v.handle ); }
	template<> inline void SetUniformDSA( GLuint prog, GLint loc, const GLBindlessTextureHandle &v ) { glProgramUniformHandleui64ARB( prog, loc, v.handle ); }

	template<int L> 
	inline void SetUniform( GLint loc, const glm::vec<L, bool, glm::packed_highp> &v )
	{ 
		SetUniform( loc, vec<L, int, glm::packed_highp>( v ) );
	}
	template<> inline void SetUniform( GLint loc, const bool &v ) { SetUniform( loc, (int)v ); }

	template<int L> inline void SetUniformDSA( GLuint prog, GLint loc, const glm::vec<L, bool, glm::packed_highp> &v )
	{
		SetUniformDSA( prog, loc, vec<L, int, glm::packed_highp>( v ) );
	}
	template<> inline void SetUniformDSA( GLuint prog, GLint loc, const bool &v ) { SetUniformDSA( prog, loc, (int)v ); }

	template<int L, typename T, glm::qualifier Q, typename=std::enable_if_t<Q != glm::packed_highp> >
	inline void SetUniform( GLint loc, const glm::vec<L, T, Q> &v ) { SetUniform( loc, glm::vec<L, T, glm::packed_highp>( v ) ); }
	template<int C, int R, typename T, glm::qualifier Q, typename=std::enable_if_t<Q != glm::packed_highp> >
	inline void SetUniform( GLint loc, const glm::mat<C, R, T, Q> &v ) { SetUniform( loc, glm::mat<C, R, T, glm::packed_highp>( v ) ); }

	template<int L, typename T, glm::qualifier Q, typename=std::enable_if_t<Q != glm::packed_highp> >
	inline void SetUniformDSA( GLuint prog, GLint loc, const glm::vec<L, T, Q> &v ) { SetUniformDSA( prog, loc, glm::vec<L, T, glm::packed_highp>( v ) ); }
	template<int C, int R, typename T, glm::qualifier Q, typename = std::enable_if_t<Q != glm::packed_highp> >
	inline void SetUniformDSA( GLuint prog, GLint loc, const glm::mat<C, R, T, Q> &v ) { SetUniformDSA( prog, loc, glm::mat<C, R, T, glm::packed_highp>( v ) ); }

#undef SET_UNIFORMS_FOR_FP_TYPE_T
#undef SET_UNIFORMS_FOR_INT_TYPE_T
#undef SET_UNIFORM_MATRIX_T
#undef SET_UNIFORM_SQUARE_MATRIX_T
#undef SET_UNIFORM_VECTOR_T
#undef SET_UNIFORM_SCALAR_T
}

class GLUniform
{
protected:
	GLint location;
public:

	GLUniform() : location( -1 ) {};
	GLUniform( GLuint prog, const char *name ) { Register( prog, name ); }
	void Register( GLuint prog, const char *name ) { location = glGetUniformLocation( prog, name ); }
	template<typename T> void Set( const T &v ) { uniformDetail::SetUniform( location, v ); }
	template<typename T> void SetDSA( GLuint prog, const T &v ) { uniformDetail::SetUniformDSA( prog, location, v ); }

	GLint GetLocation() const { return location; }
};

class GLUniformDSA
{
protected:
	GLuint program;
	GLint location;
public:
	GLUniformDSA() : program( 0 ), location( -1 ) {};
	GLUniformDSA( GLuint prog, const char *name ) { Register( prog, name ); }
	GLUniformDSA( GLuint prog, GLuint loc ) : program(prog), location(loc) {}
	void Register( GLuint prog, const char *name ) { program = prog, location = glGetUniformLocation( program, name ); }

	template<typename T> void Set( const T &v ) { uniformDetail::SetUniformDSA( program, location, v ); }

	GLuint GetProgram() const { return program; }
	GLint GetLocation() const { return location; }
};
