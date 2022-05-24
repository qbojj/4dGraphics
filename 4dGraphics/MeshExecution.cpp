#include "stdafx.h"
#include "MeshExecution.h"

#include "TextureLoad.h"
#include "BufferAlloc.h"

#include "GLId.h"
#include "GLUniformBlockDefinitions.h"

#include "Debug.h"
#include "CommonUtility.h"

#include "Shader.h"

using namespace std;

inline unsigned int CeilLog2( unsigned int v ) { return glm::findMSB( ( v * 2 ) - 1 ); }

static glm::vec3 ToGLM( const aiVector3D &v ) { return { v.x, v.y, v.z }; }
static glm::vec3 ToGLM( const aiColor3D &v ) { return { v.r, v.g, v.b }; }
static glm::mat4 ToGLM( const aiMatrix4x4 &m ) {
	return glm::mat4(
		m.a1, m.b1, m.c1, m.d1,
		m.a2, m.b2, m.c2, m.d2,
		m.a3, m.b3, m.c3, m.d3,
		m.a4, m.b4, m.c4, m.d4
	);
}
static glm::mat3 ToGLM( const aiMatrix3x3 &m ) {
	return glm::mat3(
		m.a1, m.b1, m.c1,
		m.a2, m.b2, m.c2,
		m.a3, m.b3, m.c3
	);
}

