#include <gli/gli.hpp>
#include "stdafx.h"

#include "optick.h"
#include "TextureLoad.h"
#include "GLId.h"
#include "Debug.h"
#include "Objects.h"
#include "BufferAlloc.h"
#include <stb_image.h>

using namespace std;
static GLTexId CreateTextureFromFile_gli( const char *Filename )
{
	OPTICK_EVENT();

	gli::texture Texture = gli::load( Filename );

	if( Texture.empty() ) return 0;

	gli::gl GL( gli::gl::PROFILE_GL33 );
	gli::gl::format const Format = GL.translate( Texture.format(), Texture.swizzles() );

	GLTexId tex;
	glCreateTextures( (GLenum)GL.translate( Texture.target() ), 1, &tex );

	glTextureParameteri( tex, GL_TEXTURE_BASE_LEVEL, (GLint)Texture.base_level() );
	glTextureParameteri( tex, GL_TEXTURE_MAX_LEVEL, static_cast<GLint>( Texture.levels() - 1 ) );
	glTextureParameteriv( tex, GL_TEXTURE_SWIZZLE_RGBA, &Format.Swizzles[0] );

	glm::tvec3<GLsizei> const Extent( Texture.extent() );
	GLsizei const FaceTotal = static_cast<GLsizei>( Texture.layers() * Texture.faces() );

	switch( Texture.target() )
	{
	case gli::TARGET_1D:
		glTextureStorage1D(
			tex, static_cast<GLint>( Texture.levels() ), (GLenum)Format.Internal, Extent.x );
		break;
	case gli::TARGET_1D_ARRAY:
	case gli::TARGET_2D:
	case gli::TARGET_CUBE:
		glTextureStorage2D(
			tex, static_cast<GLint>( Texture.levels() ), (GLenum)Format.Internal,
			Extent.x, Texture.target() == gli::TARGET_2D ? Extent.y : FaceTotal );
		break;
	case gli::TARGET_2D_ARRAY:
	case gli::TARGET_3D:
	case gli::TARGET_CUBE_ARRAY:
		glTextureStorage3D(
			tex, static_cast<GLint>( Texture.levels() ), (GLenum)Format.Internal,
			Extent.x, Extent.y,
			Texture.target() == gli::TARGET_3D ? Extent.z : FaceTotal );
		break;
	default:
		assert( 0 );
		break;
	}

	for( size_t Layer = 0; Layer < Texture.layers(); ++Layer )
		for( size_t Face = 0; Face < Texture.faces(); ++Face )
			for( size_t Level = 0; Level < Texture.levels(); ++Level )
			{
				GLsizei const LayerGL = static_cast<GLsizei>( Layer );
				glm::tvec3<GLsizei> Extent( Texture.extent( Level ) );

				GLsizei CubeZOff = (GLsizei)( gli::is_target_cube( Texture.target() ) ?
											  Face + Layer * 6 : 0 );

				GLsizei siz = static_cast<GLsizei>( Texture.size( Level ) );
				void *data = Texture.data( Layer, Face, Level );

				switch( Texture.target() )
				{
				case gli::TARGET_1D:
					if( gli::is_compressed( Texture.format() ) )
						glCompressedTextureSubImage1D(
							tex, static_cast<GLint>( Level ), 0, Extent.x,
							(GLenum)Format.Internal, siz,
							data );
					else
						glTextureSubImage1D(
							tex, static_cast<GLint>( Level ), 0, Extent.x,
							(GLenum)Format.External, (GLenum)Format.Type,
							data );
					break;
				case gli::TARGET_1D_ARRAY:
				case gli::TARGET_2D:
					if( gli::is_compressed( Texture.format() ) )
						glCompressedTextureSubImage2D(
							tex, static_cast<GLint>( Level ),
							0, 0,
							Extent.x,
							Texture.target() == gli::TARGET_1D_ARRAY ? LayerGL : Extent.y,
							(GLenum)Format.Internal, siz,
							data );
					else
						glTextureSubImage2D(
							tex, static_cast<GLint>( Level ),
							0, 0,
							Extent.x,
							Texture.target() == gli::TARGET_1D_ARRAY ? LayerGL : Extent.y,
							(GLenum)Format.External, (GLenum)Format.Type,
							data );
					break;
				case gli::TARGET_2D_ARRAY:
				case gli::TARGET_3D:
				case gli::TARGET_CUBE:
				case gli::TARGET_CUBE_ARRAY:
					if( gli::is_compressed( Texture.format() ) )
						glCompressedTextureSubImage3D(
							tex, static_cast<GLint>( Level ),
							0, 0, CubeZOff,
							Extent.x, Extent.y,
							Texture.target() == gli::TARGET_3D ? Extent.z : LayerGL,
							(GLenum)Format.Internal, siz,
							data );
					else
						glTextureSubImage3D(
							tex, static_cast<GLint>( Level ),
							0, 0, CubeZOff,
							Extent.x, Extent.y,
							Texture.target() == gli::TARGET_3D ? Extent.z : LayerGL,
							(GLenum)Format.External, (GLenum)Format.Type,
							data );
					break;
				default: assert( 0 ); break;
				}
			}

	return tex;
}

