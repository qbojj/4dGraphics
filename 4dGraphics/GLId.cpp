#include "stdafx.h"
#include "GLId.h"

GLParameters_t GLParameters;
//GLExtensions_t GLExtensions;

void GLParameters_t::Query()
{
	glGetIntegerv( GL_MAX_LABEL_LENGTH, &MaxLabelLength );
	glGetIntegerv( GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &UniformAlignment );
	glGetIntegerv( GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT, &SSBOAlignment );
	glGetIntegerv( GL_MAX_TEXTURE_IMAGE_UNITS, &MaxFragmentImageUnits );
	//if( EXT_AVAIABLE( GL_EXT_texture_filter_anisotropic ) ||
	//	EXT_AVAIABLE( GL_ARB_texture_filter_anisotropic ) )
	glGetIntegerv( GL_MAX_TEXTURE_MAX_ANISOTROPY /* == *ARB */, &MaxAnisotropy);
	//else MaxAnisotropy = 1;
}
/*
void GLExtensions_t::Query()
{
	avaiable.reset();
	for( GLextension e : glbinding::aux::ContextInfo::extensions() )
		if( (unsigned int)e < (unsigned int)avaiable.size() )
			avaiable.set( (unsigned int)e );
}
*/
static std::unique_ptr<GLchar[]> LabelStr = NULL;

static bool IsPointerObject( GLenum identifier )
{
	return identifier == GL_SYNC_FENCE;
}

void SetLabelAnyv( GLenum identifier, const void *id, const char *fmt, va_list va )
{
	//if( EXT_AVAIABLE( GL_KHR_debug ) )
	//{
		if( !LabelStr ) LabelStr = std::make_unique<GLchar[]>( GLParameters.MaxLabelLength );

		int si = stbsp_vsnprintf( LabelStr.get(), GLParameters.MaxLabelLength, fmt, va );

		if( IsPointerObject( identifier ) )
			glObjectPtrLabel( *(void **)id,
				std::min( GLParameters.MaxLabelLength, si ), LabelStr.get() );
		else
			glObjectLabel( identifier, *(GLuint *)id,
				std::min( GLParameters.MaxLabelLength, si ), LabelStr.get() );
	//}
}

void SetLabelAny( GLenum identifier, const void *id, const char *fmt, ... )
{
	va_list va;
	va_start( va, fmt );
	SetLabelAnyv( identifier, id, fmt, va );
	va_end( va );
}

const char *GetLabelAny( GLenum identifier, const void *id )
{
	//if( EXT_AVAIABLE( GL_KHR_debug ) )
	//{
		if( !LabelStr ) LabelStr = std::make_unique<GLchar[]>( GLParameters.MaxLabelLength );

		LabelStr[0] = 0;

		if( IsPointerObject( identifier ) )
			glGetObjectPtrLabel( *(void **)id, GLParameters.MaxLabelLength, NULL, LabelStr.get() );
		else
			glGetObjectLabel( identifier, *(GLuint *)id, GLParameters.MaxLabelLength, NULL, LabelStr.get() );

		return LabelStr.get();
	//}
	//return "";
}