/*
void FontRenderer::RenderText( GLProgram *fontFrag, const char *str, glm::vec2 pos, glm::vec4 col, float scale )
{
	OPTICK_EVENT();

	if( !VAO ) { TRACE( DebugLevel::Warning, "Tried rendering text with incomplete FontRenderer\n" ); return; }
	fontFrag->setVec4( "vTextColor", col );

	//scale *= 64;// charHeight / 64 / 24;
	glBindVertexArray( VAO );
	glBindTextureUnit( 0, tex );
	fontFrag->setInt( "tTex", 0 );

	glm::vec2 txSc = 1.f / glm::vec2(1.f*texSize.x,1.f*texSize.y);
	glm::vec2 startLine = pos;

	// iterate through all characters
	unsigned int c;
	const char *s = str;

	int l = (int)strlen( str );

	const float ScaleDiv64 = scale / 64.f;
	const float charHeightDisplacement = charHeight * ScaleDiv64;
	while( *s )
	{
		quad *data = (quad *)g_BufAllocator->allocAligned( sizeof( quad ) * min( l, numOfGlyphsInBatch ), sizeof(glm::vec4) );

		int si = 0;
		for( ; (c = *s) && si < numOfGlyphsInBatch; s++, l-- )
		{
			if( c == '\n' || c == '\r' )
			{
				pos = ( startLine += glm::vec2( 0.f, c == '\n' ? -charHeightDisplacement : charHeightDisplacement ) );
				continue;
			}

			glyphInfo &ch = GlyphInfo[CharToGlyph[c]];

			float xpos = pos.x + ch.bearing.x * scale;
			float ypos = pos.y - ( ch.size.y - ch.bearing.y ) * scale;

			float w = ch.size.x * scale;
			float h = ch.size.y * scale;
			// update VBO for each character

			glm::vec2 ptl = { xpos + w, ypos };
			glm::vec2 pbr = { xpos, ypos + h };

			glm::vec2 tl = txSc * glm::vec2( float( ch.pos.x ) + ch.size.x, float( ch.pos.y ) + ch.size.y );
			glm::vec2 br = txSc * glm::vec2( float( ch.pos.x ), float( ch.pos.y ) );

			//vertices[si++] = quad( ptl, pbr, tl, br );

			data[si++] = {
				{ pbr, br },
				{ pbr.x, ptl.y, br.x, tl.y },
				{ ptl, tl },
				{ ptl.x, pbr.y, tl.x, br.y }
			};

			pos += glm::vec2( ch.advance ) * ScaleDiv64;
		}

		g_BufAllocator->Flush( si * sizeof(quad) );
		if( VBOBound != g_BufAllocator->ID() ) glVertexArrayVertexBuffer( VAO, 0, VBOBound = g_BufAllocator->ID(), 0, sizeof( glm::vec4 ) );
		glDrawRangeElementsBaseVertex( GL_TRIANGLES, 0, si * 4 - 1, si * 6, GL_UNSIGNED_SHORT, 0, (GLint)g_BufAllocator->GetCurrentOffset() / sizeof( glm::vec4 ) );
	}

	glBindTextureUnit( 0, 0 );
	glBindVertexArray( 0 );
}

bool FontRenderer::Load( FT_Library ftLib, const char *fontPath, int _CharHeight )
{
	OPTICK_EVENT();
	clear();

	FT_Face face;
	if( FT_Error err = FT_New_Face( ftLib, fontPath, 0, &face ) )
	{
		const char *why = FT_Error_String( err );
		TRACE( DebugLevel::Error, "Cannot load font face form %s (%d%s%s)\n", fontPath, err, why ? ": " : "", why ? why : "" );
		return false;
	}

	FT_Set_Pixel_Sizes( face, 0, _CharHeight );

	charHeight = face->size->metrics.height;
	_CharHeight = (charHeight+63) / 64;

	constexpr unsigned int numOfCharsToLoad = 256;

	vector< stbrp_rect > rects;
	vector< unsigned int > GlyphId( numOfCharsToLoad );
	vector< unsigned int > What;

	CharToGlyph.resize( numOfCharsToLoad, 0 );

	{
		What.push_back( 0 );

		FT_UInt idx;
		FT_ULong ch = FT_Get_First_Char( face, &idx );

		while( idx != 0 && ch < numOfCharsToLoad )
		{
			CharToGlyph[ch] = (unsigned int)What.size();
			What.push_back( idx );

			ch = FT_Get_Next_Char( face, ch, &idx );
		}
	}

	for( unsigned int it : What )
	{
		if( FT_Error err = FT_Load_Glyph( face, it, FT_LOAD_BITMAP_METRICS_ONLY ) )
		{
			const char *why = FT_Error_String( err );
			TRACE( DebugLevel::Warning, "Cannot load glyph index %u (%d%s%s)\n", it, err, why ? ": " : "", why ? why : "" );
			continue;
		}

		stbrp_rect r = { 0 };
		r.w = face->glyph->bitmap.width + 1; // border of 1 pixel (right bottom) so blending doesn't break
		r.h = face->glyph->bitmap.rows + 1;
		r.id = (int)rects.size();

		rects.push_back( r );

		glyphInfo info = {
			glm::ivec2( 0, 0 ),
			glm::ivec2( face->glyph->bitmap.width, face->glyph->bitmap.rows ),
			glm::ivec2( face->glyph->bitmap_left, face->glyph->bitmap_top ),
			glm::ivec2( face->glyph->advance.x, face->glyph->advance.y )
		};

		GlyphInfo.push_back( info );

		//What.push_back( c );
	}

	assert( _CharHeight != 0 );
	assert( What.size() != 0 );

	unsigned int maLog2 = glm::clamp(CeilLog2( (unsigned int)( _CharHeight * _CharHeight * What.size() ) ) >> 1, 0u, 16u);

	unique_ptr<stbrp_node[]> nodes = make_unique<stbrp_node[]>( 1ull<<maLog2 );

	int mi = 0, ma = maLog2;
	//int si = MaxSi;
	//for(; si >= 0; si>>=1 )
	while( mi < ma )
	{
		int si = ( mi + ma ) >> 1;

		stbrp_context ctx;
		stbrp_init_target( &ctx, 1u << si, 1u << si, nodes.get(), 1u << si );
		if( stbrp_pack_rects( &ctx, rects.data(), (int)rects.size() ) ) ma = si;
		else															mi = si + 1;

		//if( !stbrp_pack_rects( &ctx, rects.data(), (int)rects.size() ) ) break;
	}
	//si *= 2;
	int si = 1<<mi;

	stbrp_context ctx;
	stbrp_init_target( &ctx, si, si, nodes.get(), si );
	if( !stbrp_pack_rects( &ctx, rects.data(), (int)rects.size() ) )
	{
		clear();
		return false;
	}
	
	int Y = si, X = si;

	nodes.reset();

	//double start = glfwGetTime();

	unsigned char *texture = (unsigned char *)malloc( X * Y );
	memset( texture, 0, X * Y );

	for( auto &i : rects )
	{
		auto &ci = GlyphInfo[i.id];
		ci.pos = glm::ivec2( i.x, i.y );
		
		assert( i.was_packed );
		assert( i.x >= 0 && i.x + i.w-1  < X );
		assert( i.y >= 0 && i.y + i.h-1 < Y );

		if( FT_Error err = FT_Load_Glyph( face, What[i.id], FT_LOAD_RENDER ) )
		{
			const char *why = FT_Error_String( err );
			TRACE( DebugLevel::Warning, "Cannot load and/or render glyph index %u (%d%s%s)\n", What[i.id], err, why ? ": " : "", why ? why : "" );
			continue;
		}

		unsigned char *data = face->glyph->bitmap.buffer;
		
		for( int y = 0; y < ci.size.y; y++ )
			memcpy( &texture[i.x + ( i.y + y ) * X], &data[y * ci.size.x], ci.size.x );
	}
	
	FT_Done_Face( face );
	int lv = glm::findMSB( max( X, Y ) ) + 1;

	glCreateTextures( GL_TEXTURE_2D, 1, &tex );

	glTextureParameteri( tex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTextureParameteri( tex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );

	glTextureParameteri( tex, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR ); // bilinear with mipmaps
	glTextureParameteri( tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

	const GLint Swizzles[] = { GL_ONE, GL_ONE, GL_ONE, GL_RED };
	glTextureParameteriv( tex, GL_TEXTURE_SWIZZLE_RGBA, Swizzles );

	glTextureStorage2D( tex, lv, GL_R8, X, Y );

	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	glTextureSubImage2D( tex, 0, 0, 0, X, Y, GL_RED, GL_UNSIGNED_BYTE, texture );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 4 );

	glGenerateTextureMipmap( tex );

	std::free( texture );

	//double duration = glfwGetTime() - start;
	//OutputDebug( "Texture creation took %lgus\n", duration * 1000000 );

	texSize = { X,Y };

	glCreateVertexArrays( 1, &VAO );
	
	//glCreateBuffers( 1, &VBO );
	//glNamedBufferStorage( VBO, sizeof( quad ) * numOfGlyphsInBatch, NULL, GL_DYNAMIC_STORAGE_BIT );
	//glVertexArrayVertexBuffer( VAO, 0, VBO, 0, sizeof( glm::vec4p ) );
	
	//VBO.Create();
	//glVertexArrayVertexBuffer( VAO, 0, VBO.Buff, 0, sizeof( float ) * 4 );

	glEnableVertexArrayAttrib( VAO, 0 );
	glVertexArrayAttribFormat( VAO, 0, 4, GL_FLOAT, GL_FALSE, 0 );
	glVertexArrayAttribBinding( VAO, 0, 0 );
	
	glm::u16 dat[6 * numOfGlyphsInBatch];

	for( int i = 0; i < numOfGlyphsInBatch; i++ )
	{
		dat[6 * i + 0] = 4 * i + 0;
		dat[6 * i + 1] = 4 * i + 1;
		dat[6 * i + 2] = 4 * i + 2;
		dat[6 * i + 3] = 4 * i + 0;
		dat[6 * i + 4] = 4 * i + 2;
		dat[6 * i + 5] = 4 * i + 3;
	}

	glCreateBuffers( 1, &EBO );
	glNamedBufferStorage( EBO, sizeof( dat ), dat, 0 );
	glVertexArrayElementBuffer( VAO, EBO );
	
	tex.SetLabel( "font '%s' Texture", fontPath );
	VAO.SetLabel( "font '%s' VAO", fontPath );
	EBO.SetLabel( "font '%s' EBO", fontPath );

	return true;
}

void FontRenderer::clear()
{

	VBOBound = 0;
	VAO.clear();
	EBO.clear();
	tex.clear();
	//CharToGlyph.clear();
	//GlyphInfo.clear();
	//texSize = { 0, 0 };

	//memset( &fontInfo, 0, sizeof( fontInfo ) );
	//fontData.clear();
}
*/