class Texture
{
private:
	bool IsSTBAllocated = false;
public:
	void *pData;
	int width, height;
	int channels;
	GLenum type; // GL_UNSIGNED_BYTE/GL_UNSIGNED_SHORT/GL_FLOAT

	Texture();
	Texture( int w, int h, GLenum type = GL_UNSIGNED_BYTE, int channels = 4 );
	Texture( const char *file, bool swapY );
	Texture( const Texture & ) = delete;
	Texture &operator =( const Texture & ) = delete;

	~Texture() { clear(); };
	//Texture( Texture && );
	//Texture &operator =( Texture && );

	//glm::vec4 GetPixel( int x, int y ) const;
	//glm::vec4 GetPixel( const glm::ivec2 &v ) const;
	//void  SetPixel( int x, int y, glm::vec4 c );
	//void  SetPixel( const glm::ivec2 &v, glm::vec4 c );

	int GetTypeSize() const;
	int GetDataSize() const;

	struct GetFormatRes
	{
		GLenum internalFormat;
		GLenum format;
		GLint swizzle[4];
	};
	GetFormatRes GetFormat( TextureCompressionModel compr ) const;

	void LoadFromFile( const char *file, bool swapY );
	void MakeDefault( const glm::u8 def[4] );

	void clear();
};
/*
glm::vec4 Texture::GetPixel( int x, int y ) const
{
	if( pData == nullptr || x < 0 || y < 0 || x >= width || y >= height ) return BLANK;

	if( type == GL_UNSIGNED_BYTE )
	{
		return glm::vec4( ( ( glm::tvec4p<glm::u8> * )pData )[width * y + x] ) /
			(float)std::numeric_limits<glm::u8>::max();
	}
	else if( type == GL_UNSIGNED_SHORT )
	{
		return glm::vec4( ( ( glm::tvec4p<glm::u16> * )pData )[width * y + x] ) /
			(float)std::numeric_limits<glm::u16>::max();
	}
	else if( type == GL_FLOAT )
	{
		return ( ( glm::tvec4p<glm::f32> * )pData )[width * y + x] / 1.f;
	}

	return BLANK;
}

glm::vec4 Texture::GetPixel( const glm::ivec2 &v ) const { return GetPixel( v.x, v.y ); }

void Texture::SetPixel( int x, int y, glm::vec4 c )
{
	if( pData != nullptr && x >= 0 && y >= 0 && x < width && y < height )
	{
		c = glm::clamp( c, 0.f, 1.f );

		//pData[width * y + x] = c;
		if( type == GL_UNSIGNED_BYTE )
		{
			( ( glm::tvec4p<glm::u8> * )pData )[width * y + x] =
				c * (float)std::numeric_limits<glm::u8>::max();
		}
		else if( type == GL_UNSIGNED_SHORT )
		{
			( ( glm::tvec4p<glm::u16> * )pData )[width * y + x] =
				c * (float)std::numeric_limits<glm::u16>::max();
		}
		else if( type == GL_FLOAT )
		{
			( ( glm::tvec4p<glm::f32> * )pData )[width * y + x] =
				c * 1.f;
		}
	}
}

void Texture::SetPixel( const glm::ivec2 &v, glm::vec4 c ) { SetPixel( v.x, v.y, c ); }
*/
int Texture::GetTypeSize() const { return type == GL_UNSIGNED_BYTE ? 1 : type == GL_UNSIGNED_SHORT ? 2 : type == GL_FLOAT ? 4 : 0; }
int Texture::GetDataSize() const { return GetTypeSize() * width * height * channels; }