void Model::clear()
{
	//for( int i = 0; i < textures.size(); i++ ) glMakeTextureHandleNonResidentARB( glGetTextureHandleARB( textures[i] ) );

	//VBO.clear();
	//EBO.clear();
	VAO.clear();
	RenderDataBuffer.clear();
	EBOType = GL_NONE;
	VBO = EBO = Matrices = Materials = { 0,0 };
	//meshInfo.clear();
	textures.clear();
	textureHandles.clear();
	//materials.clear();
	texturesResident = false;

	MeshData.clear();
	NodeData.clear();
	matrices.clear();
	//DrawCallData.clear();
	DrawCallInfo.clear();
}

void Model::MakeNonResident() const
{
	if( EXT_AVAIABLE( GL_ARB_bindless_texture ) && texturesResident )
	{
		for( GLuint64 handle : textureHandles ) glMakeTextureHandleResidentARB( handle );
		texturesResident = false;
	}
}

void Model::MakeResident() const
{
	if( EXT_AVAIABLE( GL_ARB_bindless_texture ) && !texturesResident )
	{
		for( GLuint64 handle : textureHandles ) glMakeTextureHandleResidentARB( handle );
		texturesResident = true;
	}
}

void Model::GetOBBS( OBB *res ) const
{
	for( int i = 0; i < (int)DrawCallInfo.size(); i++ )
	{
		const DrawCallInfo_t &info = DrawCallInfo[i];
		res[i] = MeshData[info.MeshIdx].aabb;
		res[i].rotate( matrices[NodeData[info.NodeIdx].MatrixIdx] );
	}

	return;
}

/*
template<>
struct hash<glm::mat4>
{
	size_t operator()( const glm::mat4 &o ) const noexcept
	{
		return robin_hood::hash_bytes( &o, sizeof( o ) );
	}
};
*/

glm::uint Model::TraverseNodes( aiNode *node, int parent, glm::mat4 currMat )// , GLModelMatrices *Matrices, int curMatIdx )// , std::unordered_map<glm::mat4, int> &MatsIdxs )
{
	OPTICK_EVENT();

	NodeData_t d;
	d.MeshIds.assign( node->mMeshes, node->mMeshes + node->mNumMeshes );
	d.parent = parent;
	d.Children.resize( node->mNumChildren );
	d.name = node->mName.C_Str();

	currMat *= ToGLM( node->mTransformation );

	d.MatrixIdx = (int)matrices.size();
	matrices.push_back( currMat );

	int cur = (int)NodeData.size();
	NodeData.push_back( d );

	for( unsigned int i = 0; i < node->mNumMeshes; i++ ) 
		DrawCallInfo.push_back( DrawCallInfo_t{ (unsigned int)cur, node->mMeshes[i] } );

	for( unsigned int i = 0; i < node->mNumChildren; i++ )
		NodeData[cur].Children[i] = TraverseNodes( node->mChildren[i], cur, currMat );// , Matrices, curMatIdx );// , MatsIdxs );

	return cur;
}

void Model::DrawInternal( GLProgram *fragment, const GLDrawElementsIndirectData *dat, GLsizei size ) const
{
	OPTICK_EVENT();

	glBindVertexArray( VAO );

	int texturesCnt = min( (int)textures.size(), GLParameters.MaxFragmentImageUnits - 2 );
	GLint materialTexturesLocation;
	vector<int> textureIds;
	//vector<GLuint64> texHandles;

	if( fragment )
	{
		if( EXT_AVAIABLE( GL_ARB_bindless_texture ) )
		{
			MakeResident();
			//glProgramUniformHandleui64vARB( fragment->ID, materialTexturesLocation, (GLsizei)textureHandles.size(), textureHandles.data() );
			//texHandles = textureHandles;
			//texHandles.resize( GLParameters.MaxFragmentImageUnits - 2, texHandles[0] );
			//std::iota( textureIds.begin(), textureIds.begin() + min( (GLint)textures.size(), GLParameters.MaxFragmentImageUnits - 2 ), 0 );
			//glProgramUniformHandleui64vARB( fragment->ID, materialTexturesLocation, (GLsizei)textureHandles.size(), textureHandles.data() );
			//glBindTextures( 0, texturesCnt, (GLuint *)textures.data() );
			//glProgramUniform1iv( fragment->ID, materialTexturesLocation, (GLsizei)textureIds.size(), textureIds.data() );
		}
		else
		{
			materialTexturesLocation = fragment->GetUniformLocation( "ahMaterialTextures[0]" );

			textureIds.resize( GLParameters.MaxFragmentImageUnits - 2, 0 );
			iota( textureIds.begin(), textureIds.begin() + min( (GLint)textures.size(), GLParameters.MaxFragmentImageUnits - 2 ), 0 );

			glBindTextures( 0, texturesCnt, (GLuint *)textures.data() );
			glProgramUniform1iv( fragment->ID, materialTexturesLocation, (GLsizei)textureIds.size(), textureIds.data() );
		}
	}

	glBindBufferRange( GL_SHADER_STORAGE_BUFFER, SSBO_MATERIALS_BINDING, 
		RenderDataBuffer, Materials.offset, Materials.size );
	glBindBufferRange( GL_SHADER_STORAGE_BUFFER, SSBO_MODEL_MATRICES_BINDING, 
		RenderDataBuffer, Matrices.offset, Matrices.size );
	/////////////////////////////////////////////////

	//if( EXT_AVAIABLE( GL_ARB_multi_draw_indirect ) )
	//{
		if( void *indirect = g_BufAllocator->alloc( size * sizeof( GLDrawElementsIndirectData ) ) )
		{
			memcpy( indirect, dat, size * sizeof( GLDrawElementsIndirectData ) );
			g_BufAllocator->Flush();
	
			glBindBuffer( GL_DRAW_INDIRECT_BUFFER, g_BufAllocator->ID() );
			glMultiDrawElementsIndirect( GL_TRIANGLES, EBOType, (void *)g_BufAllocator->GetCurrentOffset(), size, 0 );
			glBindBuffer( GL_DRAW_INDIRECT_BUFFER, 0 );
		}
		else TRACE( DebugLevel::Error, "Cannot alloc indirect buffer!\n" );


		
		/*	}

	else
	{*/
	/*
		size_t TypeSize =
		EBOType == GL_UNSIGNED_BYTE ? sizeof( unsigned char ) :
			EBOType == GL_UNSIGNED_SHORT ? sizeof( unsigned short ) :
			EBOType == GL_UNSIGNED_INT ? sizeof( unsigned int ) :
			(TRACE( DebugLevel::Error, "Incorrect EBOType. type=%x (Model not initialized?)\n", EBOType ), 0);

		for( int i = 0; i < size; i++ )
		{
			const GLDrawElementsIndirectData *cmd = &dat[i];
			glDrawElementsInstancedBaseVertexBaseInstance(
				GL_TRIANGLES,
				cmd->count,
				EBOType,
				(void *)(cmd->firstIndex * TypeSize),
				cmd->instanceCount,
				cmd->baseVertex,
				cmd->baseInstance );
		}
		*/
		/*
	}
	*/
	/////////////////////////////////////////////////

	glBindBufferBase( GL_SHADER_STORAGE_BUFFER, SSBO_MATERIALS_BINDING, 0 );
	glBindBufferBase( GL_SHADER_STORAGE_BUFFER, SSBO_MODEL_MATRICES_BINDING, 0 );

	if( fragment )
	{
		if( EXT_AVAIABLE( GL_ARB_bindless_texture ) )
		{
			//texHandles = vector<GLuint64>( GLParameters.MaxFragmentImageUnits - 2, 0 );
			//fill( texHandles.begin(), texHandles.end(), 0 );// .resize( GLParameters.MaxFragmentImageUnits - 2, texHandles[0] );
			//std::iota( textureIds.begin(), textureIds.begin() + min( (GLint)textures.size(), GLParameters.MaxFragmentImageUnits - 2 ), 0 );
			//glProgramUniformHandleui64vARB( fragment->ID, materialTexturesLocation, (GLsizei)texHandles.size(), texHandles.data() );
			//glBindTextures( 0, texturesCnt, (GLuint *)textures.data() );
			//glProgramUniform1iv( fragment->ID, materialTexturesLocation, (GLsizei)textureIds.size(), textureIds.data() );
		}
		else
		{
			//fill( textureIds.begin(), textureIds.end(), 0 );

			glBindTextures( 0, texturesCnt, NULL );
			//glProgramUniform1iv( fragment->ID, materialTexturesLocation, (GLsizei)textureIds.size(), textureIds.data() );
		}
	}
	//if( !GLAD_GL_ARB_bindless_texture && fragment ) glBindTextures( 0, texturesCnt, NULL );
	/*
	if( !GLAD_GL_ARB_bindless_texture && fragment )
	{
		std::fill( textureIds.begin(), textureIds.end(), 0 );

		glProgramUniform1iv( fragment->ID, materialTexturesLocation, (GLsizei)textureIds.size(), textureIds.data() );
		glBindTextures( 0, texturesCnt, NULL );
	}
	*/
	glBindVertexArray( 0 );
}

GLDrawElementsIndirectData Model::GetDrawElementsData( size_t DrawCallInfoIdx ) const
{
	OPTICK_EVENT();

	const MeshData_t &meshDat = MeshData[DrawCallInfo[DrawCallInfoIdx].MeshIdx];

	GLDrawElementsIndirectData dat;
	dat.count = meshDat.end - meshDat.start;
	dat.instanceCount = 1;
	dat.firstIndex = (glm::u32)(EBO.offset + meshDat.start);
	dat.baseVertex = meshDat.baseVertex;
	dat.baseInstance = glm::packUint2x16( glm::u16vec2( 
		(glm::u16)meshDat.material, 
		(glm::u16)NodeData[DrawCallInfo[DrawCallInfoIdx].NodeIdx].MatrixIdx )
	);

	return dat;
}

void Model::Draw( GLProgram *fragment ) const
{
	vector<GLDrawElementsIndirectData> DrawData( DrawCallInfo.size() );

	for( size_t i = 0; i < DrawCallInfo.size(); i++ )
		DrawData[i] = GetDrawElementsData( i );

	DrawInternal( fragment, DrawData.data(), (GLsizei)DrawData.size() );
}

void Model::DrawMasked( GLProgram *fragment, const std::vector<bool> &mask, size_t startIdx ) const
{
	vector<GLDrawElementsIndirectData> DrawData;

	for( size_t i = 0; i < GetOBBcnt(); i++ )
	{
		if( !mask[startIdx + i] ) continue;
		DrawData.push_back( GetDrawElementsData( i ) );
	}

	if( DrawData.size() != 0 ) DrawInternal( fragment, DrawData.data(), (GLsizei)DrawData.size() );
}

bool Model::Load( const char *file )
{
	Assimp::Importer loader;
	return Load( file, loader );
}