Texture::GetFormatRes Texture::GetFormat( TextureCompressionModel compr ) const
{
	assert( width != 0 && height != 0 );
	assert( channels > 0 && channels <= 4 );
	assert( type == GL_UNSIGNED_BYTE || type == GL_UNSIGNED_SHORT || type == GL_FLOAT );
	assert( compr >= TextureCompressionModel::NoCompression && compr <= TextureCompressionModel::Strong );

	int ty =
		( (type == GL_UNSIGNED_BYTE) << 0 ) |
		( (type == GL_UNSIGNED_SHORT) << 1 ) |
		( (type == GL_FLOAT) << 2 );
	int t = ty == 1 ? 0 : ty == 2 ? 1 : 2;
	int c = channels - 1;

	constexpr GLenum InternalFormats[4][4][3] =
	{
		// CompressionNoCompression:
		{
			GL_R8, GL_R16, GL_R32F,
			GL_RG8, GL_RG16, GL_RG32F,
			GL_RGB8, GL_RGB16, GL_RGB32F,
			GL_RGBA8, GL_RGBA16, GL_RGBA32F
		},
		// CompressionWeak:
		{
			GL_R8, GL_R16, GL_R16F,
			GL_RG8, GL_RG16, GL_RG16F,
			GL_RGB5, GL_RGB10, GL_RGB16F,
			GL_RGB5_A1, GL_RGB10_A2, GL_RGBA16F
		},
		// CompressionMedium:
		{
			GL_R8, GL_R16, GL_R16F,
			GL_RG8, GL_RG16, GL_RG16F,
			GL_R3_G3_B2, GL_RGB5, GL_RGB16F,
			GL_RGBA4, GL_RGB5_A1, GL_R11F_G11F_B10F
		},
		// CompressionStrong:
		{
			GL_R8, GL_R16, GL_R16F,
			GL_RG8, GL_RG16, GL_RG16F,
			GL_R3_G3_B2, GL_RGB5, GL_RGB16F,
			GL_RGBA4, GL_RGB5_A1, GL_R11F_G11F_B10F
		},
	};

	constexpr GLenum formats[4] = { GL_RED, GL_RG, GL_RGB, GL_RGBA };

	Texture::GetFormatRes r{
		.internalFormat = InternalFormats[(int)compr][c][t],
		.format = formats[c],

		// chanels:
		// 1 : L (RRR1)
		// 2 : LA (RRRG)
		// 3 : RGB (RGB1)
		// 4 : RGBA (RGBA)
		.swizzle = {
			GL_RED,
			channels <= 2 ? GL_RED : GL_GREEN,
			channels <= 2 ? GL_RED : GL_BLUE,
			channels == 1 || channels == 3 ? GL_ONE : channels == 2 ? GL_GREEN : GL_ALPHA
		},
	};

	return r;
}

Texture::Texture() { width = height = channels = 0; pData = NULL; type = GL_NONE; IsSTBAllocated = false; }
Texture::Texture( int w, int h, GLenum _type, int _channels )
{
	width = w; height = h; channels = _channels; type = _type;

	if( type == GL_UNSIGNED_BYTE )
	{
		pData = new glm::u8[w * h * channels];
	}
	else if( type == GL_UNSIGNED_SHORT )
	{
		pData = new glm::u16[w * h * channels];
	}
	else if( type == GL_FLOAT )
	{
		pData = new glm::f32[w * h * channels];
	}
	else pData = NULL;

	IsSTBAllocated = false;
}
/*
Texture::Texture( Texture &&t )
{
	width = t.width; height = t.height; pData = t.pData; IsSTBAllocated = t.IsSTBAllocated; type = t.type;
	t.width = 0; t.height = 0; t.pData = NULL; t.IsSTBAllocated = false; type = 0;
}

Texture &Texture::operator =( Texture &&t )
{
	if( this == &t ) return *this;

	clear();
	width = t.width; height = t.height; pData = t.pData; IsSTBAllocated = t.IsSTBAllocated; type = t.type;
	t.width = 0; t.height = 0; t.pData = NULL; t.IsSTBAllocated = false; type = 0;
	return *this;
}
*/

Texture::Texture( const char *file, bool swapY )
{
	width = 0; height = 0; pData = NULL; IsSTBAllocated = false; type = GL_NONE;
	LoadFromFile( file, swapY );
}

void Texture::MakeDefault( const glm::u8 def[4] )
{
	clear();

	IsSTBAllocated = false; type = GL_UNSIGNED_BYTE;
	width = 1; height = 1; channels = 4; pData = new glm::u8[4];
	memcpy( pData, def, 4 );
}

inline void Texture::clear()
{
	if( pData )
	{
		if( IsSTBAllocated ) { stbi_image_free( pData ); }
		else { delete[] (glm::u8*)pData; }
	}

	IsSTBAllocated = false; type = GL_NONE;
	pData = NULL; width = 0; height = 0;
}

void Texture::LoadFromFile( const char *file, bool swapY )
{
	OPTICK_EVENT();
	clear();

	//FILE *f = fopen( file.str().data(), "rb" );
	//if( !f ) return;

	string data = GetFileString( file, true );
	if( data == "" ) return;

	stbi_set_flip_vertically_on_load_thread( swapY );

	int x, y, _channels;
	void *buf;

	GLenum typ = GL_NONE;

	if( stbi_is_16_bit_from_memory( (stbi_uc *)data.data(), data.size() ) )
		typ = GL_UNSIGNED_SHORT, buf = stbi_load_16_from_memory( (stbi_uc *)data.data(), data.size(), &x, &y, &_channels, 0 );
	else 
		if( stbi_is_hdr_from_memory( (stbi_uc *)data.data(), data.size() ) )
		typ = GL_FLOAT, buf = stbi_loadf_from_memory( (stbi_uc *)data.data(), data.size(), &x, &y, &_channels, 0 );
	else
		typ = GL_UNSIGNED_BYTE, buf = stbi_load_from_memory( (stbi_uc *)data.data(), data.size(), &x, &y, &_channels, 0 );

	//fclose( f );

	if( buf )
	{
		IsSTBAllocated = true;
		pData = buf;
		width = x;
		height = y;
		type = typ;
		channels = _channels;
	}
}

static GLTexId CreateTextureInternal( const Texture &t, TextureCompressionModel compr )
{
	OPTICK_EVENT();
	int lv = glm::log2( max( t.height, t.width ) ) + 1;

	GLTexId tex;
	glCreateTextures( GL_TEXTURE_2D, 1, &tex );

	Texture::GetFormatRes format = t.GetFormat( compr );

	glTextureStorage2D( tex, lv, format.internalFormat, t.width, t.height );

	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );

	glTextureSubImage2D( tex, 0, 0, 0, t.width, t.height, format.format, t.type, t.pData );

	glPixelStorei( GL_UNPACK_ALIGNMENT, 4 );

	glGenerateTextureMipmap( tex );

	glTextureParameteriv( tex, GL_TEXTURE_SWIZZLE_RGBA, format.swizzle );

	glTextureParameteri( tex, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR ); // trilinear
	glTextureParameteri( tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	//if( EXT_AVAIABLE( GL_EXT_texture_filter_anisotropic	) || EXT_AVAIABLE( GL_ARB_texture_filter_anisotropic ) )
		glTextureParameteri( tex, GL_TEXTURE_MAX_ANISOTROPY, GLParameters.MaxAnisotropy );
	return tex;
}

const glm::u8 DefaultWhite[4] = { 255, 255, 255, 255 };

GLTexId CreateEmptyTexture()
{
	Texture t;
	t.MakeDefault( DefaultWhite );
	return CreateTextureInternal( t, TextureCompressionModel::NoCompression );
}

GLTexId CreateTexture( const char *Filename, TextureCompressionModel compr )
{
	return CreateTextureDefault( Filename, DefaultWhite, compr );
}

GLTexId CreateTextureDefault( const char *Filename, const glm::u8 def[4], TextureCompressionModel compr )
{
	OPTICK_EVENT();

	if( endsWith( Filename, ".dds" ) ||
		endsWith( Filename, ".kmg" ) ||
		endsWith( Filename, ".ktx" )
		)
		return CreateTextureFromFile_gli( Filename );

	Texture t( Filename, true );

	if( !t.pData )
	{
		TRACE( DebugLevel::Warning, "file %s couldn't be loaded as texture\n", Filename );
		t.MakeDefault( def ); 
	}

	return CreateTextureInternal( t, compr );
}