bool Model::Load( const char *file, Assimp::Importer &importer, unsigned int additionalFlags )
{
	namespace fs = std::filesystem;
	OPTICK_EVENT();

	clear();

	//filesystem::path dir( file );
	//string filename = dir.filename().string();
	//dir = dir.parent_path();

	fs::path dir = fs::path( file ).parent_path();

	TRACE( DebugLevel::Debug, "Model: Start import '%s'\n", file );

	importer.SetPropertyInteger( AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_LINE | aiPrimitiveType_POINT );
	importer.SetPropertyInteger( AI_CONFIG_PP_RVC_FLAGS, 
		aiComponent_ANIMATIONS | aiComponent_BONEWEIGHTS | aiComponent_CAMERAS | aiComponent_LIGHTS
	);

	const aiScene *scene = importer.ReadFile( file,
											aiProcessPreset_TargetRealtime_Fast |
											aiProcess_RemoveComponent |
											aiProcess_CalcTangentSpace |
											aiProcess_TransformUVCoords |
											aiProcess_ImproveCacheLocality |
											aiProcess_RemoveRedundantMaterials |
											aiProcess_SortByPType |
											aiProcess_GenBoundingBoxes |
											//aiProcess_OptimizeGraph |
											//aiProcess_OptimizeMeshes |
											additionalFlags
	);

	if( !scene || !scene->mRootNode )
	{
		TRACE( DebugLevel::Warning, "Error during importing model '%s' (%s)\n", file, importer.GetErrorString() );
		return false;
	}

	TRACE( DebugLevel::Debug, "Model: File read\n" );

	struct Vertice
	{
		glm::vec3 pos;
		glm::vec2 tex;
		glm::uint norm, tangent;// , bitangent; // normalized GL_INT_2_10_10_10_REV
		glm::uint col; // normalized, packed rgba8
	};

	//vector<unsigned int> verticeOffset( scene->mNumMeshes );

	unsigned int verticeCnt = 0, maxVerticePerMeshCnt = 0;
	unsigned int triangleCnt = 0;

	for( unsigned int i = 0; i < scene->mNumMeshes; i++ )
	{
		aiMesh *mesh = scene->mMeshes[i];
		
		verticeCnt += mesh->mNumVertices;
		maxVerticePerMeshCnt = max( maxVerticePerMeshCnt, mesh->mNumVertices );
		triangleCnt += mesh->mNumFaces;
	}
	
	unsigned int typeSize;
	if( verticeCnt <= 0xff ) { EBOType = GL_UNSIGNED_BYTE; typeSize = 1; }
	else if( verticeCnt <= 0xffff ) { EBOType = GL_UNSIGNED_SHORT; typeSize = 2; }
	else { EBOType = GL_UNSIGNED_INT; typeSize = 4; }

	// std::unordered_map<glm::mat4, int> matsIds;
	TraverseNodes( scene->mRootNode, -1, glm::mat4( 1 ) );//, matsIds );

	MeshData.resize( scene->mNumMeshes );

	glCreateBuffers( 1, &RenderDataBuffer );
	if( !RenderDataBuffer ) { TRACE( DebugLevel::Error, "Error: glCreateBuffers returned invalid hadle\n" ); importer.FreeScene(); clear(); return false; }

	EBO = { 0, (GLsizeiptr)( typeSize ) * triangleCnt * 3 };
	VBO = { AlignVal( EBO.end() ), (GLsizeiptr)( sizeof( Vertice ) * verticeCnt ) };
	Materials = { AlignVal( VBO.end(), GLParameters.SSBOAlignment ), (GLsizeiptr)( sizeof( glsl::MaterialSpec ) * scene->mNumMaterials ) };
	Matrices = { AlignVal( Materials.end(), GLParameters.SSBOAlignment ), (GLsizeiptr)( sizeof( glsl::ModelMatrixSet ) * matrices.size() ) };

	glNamedBufferStorage( RenderDataBuffer, (GLsizeiptr)Matrices.end(), NULL, GL_MAP_WRITE_BIT );

	const int maxTries_ = 2;
	for( int tries_ = 0; tries_ < maxTries_; tries_++ )
	{
		void *RenderDataPtr = glMapNamedBufferRange( RenderDataBuffer, 0, (GLsizeiptr)Matrices.end(), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT );
		if( !RenderDataPtr ) { TRACE( DebugLevel::Error, "Error: glMapNamedBuffer returned NULL ptr\n" ); importer.FreeScene(); clear(); return false; }

		//vector<Vertice> vertices( verticeCnt );
		//char *Indexes = (char*)malloc( typeSize * triangleCnt * 3 );

		void *Indexes = (char *)( (char*)RenderDataPtr + EBO.offset );
		Vertice *vertices = (Vertice *)( (char*)RenderDataPtr + VBO.offset );
		glsl::MaterialSpec *Maters = (glsl::MaterialSpec *)((char *)RenderDataPtr + Materials.offset);
		glsl::ModelMatrixSet *Matris = (glsl::ModelMatrixSet *)((char *)RenderDataPtr + Matrices.offset);

		auto SetTriangleIndexes = [&]( int idx, unsigned int *indeces )
		{
			//memcpy( (char *)Indexes + idx * typeSize, &v, typeSize );

			union triangle
			{
				glm::u8vec3 Byte;
				glm::u16vec3 Short;
				glm::u32vec3 Int;
			};

			triangle *t = (triangle *)((char*)Indexes + idx * typeSize);

			glm::u32vec3 data{ indeces[0], indeces[1], indeces[2] };

			if( EBOType == GL_UNSIGNED_BYTE )		t->Byte = data;
			else if( EBOType == GL_UNSIGNED_SHORT )	t->Short = data;
			else									t->Int = data;
		};

		//meshInfo.resize( scene->mNumMeshes );

		unsigned int EBOOffset = 0;
		int VertexOffset = 0;
		for( unsigned int i = 0; i < scene->mNumMeshes; i++ )
		{
			aiMesh *mesh = scene->mMeshes[i];
			unsigned int start = EBOOffset;

			for( unsigned int j = 0; j < mesh->mNumFaces; j++ )
			{
				assert( mesh->mFaces[j].mNumIndices == 3 );
				unsigned int *indeces = mesh->mFaces[j].mIndices;
				SetTriangleIndexes( EBOOffset, indeces ); EBOOffset += 3;
			}

			MeshData[i].start = start;
			MeshData[i].end = EBOOffset;
			MeshData[i].material = mesh->mMaterialIndex;
		}

		VertexOffset = 0;
		for( unsigned int i = 0; i < scene->mNumMeshes; i++ )
		{
			aiMesh *mesh = scene->mMeshes[i];

			bool hasTangent = mesh->HasTangentsAndBitangents();
			bool hasTextureCoord = mesh->HasTextureCoords( 0 );
			bool hasVertexColor = mesh->HasVertexColors( 0 );

			//glm::vec3 mi( INFINITY ), ma( -INFINITY );

			for( unsigned int j = 0; j < mesh->mNumVertices; j++ )
			{
				glm::vec3 pos( mesh->mVertices[j].x, mesh->mVertices[j].y, mesh->mVertices[j].z );
				glm::vec2 tex =
					hasTextureCoord ? glm::vec2( mesh->mTextureCoords[0][j].x, mesh->mTextureCoords[0][j].y ) :
					glm::vec2( 0 );

				glm::vec3 norm( mesh->mNormals[j].x, mesh->mNormals[j].y, mesh->mNormals[j].z );
				glm::vec3 tangent( 0, 0, 1 );
				glm::vec3 bitangent( 0, 1, 0 );

				if( hasTangent )
				{
					tangent = glm::vec3( mesh->mTangents[j].x, mesh->mTangents[j].y, mesh->mTangents[j].z );
					bitangent = glm::vec3( mesh->mBitangents[j].x, mesh->mBitangents[j].y, mesh->mBitangents[j].z );
				}
				glm::vec4 col =
					hasVertexColor ?
					glm::vec4( mesh->mColors[0][j].r, mesh->mColors[0][j].g, mesh->mColors[0][j].b, mesh->mColors[0][j].a ) :
					glm::vec4( 1 );

				vertices[VertexOffset + j] =
				{
					pos,
					tex,
					glm::packSnorm3x10_1x2( glm::vec4( norm,      0 ) ),
					glm::packSnorm3x10_1x2( glm::vec4( tangent,   0 ) ),
					//glm::packSnorm3x10_1x2( glm::vec4( bitangent, 0 ) ),
					glm::packUnorm4x8( col )
				};

				//mi = glm::min( mi, pos );
				//ma = glm::max( ma, pos );
			}

			MeshData[i].baseVertex = VertexOffset;
			MeshData[i].aabb = AABB( ToGLM( mesh->mAABB.mMin ), ToGLM( mesh->mAABB.mMax ) );//mi, ma );

			VertexOffset += mesh->mNumVertices;
		}

		unordered_map<string, unsigned int> NameToTex;

		//GLUniformArrayData<GLMaterialSpec> Materials( scene->mNumMaterials );
		//materials.resize( scene->mNumMaterials );

		//vector<GLMaterialSpec> Materials( scene->mNumMaterials );


		/*
		auto GLMaterialFormMaterial = []( const Material &m, const GLuint *textures ) -> GLMaterialSpec
		{
			GLMaterialSpec mat;
			mat.fAlpha = m.fAlpha;
			mat.fShininess = m.fShininess;
			//mat.iShadingModel = m.ShadingModel;
			mat.vColAmb = m.cAmbient;
			mat.vColDif = m.cDiffuse;
			mat.vColSpec = m.cSpecular;

			//mat.hTexDiffuse = glGetTextureHandleARB( textures[m.tDiffuse] );
			//mat.hTexSpecular = glGetTextureHandleARB( textures[m.tSpecular] );
			return mat;
		};
		*/
		for( unsigned int i = 0; i < scene->mNumMaterials; i++ )
		{
			aiMaterial *mat = scene->mMaterials[i];
			glsl::MaterialSpec m;

			aiColor3D c; float f; aiString s;
			mat->Get( AI_MATKEY_COLOR_AMBIENT, c );
			m.vColAmb = glm::vec4( ToGLM( c ), 1 );

			mat->Get( AI_MATKEY_COLOR_DIFFUSE, c );
			m.vColDif = glm::vec4( ToGLM( c ), 0 );

			mat->Get( AI_MATKEY_COLOR_SPECULAR, c );
			if( AI_SUCCESS != mat->Get( AI_MATKEY_SHININESS_STRENGTH, f ) ) f = 1;
			m.vColSpec = glm::vec4( ToGLM( c ) * f, 0 );

			if( AI_SUCCESS != mat->Get( AI_MATKEY_OPACITY, m.vColDif.a ) ) m.vColDif.a = 1;
			if( AI_SUCCESS != mat->Get( AI_MATKEY_SHININESS, m.vColSpec.a ) ) m.vColSpec.a = 1;

			/*
			aiShadingMode sm;
			if( AI_SUCCESS != mat->Get( AI_MATKEY_SHADING_MODEL, sm ) ) sm = aiShadingMode_Blinn;

			m.ShadingModel =
				sm == aiShadingMode_Flat || sm == aiShadingMode_NoShading ? 0 : // flat
				sm == aiShadingMode_Phong || sm == aiShadingMode_Gouraud ? 1 :  // phong
				2;																// blinn-phong
			*/

			const auto _LoadTexture = [&]( const aiString &s ) -> unsigned int
			{
				string pth = s.C_Str();
				
				for( char &c : pth ) if( c == '\\' ) c = '/';
			
				filesystem::path w = pth;
				//TRACE( DebugLevel::Error, "TexPath (%s)\n", s.C_Str() );
				
				if( w.is_relative() )
				{
					//TRACE( DebugLevel::Error, "relative (%s)\n", ( string( dir / w ) ).c_str() );
					if( filesystem::exists( dir / w ) ) w = dir / w;//, TRACE( DebugLevel::Error, "exists in dir\n" );
					else w = filesystem::current_path() / w;
				}

				pth = w.string();
				//TRACE( DebugLevel::Error, "TexPath Processed (%s)\n", pth.c_str() );
				
				
				auto p = NameToTex.find( pth );

				if( p != NameToTex.end() ) return p->second;

				unsigned int res = NameToTex[pth] = (unsigned int)NameToTex.size();

				GLTexId tex = CreateTexture( pth.c_str() );

				/*
				GLenum target;
				glGetTextureParameteriv( tex, GL_TEXTURE_TARGET, (GLint *)&target );
				if( target != GL_TEXTURE_2D )
				{
					TRACE( DebugLevel::Error, "Error: models textures must be 2D-textures (\"%s\")\n" );
					tex = CreateEmptyTexture();
				}*/


				tex.SetLabel( "mesh '%s' texture %d ('%s')", file, res, s.C_Str() );
				GLuint texID = tex;
				textures.push_back( move( tex ) );

				return res;
			};

			const auto LoadTexture = [&]( const aiString &s ) -> GLuint64
			{
				unsigned int texIdx = _LoadTexture( s );
				if( EXT_AVAIABLE( GL_ARB_bindless_texture ) )
				{
					if( textureHandles.size() == texIdx )
					{
						GLuint64 TextureHandle = glGetTextureHandleARB( textures[texIdx] );
						textureHandles.push_back( TextureHandle );
						return TextureHandle;
					}
					else return textureHandles[texIdx];
				}
				else return (GLuint64)texIdx;
			};

			if( AI_SUCCESS != mat->Get( AI_MATKEY_TEXTURE_DIFFUSE( 0 ), s ) ) s = DATA_PATH "NoTextureColor.dds";
			m.iTexDiffuse = glm::unpackUint2x32( LoadTexture( s ) );

			if( AI_SUCCESS != mat->Get( AI_MATKEY_TEXTURE_SPECULAR( 0 ), s ) ) s = DATA_PATH "NoTextureColor.dds";
			m.iTexSpecular = glm::unpackUint2x32( LoadTexture( s ) );

			if( AI_SUCCESS != mat->Get( AI_MATKEY_TEXTURE_NORMALS( 0 ), s ) ) s = DATA_PATH "NoTextureNormal.dds";
			m.iTexNormal = glm::unpackUint2x32( LoadTexture( s ) );

			Maters[i] = m;//GLMaterialFormMaterial( m, Textures.data() );
		}

		for( int i = 0; i < (int)matrices.size(); i++ ) Matris[i] = { matrices[i], glm::transpose( glm::inverse( matrices[i] ) ) };

		if( !glUnmapNamedBuffer( RenderDataBuffer ) )
		{
			// data store contents have become corrupt during the time the data store was mapped
			// try again and if bad again report error
			if( tries_ < maxTries_ - 1 ) TRACE( DebugLevel::Warning, "Warning: glUnmapNamedBuffer returned GL_FALSE => try to write data again\n" );
			else
			{
				TRACE( DebugLevel::Error, "Error: glUnmapNamedBuffer returned GL_FALSE again! Trying no more!\n" );
				importer.FreeScene(); 
				clear();
				return false;
			}
		}
	}

	glCreateVertexArrays( 1, &VAO );
	if( !VAO ) { TRACE( DebugLevel::Error, "Error: glCreateVertexArrays returned invaid handle!\n" ); importer.FreeScene(); clear(); return false; }
	//glCreateBuffers( 1, &VBO );
	//glCreateBuffers( 1, &EBO );
	//glCreateBuffers( 1, &MaterialBO );
	//glCreateBuffers( 1, &MatricesBO );
	//glCreateTextures( GL_TEXTURE_RECTANGLE, 1, &MatricesTex );
	//glCreateBuffers( 1, &IndirectBO );

	//glNamedBufferStorage( VBO, sizeof( Vertice ) * verticeCnt, vertices.data(), 0 );
	//glNamedBufferStorage( EBO, typeSize * triangleCnt * 3, Indexes, 0 ); std::free( Indexes );
	//glNamedBufferStorage( MaterialBO, Materials.size() * sizeof( Materials[0] ), Materials.data(), 0 );
	//glNamedBufferStorage( MatricesBO, Matrices.size() * sizeof( Matrices[0] ), Matrices.data(), 0 );
	/*
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	glTextureStorage2D( MatricesTex, 1, GL_RGBA32F, 8, (GLsizei)Matrices.size() );
	glTextureSubImage2D( MatricesTex, 0, 0, 0, 8, (GLsizei)Matrices.size(), GL_RGBA, GL_FLOAT, &Matrices[0] );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 4 );
	*/
	//glNamedBufferStorage( IndirectBO, Indirect.size() * sizeof( Indirect[0] ), &Indirect[0], 0 );
	//IndirectCnt = (int)Indirect.size();

	glVertexArrayVertexBuffer( VAO, 0, RenderDataBuffer, VBO.offset, sizeof( Vertice ) );
	glVertexArrayElementBuffer( VAO, RenderDataBuffer );

	{
		glEnableVertexArrayAttrib( VAO, 0 );
		glVertexArrayAttribFormat( VAO, 0, 3, GL_FLOAT, GL_FALSE, offsetof( Vertice, pos ) );
		glVertexArrayAttribBinding( VAO, 0, 0 );

		glEnableVertexArrayAttrib( VAO, 1 );
		glVertexArrayAttribFormat( VAO, 1, 2, GL_FLOAT, GL_FALSE, offsetof( Vertice, tex ) );
		glVertexArrayAttribBinding( VAO, 1, 0 );

		glEnableVertexArrayAttrib( VAO, 2 );
		glVertexArrayAttribFormat( VAO, 2, 4, GL_INT_2_10_10_10_REV, GL_TRUE, offsetof( Vertice, norm ) );
		glVertexArrayAttribBinding( VAO, 2, 0 );

		glEnableVertexArrayAttrib( VAO, 3 );
		glVertexArrayAttribFormat( VAO, 3, 4, GL_INT_2_10_10_10_REV, GL_TRUE, offsetof( Vertice, tangent ) );
		glVertexArrayAttribBinding( VAO, 3, 0 );
/*
		glEnableVertexArrayAttrib( VAO, 4 );
		glVertexArrayAttribFormat( VAO, 4, 4, GL_INT_2_10_10_10_REV, GL_TRUE, offsetof( Vertice, bitangent ) );
		glVertexArrayAttribBinding( VAO, 4, 0 );
*/
		glEnableVertexArrayAttrib( VAO, 4 );
		glVertexArrayAttribFormat( VAO, 4, 4, GL_UNSIGNED_BYTE, GL_TRUE, offsetof( Vertice, col ) );
		glVertexArrayAttribBinding( VAO, 4, 0 );
	}

	VAO.SetLabel( "mesh '%s' VAO", file );
	RenderDataBuffer.SetLabel( "mesh '%s' data buffer", file );
	//VBO.SetLabel( "mesh '%s' VBO", filename.data() );
	//EBO.SetLabel( "mesh '%s' EBO", filename.data() );
	//MaterialBO.SetLabel( "mesh '%s' Materials", filename.data() );
	//MatricesBO.SetLabel( "mesh '%s' Matrices", filename.data() );
	//MatricesTex.SetLabel( "mesh '%s' Matrices", filename.data() );
	//IndirectBO.SetLabel( "mesh '%s' Indirect data", filename.data() );

	TRACE( DebugLevel::Debug, "Model: loading ended\n" );
	importer.FreeScene();
	return true;
}

void StreamArrayDrawer::Create( const std::vector<VertexAttributeDescr> &attributes, const std::vector<BindingDescr> &arrays )
{
	OPTICK_EVENT();

	clear();
	arrayDescriptors = arrays;

	glCreateVertexArrays( 1, &VAO );

	for( const VertexAttributeDescr &i : attributes )
	{
		glEnableVertexArrayAttrib( VAO, i.attribindex );
		glVertexArrayAttribFormat( VAO, i.attribindex, i.size, i.type, i.normalized, i.relativeOffset );
		glVertexArrayAttribBinding( VAO, i.attribindex, i.arrayID );
	}

	for( size_t i = 0; i < arrays.size(); i++ )
		glVertexArrayBindingDivisor( VAO, (GLuint)i, arrayDescriptors[i].divisor );
}

void StreamArrayDrawer::DrawStreamArray( const void *vertex_data[], GLsizei size, GLenum mode /*=GL_TRIANGLES*/ )
{
	OPTICK_EVENT();
	if( !prepareData( vertex_data, size ) ) return;

	glBindVertexArray( VAO );
	glDrawArrays( mode, 0, size );
	glBindVertexArray( 0 );

	//glVertexArrayVertexBuffers( VAO, 0, (GLsizei)arrayDescriptors.size(), NULL, NULL, NULL );
}

void StreamArrayDrawer::DrawStreamElements( 
	const void *vertex_data[], GLsizei dataSize, const void *indices, 
	GLsizei count, GLenum mode, GLenum indices_type )
{
	OPTICK_EVENT();
	GLsizei indicesSize = count * (
		indices_type == GL_UNSIGNED_BYTE ? 1 :
		indices_type == GL_UNSIGNED_SHORT ? 2 :
		indices_type == GL_UNSIGNED_INT ? 4 :
		0
		);

	if( !prepareData( vertex_data, dataSize, indices, indicesSize ) ) return;

	glBindVertexArray( VAO );
	glDrawRangeElements( mode, 0, dataSize, count, indices_type, (void*)indecesOffset );
	glBindVertexArray( 0 );

	//glVertexArrayVertexBuffers( VAO, 0, (GLsizei)arrayDescriptors.size(), NULL, NULL, NULL );
}

bool StreamArrayDrawer::prepareData( const void *vertex_data[], GLsizei size, const void *index, GLsizei indexSize )
{
	OPTICK_EVENT();
	size_t bufferSize = 0;
	for( const BindingDescr &i : arrayDescriptors )
		bufferSize += i.stride * (size / (i.divisor ? i.divisor : 1));

	bufferSize += indexSize;

	void *dat = g_BufAllocator->alloc( bufferSize );

	if( !dat )
	{
		TRACE( DebugLevel::Error, "Buffer allocator could not create buffer of size %d\n", (int)bufferSize );
		return false;
	}

	std::vector<GLuint> buffers( arrayDescriptors.size(), g_BufAllocator->ID() );
	std::vector<GLsizei> strides( arrayDescriptors.size() );
	std::vector<GLintptr> offsets( arrayDescriptors.size() );

	size_t offset = 0;
	for( size_t i = 0; i < arrayDescriptors.size(); i++ )
	{
		const BindingDescr &d = arrayDescriptors[i];
		size_t siz = d.stride * (size / (d.divisor ? d.divisor : 1));
		memcpy( dat, vertex_data[i], siz );

		strides[i] = d.stride;
		offsets[i] = offset + g_BufAllocator->GetCurrentOffset();

		offset += siz;
	}

	if( indexSize ) 
	{
		memcpy( (char *)dat + offset, index, indexSize ); 
		glVertexArrayElementBuffer( VAO, g_BufAllocator->ID() );
		indecesOffset = offset + g_BufAllocator->GetCurrentOffset();
	}

	g_BufAllocator->Flush();
	glVertexArrayVertexBuffers( VAO, 0, (GLsizei)arrayDescriptors.size(), buffers.data(), offsets.data(), strides.data() );
	return true;
}
