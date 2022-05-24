#include "stdafx.h"
#include "GameRenderHandler.h"
#include "BufferAlloc.h"
#include "GameTickHandler.h"

#include "Debug.h"
#include "Objects.h"

using namespace std;


#define FINISH() {}//(void(glFinish()))

using glm::vec2;
using glm::vec3;
using glm::vec4;
using glm::mat3;
using glm::mat4;



static void GLDebugMessageCallback( GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
	const GLchar *msg, const void *data )
{
	OPTICK_EVENT();

	const char *_source;
	const char *_type;
	const char *_severity;

	DebugLevel lev;

	switch( source ) {
	case GL_DEBUG_SOURCE_API:				_source = "API"; break;
	case GL_DEBUG_SOURCE_WINDOW_SYSTEM:		_source = "WINDOW SYSTEM"; break;
	case GL_DEBUG_SOURCE_SHADER_COMPILER:	_source = "SHADER COMPILER"; break;
	case GL_DEBUG_SOURCE_THIRD_PARTY:		_source = "THIRD PARTY"; break;
	case GL_DEBUG_SOURCE_APPLICATION:		_source = "APPLICATION"; break;
	case GL_DEBUG_SOURCE_OTHER:				_source = "OTHER"; break;
	default:								_source = "UNKNOWN"; break;
	}

	switch( type ) {
	case GL_DEBUG_TYPE_ERROR:				_type = "ERROR"; break;
	case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:	_type = "DEPRECATED BEHAVIOR"; break;
	case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:	_type = "UDEFINED BEHAVIOR"; break;
	case GL_DEBUG_TYPE_PORTABILITY:			_type = "PORTABILITY"; break;
	case GL_DEBUG_TYPE_PERFORMANCE:			_type = "PERFORMANCE"; break;
	case GL_DEBUG_TYPE_OTHER:				_type = "OTHER"; break;
	case GL_DEBUG_TYPE_MARKER:				_type = "MARKER"; break;
	default:								_type = "UNKNOWN"; break;
	}

	switch( severity ) {
	case GL_DEBUG_SEVERITY_HIGH:			_severity = "HIGH"; lev = DebugLevel::Warning; break;
	case GL_DEBUG_SEVERITY_MEDIUM:			_severity = "MEDIUM"; lev = DebugLevel::Warning; break;
	case GL_DEBUG_SEVERITY_LOW:				_severity = "LOW"; lev = DebugLevel::Warning; break;
	case GL_DEBUG_SEVERITY_NOTIFICATION:	_severity = "NOTIFICATION"; lev = DebugLevel::Log; break;
	default:								_severity = "UNKNOWN"; lev = DebugLevel::Warning; break;
	}

	if( severity != GL_DEBUG_SEVERITY_NOTIFICATION )
		TRACE( lev, "%d: %s (%s) [%s]: %s\n", id, _type, _severity, _source, msg );
}

/*
struct GLextentionDef_t {
	std::reference_wrapper<int> avaiable;
	//GLextension ext;

	const char *name;
};

#define EXDEF(name) GLextentionDef_t{ EXT_AVAIABLE( GLAD_ ## name ), #name }

const GLextentionDef_t ExtensionsRequired[] = {
	EXDEF( GL_ARB_uniform_buffer_object ),
	EXDEF( GL_ARB_shader_storage_buffer_object ),
	EXDEF( GL_ARB_clip_control ),
	EXDEF( GL_ARB_draw_buffers ),
	EXDEF( GL_ARB_gpu_shader5 ),
	EXDEF( GL_ARB_buffer_storage ),
	EXDEF( GL_ARB_texture_storage ),
	EXDEF( GL_ARB_texture_view ),
	EXDEF( GL_ARB_shading_language_420pack ),
	EXDEF( GL_ARB_clear_buffer_object ),
	EXDEF( GL_ARB_base_instance ),
	EXDEF( GL_ARB_shader_draw_parameters ),
	EXDEF( GL_ARB_separate_shader_objects ),
	EXDEF( GL_ARB_depth_texture ),
	EXDEF( GL_ARB_multi_bind ),
	EXDEF( GL_ARB_direct_state_access ),
	EXDEF( GL_ARB_explicit_attrib_location ),
	EXDEF( GL_ARB_clip_control ),
	EXDEF( GL_ARB_map_buffer_range )
};

#undef EXDEF
*/

void GameRenderHandler::OnDestroy()
{
	OPTICK_EVENT();
/*
	if( FT_Error err = FT_Done_FreeType( FreeTypeLib ) )
	{
		const char *why = FT_Error_String( err );
		TRACE( DebugLevel::Error, "FT_Done_FreeType returned error (%d%s%s)\n", err, why ? ": " : "", why ? why : "" );
	}
	*/
	if( g_BufAllocator ) delete g_BufAllocator;
}

void GameRenderHandler::CreateAndInitShadowFBO( int cnt, int size )
{
	OPTICK_EVENT();

	if( !ShadowFBO )
	{
		glCreateFramebuffers( 1, &ShadowFBO );
		ShadowFBO.SetLabel( "Shadow FBO" );

		glNamedFramebufferDrawBuffer( ShadowFBO, GL_NONE );
		glNamedFramebufferReadBuffer( ShadowFBO, GL_NONE );
	}

	ShadowMapCubeArray.clear(); ShadowMapViewArray.clear();
	glCreateTextures( GL_TEXTURE_CUBE_MAP_ARRAY, 1, &ShadowMapCubeArray );
	glGenTextures( 1, &ShadowMapViewArray );

	ShadowMapCount = AlignValNonPOT( cnt, 6 );
	
	glTextureStorage3D( ShadowMapCubeArray, 1, GL_DEPTH_COMPONENT32F, size, size, ShadowMapCount );
	glTextureView( ShadowMapViewArray, GL_TEXTURE_2D_ARRAY, ShadowMapCubeArray, GL_DEPTH_COMPONENT32F, 0, 1, 0, ShadowMapCount );

	ShadowMapCubeArray.SetLabel( "Shadow map textures (cube array)" );
	ShadowMapViewArray.SetLabel( "Shadow map textures (2D array)" );

	vec4 border = vec4( 1, 0, 0, 0 ); // depth nearest camera

	glTextureParameteri( ShadowMapCubeArray, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTextureParameteri( ShadowMapCubeArray, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTextureParameteri( ShadowMapViewArray, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTextureParameteri( ShadowMapViewArray, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

	glTextureParameteri( ShadowMapViewArray, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER );
	glTextureParameteri( ShadowMapViewArray, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER );
	glTextureParameteri( ShadowMapViewArray, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER );
	glTextureParameterfv( ShadowMapViewArray, GL_TEXTURE_BORDER_COLOR, &border[0] );

	glTextureParameteri( ShadowMapCubeArray, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE );
	glTextureParameteri( ShadowMapViewArray, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE );
	glTextureParameteri( ShadowMapCubeArray, GL_TEXTURE_COMPARE_FUNC, GL_GEQUAL );
	glTextureParameteri( ShadowMapViewArray, GL_TEXTURE_COMPARE_FUNC, GL_GEQUAL );

	glNamedFramebufferTexture( ShadowFBO, GL_DEPTH_ATTACHMENT, ShadowMapViewArray, 0 );

	if( glCheckNamedFramebufferStatus( FrameBuffer, GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE ) TRACE( DebugLevel::Error, "Shadow FBO not complete\n" );
}

inline void GameRenderHandler::CreateAndInitFBO( glm::ivec2 size )
{
	OPTICK_EVENT();

	if( !FrameBuffer )
	{
		glCreateFramebuffers( 1, &FrameBuffer );
		FrameBuffer.SetLabel( "backbuffer FBO" );

		//GLenum attachments[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
		//glNamedFramebufferDrawBuffers( FrameBuffer, 2, attachments );
		glNamedFramebufferDrawBuffer( FrameBuffer, GL_COLOR_ATTACHMENT0 );
	}

	for( int i = 0; i < 3; i++ ) FrameBufferTextures[i].clear(); 
	FrameBufferRenderbuffer.clear();

	glCreateTextures( GL_TEXTURE_2D, 3, &FrameBufferTextures[0] );
	//glCreateRenderbuffers( 1, &FrameBufferRenderbuffer );

	FrameBufferTextures[0].SetLabel( "backbuffer color" );
	//FrameBufferTextures[1].SetLabel( "backbuffer normal" );
	FrameBufferTextures[2].SetLabel( "backbuffer depth/stencil" );
	//FrameBufferRenderbuffer.SetLabel( "backbuffer stencil" );

	glTextureStorage2D( FrameBufferTextures[0], 1, /*GL_SRGB8_ALPHA8*/GL_SRGB8, size.x, size.y);
	//glTextureStorage2D( FrameBufferTextures[1], 1, GL_RGB10_A2, size.x, size.y );
	glTextureStorage2D( FrameBufferTextures[2], 1, GL_DEPTH_COMPONENT32F, size.x, size.y );
	//glNamedRenderbufferStorage( FrameBufferRenderbuffer, GL_STENCIL_INDEX8, size.x, size.y );

	glNamedFramebufferTexture( FrameBuffer, GL_COLOR_ATTACHMENT0, FrameBufferTextures[0], 0 );
	//glNamedFramebufferTexture( FrameBuffer, GL_COLOR_ATTACHMENT1, FrameBufferTextures[1], 0 );
	glNamedFramebufferTexture( FrameBuffer, GL_DEPTH_ATTACHMENT, FrameBufferTextures[2], 0 );
	//glNamedFramebufferRenderbuffer( FrameBuffer, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, FrameBufferRenderbuffer );

	if( glCheckNamedFramebufferStatus( FrameBuffer, GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE ) TRACE( DebugLevel::Error, "Framebuffer not complete\n" );
}

bool GameRenderHandler::OnCreate()
{
	OPTICK_EVENT();

	TRACE( DebugLevel::Log, "Renderer: On create - start\n" );

	if( !gladLoadGL( glfwGetProcAddress ) )
	{
		TRACE( DebugLevel::FatalError, "%s", "Cannot initialize OpenGL!" );
		return false;
	}

	GLAD_GL_ARB_bindless_texture &= !FOR_RENDERDOC;

	/*
	glbinding::initialize( glfwGetProcAddress );
	glbinding::setCallbackMask( glbinding::CallbackMask::Unresolved );
	glbinding::setUnresolvedCallback( []( const glbinding::AbstractFunction &unresolved ) {
		TRACE( DebugLevel::Error, "Program tried to reference unresolved function: %s !!!!!!!!!!!!!!\n", unresolved.name() );
	} );
	*/
	{
		ostringstream out;
		out << "Vendor: " << glGetString( GL_VENDOR ) << "\n";
		out << "Renderer: " << glGetString( GL_RENDERER ) << "\n";
		out << "Version: " << glGetString( GL_VERSION ) << "\n";
		out << "GLFW version: " << glfwGetVersionString() << "\n";

		TRACE( DebugLevel::Log, "%s", move( out ).str().c_str() );

		GLint major, minor;
		glGetIntegerv( GL_MAJOR_VERSION, &major );
		glGetIntegerv( GL_MINOR_VERSION, &minor );

		// glGetIntegerv with GL_MAJOR_VERSION and GL_MINOR_VERSION only work with opengl >= 3.0
		if( glGetError() != GL_NO_ERROR || major < 3 || (major == 3 && minor < 3) )
		{
			TRACE( DebugLevel::FatalError, "OpenGL version is less than minimum required version - 3.3\n" );
			return false;
		}
	}

	{
		ostringstream out;
		GLint numExt;
		glGetIntegerv( GL_NUM_EXTENSIONS, &numExt );
		out << "Avaiable GL extensions (" << numExt << "): \n";
		for( int i = 0; i < numExt; i++ ) out << glGetStringi( GL_EXTENSIONS, i ) << " ";

		TRACE( DebugLevel::Log, "%s\n", move( out ).str().c_str() );
	}

	//GLExtensions.Query();
	GLParameters.Query();
	
	//EXT_AVAIABLE( GL_ARB_bindless_texture ) = false;

	//if( EXT_AVAIABLE( GL_KHR_debug ) )
	//{
		glEnable( GL_DEBUG_OUTPUT );
		glDisable( GL_DEBUG_OUTPUT_SYNCHRONOUS );
		glDebugMessageCallback( GLDebugMessageCallback, NULL );
	//}
	//else if( EXT_AVAIABLE( GL_ARB_debug_output ) )
	//{
	//	glEnable( GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB );
	//	glDebugMessageCallbackARB( GLDebugMessageCallback, NULL );
	//}
	//else TRACE( DebugLevel::Warning, "GL_ARB_debug_output and GL_KHR_debug not avaiable\n" );
	
	/*
	// Make sure all required extensions are avaiable
	{
		bool ok = true;
		string errStr = "Some required extensions not avaiable!: ";
		for( const GLextentionDef_t &d : ExtensionsRequired )
		{
			if( !GLExtensions[ d.ext ] )
			{
				errStr += d.name + ' ';
				ok = false;
			}
		}

		if( !ok )
		{
			TRACE( DebugLevel::FatalError, "%s\n", errStr.c_str() );
			return false;
		}
	}
	*/

	// set depth space to [0,1]
	glClipControl( GL_LOWER_LEFT, GL_ZERO_TO_ONE );
	
	// we don't save on disk the compressed textures
	glHint( GL_TEXTURE_COMPRESSION_HINT, GL_FASTEST );

	g_BufAllocator = new GLBufferAllocator();
	g_BufAllocator->Create();

	// some normal setup
	glDepthFunc( GL_GEQUAL ); // we use reversed depth buffer
	glFrontFace( GL_CCW ); // CCW culling

	glEnable( GL_FRAMEBUFFER_SRGB );
	glEnable( GL_TEXTURE_CUBE_MAP_SEAMLESS );

	TRACE( DebugLevel::Debug, "Renderer: On create - meshes\n" );

	const char
		*peachCastleName = MODELS_BASE_PATH "princess peaches castle (outside).obj",
		*cubeName = MODELS_BASE_PATH "cube.obj",
		*texCubeName = MODELS_BASE_PATH "cube2.obj",
		*spaceShuttleName = MODELS_BASE_PATH "SpaceShuttle.obj",
		*cubeRoomName = MODELS_BASE_PATH "CubeRoom.obj",
		*teapotName = MODELS_BASE_PATH "teapot.obj",
		*peach2Name = MODELS_BASE_PATH "SM64/Peaches Castle.obj",
		*wolf = MODELS_BASE_PATH "wolf/Wolf-Blender-2.82a.gltf",
		*bistro = "../deps/src/bistro/Interior/interior.obj";

	bool ok = true;

	{
		Assimp::Importer AssimpImp;
		ok &= box.Load( wolf, AssimpImp );
		ok &= room.Load( cubeRoomName, AssimpImp );
		ok &= object.Load( peach2Name, AssimpImp );
	}

	if( !ok ) { TRACE( DebugLevel::Error, "Cannot load meshes\n" ); }
	ok = true;
	/*
	TRACE( DebugLevel::Debug, "Renderer: On create - fonts\n" );

	if( FT_Error err = FT_Init_FreeType( &FreeTypeLib ) ) 
	{ 
		const char *why = FT_Error_String( err );
		TRACE( DebugLevel::Error, "FT_Init_FreeType returned error (%d%s%s)\n", err, why ? ": " : "", why ? why : "" );
	}
	else
	{
		const char *fontPath = FONTS_BASE_PATH "arial.ttf";
		ok = fontArial.Load( FreeTypeLib, fontPath, 24 );

		if( !ok ) { TRACE( DebugLevel::Error, "Cannot load font form %s\n", fontPath ); };// return false; }
		ok = true;
	}
	*/
	TRACE( DebugLevel::Debug, "Renderer: On create - shaders\n" );

	//if( GLAD_GL_ARB_parallel_shader_compile ) glMaxShaderCompilerThreadsARB( 64 );

	//ok &= GLShader::AddIncudeDir( SHADER_BASE_PTH );

	{
		GLShader v, f;
		ok &= v.Load( SHADER_BASE_PTH "fontShader.vert", GL_VERTEX_SHADER );
		ok &= f.Load( SHADER_BASE_PTH "fontShader.frag", GL_FRAGMENT_SHADER );
		ok &= fontProgram.Create( { v,f } );
	}

	{
		GLShader v, f;
		ok &= v.Load( SHADER_BASE_PTH "3d.vert", GL_VERTEX_SHADER );
		ok &= f.Load( SHADER_BASE_PTH "genericTextured.frag", GL_FRAGMENT_SHADER );
		ok &= CommonProgram.Create( { v,f } );
	}

	{
		GLShader v, g, f;
		ok &= v.Load( SHADER_BASE_PTH "3dShadow.vert", GL_VERTEX_SHADER );
		ok &= g.Load( SHADER_BASE_PTH "shadow.geom", GL_GEOMETRY_SHADER );
		ok &= f.Load( SHADER_BASE_PTH "shadow.frag", GL_FRAGMENT_SHADER );
		ok &= ShadowProgram.Create( { v,g,f } );
		CreateAndInitShadowFBO( 1, 1 );
		//SetUpShadowPipeline( true );
	}

	{
		GLShader v, t, b;
		ok &= v.Load( SHADER_BASE_PTH "postprocess.vert", GL_VERTEX_SHADER );
		ok &= t.Load( SHADER_BASE_PTH "toonFilter.frag", GL_FRAGMENT_SHADER );
		ok &= b.Load( SHADER_BASE_PTH "blitFilter.frag", GL_FRAGMENT_SHADER );
		ok &= PostprocessVertex.Create( { v }, true );
		ok &= PostprocessBlit.Create( { b }, true );
		ok &= PostprocessToon.Create( { t }, true );
		ok &= PostprocessPipeline.Create();
		PostprocessPipeline.bind( PostprocessVertex );
	}

	{
		GLShader v, f;
		ok &= v.Load( SHADER_BASE_PTH "collisionRender.vert", GL_VERTEX_SHADER );
		ok &= f.Load( SHADER_BASE_PTH "collisionRender.frag", GL_FRAGMENT_SHADER );
		ok &= StreamDrawProgram.Create( { v,f } );
	}

	{
		GLShader v, f;
		ok &= v.Load( SHADER_BASE_PTH "ImGui.vert", GL_VERTEX_SHADER );
		ok &= f.Load( SHADER_BASE_PTH "ImGui.frag", GL_FRAGMENT_SHADER );
		ok &= ImGuiProgram.Create( { v,f } );
	}

	if( !ok ) { TRACE( DebugLevel::Error, "Cannot load shaders\n" ); };// return false; }
	ok = true;

	TRACE( DebugLevel::Debug, "Renderer: On create - buffers\n" );

	glCreateBuffers( 1, &Mats );
	glCreateBuffers( 1, &Lights );
	glNamedBufferStorage( Mats, sizeof( glsl::MatsVP ), NULL, GL_DYNAMIC_STORAGE_BIT );
	glNamedBufferStorage( Lights, sizeof( glsl::Lights ), NULL, GL_DYNAMIC_STORAGE_BIT );

	// set buffer to zero;
	glClearNamedBufferData( Mats, GL_RGBA32UI, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, NULL );
	glClearNamedBufferData( Lights, GL_RGBA32UI, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, NULL );

	// bind them to correct binding points
	glBindBufferBase( GL_UNIFORM_BUFFER, UBO_MATS_VP_BINDING, Mats );
	glBindBufferBase( GL_UNIFORM_BUFFER, UBO_LIGHTS_BINDING, Lights );

	TRACE( DebugLevel::Debug, "Renderer: On create - VAOs\n" );

	{
		glCreateVertexArrays( 1, &PostprocessVAO );
		PostprocessVAO.SetLabel( "Postprocess VAO" );
	}

	{
		glCreateVertexArrays( 1, &ImGuiVAO );

		glEnableVertexArrayAttrib( ImGuiVAO, 0 );
		glEnableVertexArrayAttrib( ImGuiVAO, 1 );
		glEnableVertexArrayAttrib( ImGuiVAO, 2 );

		glVertexArrayAttribFormat( ImGuiVAO, 0, 2, GL_FLOAT, GL_FALSE, 
			IM_OFFSETOF( ImDrawVert, pos ) );
		glVertexArrayAttribFormat( ImGuiVAO, 1, 2, GL_FLOAT, GL_FALSE,
			IM_OFFSETOF( ImDrawVert, uv ) );
		glVertexArrayAttribFormat( ImGuiVAO, 2, 4, GL_UNSIGNED_BYTE, GL_TRUE,
			IM_OFFSETOF( ImDrawVert, col ) );

		glVertexArrayAttribBinding( ImGuiVAO, 0, 0 );
		glVertexArrayAttribBinding( ImGuiVAO, 1, 0 );
		glVertexArrayAttribBinding( ImGuiVAO, 2, 0 );
	}

	// Create
	CreateAndInitFBO( { 1,1 } );

	int texture_units;
	glGetIntegerv( GL_MAX_TEXTURE_IMAGE_UNITS, &texture_units );
	TRACE( DebugLevel::Log, "texture units: %d\n", texture_units );

	StreamDrawSimple.Create(
		{ 
			{ 0, 3, GL_FLOAT, GL_FALSE, 0 },
			{ 1, 4, GL_UNSIGNED_BYTE, GL_TRUE, 3*sizeof(float) }
		},
		{ 
			{ 3 * sizeof( float ) + 4 }
		}
	);

	TRACE( DebugLevel::Debug, "Renderer: On create - ImGui font setup\n" );

	{
		ImGuiIO &io = ImGui::GetIO();
		io.BackendFlags |=
			ImGuiBackendFlags_RendererHasVtxOffset;

		ImFontConfig cfg = {};
		cfg.FontDataOwnedByAtlas = false;
		cfg.RasterizerMultiply = 1.5f;
		cfg.SizePixels = 768.f / 32.f;
		cfg.PixelSnapH = true;
		cfg.OversampleH = 4;
		cfg.OversampleV = 4;

		ImFont *Font = io.Fonts->AddFontFromFileTTF( 
			FONTS_BASE_PATH "OpenSans-Light.ttf", cfg.SizePixels, &cfg );

		unsigned char *pixels = NULL;
		int width, height;

		io.Fonts->GetTexDataAsAlpha8( &pixels, &width, &height );

		glCreateTextures( GL_TEXTURE_2D, 1, &ImGuiFontAtlas );
		glTextureParameteri( ImGuiFontAtlas, GL_TEXTURE_MAX_LEVEL, 0 );
		glTextureParameteri( ImGuiFontAtlas, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		glTextureParameteri( ImGuiFontAtlas, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		glTextureStorage2D( ImGuiFontAtlas, 1, GL_R8, width, height );
		GLint swizzles[4] = { GL_ONE, GL_ONE, GL_ONE, GL_RED };
		glTextureParameteriv( ImGuiFontAtlas, GL_TEXTURE_SWIZZLE_RGBA, swizzles );

		glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
		glTextureSubImage2D( ImGuiFontAtlas, 0, 0, 0,
			width, height, GL_RED, GL_UNSIGNED_BYTE, pixels );
		glPixelStorei( GL_UNPACK_ALIGNMENT, 4 );

		io.Fonts->SetTexID( reinterpret_cast<ImTextureID>( (intptr_t)(GLuint)ImGuiFontAtlas ) );
		
		io.FontDefault = Font;
		io.DisplayFramebufferScale = ImVec2( 1, 1 );
	}

	MSPF = 0;
	TRACE( DebugLevel::Log, "Renderer: On create - end\n" );

	//glFinish();

	return true;
}

void GameRenderHandler::OnDraw( const void *_FData )
{
	OPTICK_EVENT();

	//glFinish();
// TODO: Why do I have to have this?
	

	FrameData *FData = (FrameData *)_FData;
	auto FrameStartTime = chrono::high_resolution_clock::now();

	if( LastVSync != FData->bVSync ) glfwSwapInterval( ( LastVSync = FData->bVSync ) ? 1 : 0 );

	if( FData->WndSize != LastWndSize )
	{
		LastWndSize = FData->WndSize;

		CreateAndInitFBO( FData->WndSize );
		InvWndSize = 1.f / vec2( LastWndSize );
	}

	if( FData->bReload )
	{
		CommonProgram.Reload();
		ShadowProgram.Reload();
		fontProgram.Reload();
	}

	{
		GLenum attachments[] = { GL_COLOR, GL_DEPTH, GL_STENCIL };
		//glInvalidateFramebuffer( GL_FRAMEBUFFER, (GLsizei)size(attachments), attachments );
		glClear( GL_COLOR_BUFFER_BIT );
	}
	
	chrono::high_resolution_clock::time_point t1;
	{
		t1 = chrono::high_resolution_clock::now();
		glNamedBufferSubData( Mats, 0, sizeof( glsl::MatsVP ), &FData->oMats );

		//CamCollider.Create( View, fovy, aspect, near, far );
		LowPassFilter( tMatrices, chrono::duration<float>( chrono::high_resolution_clock::now() - t1 ).count() * 1'000'000 );
	}

	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );	

	{
		t1 = chrono::high_resolution_clock::now();

		int NewSize = FData->iShadowMapSize;

		if( 
			ShadowMapSize != NewSize ||
			(int)ShadowMapCount < FData->iUsedShadowMapCnt || 
			(int)ShadowMapCount > FData->iUsedShadowMapCnt * 2
			) 
			CreateAndInitShadowFBO( max( FData->iUsedShadowMapCnt, 1 ), NewSize );

		ShadowMapSize = NewSize;

		glNamedBufferSubData( Lights, 0, sizeof( glsl::Lights ), &FData->oLightsDat );

		LowPassFilter( tLights, chrono::duration<float>( chrono::high_resolution_clock::now() - t1 ).count() * 1'000'000 );
	}

	struct ObjectToDraw
	{
		ObjectToDraw( const Model &o ) { model = &o; transform = transformIT = mat4( 1 ); }
		ObjectToDraw( const Model &o, const mat4 &t ) { model = &o; transform = t; transformIT = glm::transpose( glm::inverse( t ) ); }
		ObjectToDraw( const Model &o, const mat4 &t, const mat4 &tIT ) { model = &o; transform = t; transformIT = tIT; }
		const Model *model;
		mat4 transform, transformIT;
	};

	std::vector< ObjectToDraw > objectsToDraw;

	{
		mat3 m = mat3(
			vRight,
			vUp,
			vFront
		);

		objectsToDraw.emplace_back( object, m, glm::transpose( glm::inverse( mat4(-m)) ) );

		objectsToDraw.emplace_back( room, 
		glm::scale(
			glm::translate( mat4( 1.f ), vec3( 0, -30, 0 ) ),
			vec3( 20 ) ) );

		objectsToDraw.emplace_back( box, 
			glm::scale( 
				glm::rotate( 
					glm::translate( 
						mat4( 1 ), 
						vec3( 0, 20 * ( 1 + sin( FData->fTime ) ), 4 ) ),
					FData->fTime, 
					vec3( 0.2f, 1, -1 ) ), 
				vec3( 1.1f ) ) );
	}
	/*
	t1 = chrono::high_resolution_clock::now();
	struct streamData
	{
		glm::vec3p pos;
		glm::uint col; // packed rgba8
	};
	std::vector< streamData > CollisionBoxes;	

	std::vector<size_t> modelsVisibleStartIdx( objectsToDraw.size() + 1 );
	modelsVisibleStartIdx[0] = 0;
	for( size_t i = 0; i < objectsToDraw.size(); i++ )
		modelsVisibleStartIdx[i+1] = modelsVisibleStartIdx[i] + objectsToDraw[i].model->GetOBBcnt();

	std::vector<OBB> obbs( modelsVisibleStartIdx.back() );
	for( size_t i = 0; i < objectsToDraw.size(); i++ )
	{
		ObjectToDraw &d = objectsToDraw[i];
		size_t st = modelsVisibleStartIdx[i], en = modelsVisibleStartIdx[i+1];
		d.model->GetOBBS( &obbs[st] );
		std::for_each( &obbs[0] + st, &obbs[0] + en, [&d]( OBB &o ) { o.rotate( d.transform ); } );
	}

	std::vector<bool> visible = CamCollider.AreVisible( obbs ).visible;

	if( FData->bBoundingBoxes )
	{
		for( int i = 0; i < (int)obbs.size(); i++ )
		{
			auto pos = obbs[i].GetVertices();
			glm::uint color = glm::packUnorm4x8( glm::vec4( (visible[i] ? WHITE : RED).rgb, 0.3f ) );

			for( int idx : *obbs[i].Lines() )
				CollisionBoxes.push_back( streamData{ (*pos)[idx], color } );
		}

		{
			auto pos = CamCollider.GetVertices();

			for( int idx : *CamCollider.Lines() )
				CollisionBoxes.push_back( streamData{ (*pos)[idx], 0xFFFF00FF } );
		}
	}

	LowPassFilter( tFrustrumTest, chrono::duration<float>( chrono::high_resolution_clock::now() - t1 ).count() * 1'000'000 );
	*/


	FINISH();

	
	
	//glEnable( GL_RASTERIZER_DISCARD );
	t1 = chrono::high_resolution_clock::now();
	for( int i = 0; i < 2; i++ )
	{
		OPTICK_EVENT( "RenderPass" );
		OPTICK_TAG( "PassData", i );
		OPTICK_TAG( "PassName", i == 0 ? "depth" : "color" );

		//if( i == 0 ) i++;

		GLProgram *currVert = NULL;
		GLProgram *currFrag = NULL;

		float DefaultDepth = 0.f;
		float DefaultColor[] = { 0.2f, 0.2f, 0.2f, 1 };
		float DefaultNormal[] = { 0.5f, 0.5f, 0.5f, 0 };
		
		switch( i )
		{
		case 0:
			glBindFramebuffer( GL_FRAMEBUFFER, ShadowFBO );			
			
			//glClearBufferfv( GL_DEPTH, 0, &DefaultDepth );
			//glClearTexImage( ShadowMapViewArray, 0, GL_DEPTH_COMPONENT, GL_FLOAT, (void *)&DefaultDepth );
			
			
			// only clear visible shadow maps 
			for( int i = 0, firstOK = -1; i < MAX_SHADOW_MAPS + 1; i++ )
			{
				bool mapVisible = i != MAX_SHADOW_MAPS && PACKED_BITS_GET( FData->oLightsDat.baShadowMapActive, i );

				if( firstOK == -1 ) { if( mapVisible ) firstOK = i; }
				else
				{
					if( !mapVisible )
					{
						glClearTexSubImage(
							ShadowMapViewArray, 0, 
							0, 0, firstOK, 
							ShadowMapSize, ShadowMapSize, i - firstOK,
							GL_DEPTH_COMPONENT, GL_FLOAT, (void *)&DefaultDepth );

						firstOK = -1;
					}
				}
			}
			
			glDisable( GL_CULL_FACE );
			glEnable( GL_DEPTH_TEST );
			glDisable( GL_SCISSOR_TEST );

			ShadowProgram.use();
			//ShadowPipeline.bind( ShadowGeom );
			currVert = &ShadowProgram;
			currFrag = &ShadowProgram;

			//glDepthFunc( GL_GEQUAL );
			
			glViewport( 0, 0, ShadowMapSize, ShadowMapSize );

			

			//glCullFace( GL_FRONT );
			break;

		case 1:


			glEnable( GL_BLEND );
			glBlendEquation( GL_FUNC_ADD );
			glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			glEnable( GL_CULL_FACE ); glCullFace( GL_BACK );
			glEnable( GL_DEPTH_TEST );
			glDisable( GL_SCISSOR_TEST );

			glBindFramebuffer( GL_FRAMEBUFFER, FrameBuffer );
			glClearBufferfv( GL_DEPTH, 0, &DefaultDepth );
			glClearBufferfv( GL_COLOR, 0, DefaultColor );
			glClearBufferfv( GL_COLOR, 1, DefaultNormal );
			CommonProgram.use();
			//CommonPipeline.unbind( GL_FRAGMENT_SHADER );
			//currVert = &CommonVert;
			
			glViewport( 0, 0, (GLsizei)FData->WndSize.x, (GLsizei)FData->WndSize.y );
			

			//break;
		//case 2:
			//CommonPipeline.bind( LightingFrag );
			currFrag = &CommonProgram;
			currVert = &CommonProgram;
			
			//glEnable( GL_CULL_FACE );

			//glDepthFunc( GL_EQUAL );

			int maps = GLParameters.MaxFragmentImageUnits - 2;
			int pointmaps = GLParameters.MaxFragmentImageUnits - 1;
			glBindTextureUnit( maps, ShadowMapViewArray );
			glBindTextureUnit( pointmaps, ShadowMapCubeArray );
			currFrag->setInt( "ShadowMaps", maps );
			currFrag->setInt( "ShadowPointMaps", pointmaps );

			//glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );
			break;
		}
		
		GLUniformDSA mModel( currVert->ID, "mModel" );
		GLUniformDSA mModelIT( currVert->ID, "mModelIT" );

		for( int j = 0; j < (int)objectsToDraw.size(); j++ )
		{
			const ObjectToDraw &o = objectsToDraw[j];
			mModel.Set( o.transform );
			mModelIT.Set( o.transformIT );
			o.model->Draw( currFrag );// , visible, modelsVisibleStartIdx[j] );
		}

		switch( i )
		{
		case 0:
			break;

		case 1:
			int maps = GLParameters.MaxFragmentImageUnits - 2;
			int pointmaps = GLParameters.MaxFragmentImageUnits - 1;
			glBindTextureUnit( maps, 0 );
			glBindTextureUnit( pointmaps, 0 );
			break;
		}



		//glTextureBarrier();
		FINISH();		
	}

	
	glUseProgram( 0 );
	LowPassFilter( tDispachRender, chrono::duration<float>( chrono::high_resolution_clock::now() - t1 ).count() * 1'000'000 );
	
	
	//glFinish();

	/*
	if( FData->bBoundingBoxes )
	{
		struct vert {
			glm::vec3p coord;
			glm::uint color;
		};

		vector< vert > data;
		vector< glm::u16 > indeces;
		//auto ind = AABB{}.Lines();

		for( int i = 0; i < MAX_SHADOW_MAPS; i++ )
		{
			if( !PACKED_BITS_GET( actuallShadowMask, i ) ) continue;

			glm::uint color = PACKED_BITS_GET( LightsDat.baShadowMapActive, i ) ? 0xA000FF00u : 0xA0FF0000u;

			auto vert = lightCameras[i].GetVertices();

			glm::u16 base = (glm::u16)data.size();
			for( auto &q : *vert )
				data.push_back( { glm::vec3p( q ), color } );
			//
			//for( auto &q : *ind )
			//	indeces.push_back( (glm::u16)( q + i * ind->size() ) );
			auto lines = lightCameras[i].Lines();
			for( auto idx : *lines )
				indeces.push_back( base + idx ); //data.push_back( { glm::vec3p( (*vert)[idx] ), color } );
		}

		glEnable( GL_DEPTH_TEST );
		StreamDrawProgram.use();
		const void *a[1] = { data.data() };
		StreamDrawSimple.DrawStreamElements( a, (GLsizei)data.size(), indeces.data(), (GLsizei)indeces.size(), GL_LINES, GL_UNSIGNED_SHORT );
	}
	*/

	
	
	{
		OPTICK_EVENT( "Blit framebuffer" );
		glDisable( GL_CULL_FACE );
		glViewport( 0, 0, (GLsizei)FData->WndSize.x, (GLsizei)FData->WndSize.y );
		//glMemoryBarrier( GL_FRAMEBUFFER_BARRIER_BIT );

		glBindFramebuffer( GL_FRAMEBUFFER, 0 );
		glBlitNamedFramebuffer( FrameBuffer, 0, 0, 0, FData->WndSize.x, FData->WndSize.y, 0, 0, FData->WndSize.x, FData->WndSize.y, GL_COLOR_BUFFER_BIT, GL_NEAREST );

		{
			GLenum attachmentsDepth[] = { GL_DEPTH_ATTACHMENT };
			GLenum attachmentsCommon[] = { GL_DEPTH_ATTACHMENT,  GL_COLOR_ATTACHMENT0,  GL_COLOR_ATTACHMENT1 };
			glInvalidateNamedFramebufferData( ShadowFBO, 1, attachmentsDepth );
			glInvalidateNamedFramebufferData( FrameBuffer, 3, attachmentsCommon );
		}

		FINISH();
	}

	
	//glClear( GL_COLOR_BUFFER_BIT );
	//glDisable( GL_RASTERIZER_DISCARD );
	//glClear( GL_COLOR_BUFFER_BIT );
	//glFinish();
	
	//if( FData->bBoundingBoxes && CollisionBoxes.size() > 0 )
	//{
	//	StreamDrawProgram.use();
	//	const void *a[1] = { CollisionBoxes.data() };
	//	StreamDrawSimple.DrawStreamArray( a, (GLsizei)CollisionBoxes.size(), GL_LINES );
	//}	

	/*
	glDisable( GL_DEPTH_TEST );

	{	// draw text
		OPTICK_EVENT( "Render Text" );
		char s[1024];

		float siz = (float)ShadowMapSize * ShadowMapSize * ShadowMapCount * 4;
		char c;
		if( siz < 1e3 ) c = ' ';
		else if( siz < 1e6 ) c = 'k', siz /= 1e3;
		else if( siz < 1e9 ) c = 'M', siz /= 1e6;
		else c = 'G', siz /= 1e9;

#define FlFM "%.3f"
		snprintf( s, 1023, 
			"shadow map size: %dx%d (%d) [%.1f%cb]\r" FlFM "fps\rRender: " FlFM "ms\rTick: " FlFM "ms\r"
			"matrices: " FlFM "us\rlights: " FlFM "us\rfrustrum test: " FlFM "us\rdispach render : " FlFM "us\rUI/text: " FlFM "us\r"
			"pos: " FlFM " " FlFM " " FlFM,
				ShadowMapSize, ShadowMapSize, ShadowMapCount, siz, c,
				  FData->FPS, (float)MSPF, (float)FData->MSPT, 
			tMatrices, tLights, tFrustrumTest, tDispachRender, tText, 
			FData->CamPos.x, FData->CamPos.y, FData->CamPos.z );

		s[1023] = '\0';
#undef FlFM

		glEnable( GL_BLEND );
		glBlendEquation( GL_FUNC_ADD );
		glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		glDisable( GL_CULL_FACE );
		glDisable( GL_DEPTH_TEST );
		glEnable( GL_SCISSOR_TEST );

		{
			auto t1 = chrono::high_resolution_clock::now();

			fontProgram.use();
			fontProgram.setMat4( "mProjection", glm::ortho( 0.f, (float)FData->WndSize.x, 0.f, (float)FData->WndSize.y ) );

			fontArial.RenderText( &fontProgram, s, { 0,0 }, WHITE, 0.5f );
		}

		glDisable( GL_BLEND );
		FINISH();
	}
	*/


	{
		OPTICK_EVENT( "Render ImGui" );
		
		glEnable( GL_BLEND );
		glBlendEquation( GL_FUNC_ADD );
		glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		glDisable( GL_CULL_FACE );
		glDisable( GL_DEPTH_TEST );
		glEnable( GL_SCISSOR_TEST );

		ImGuiProgram.use();

		ImDrawData *draw_data = &FData->ImGuiDrawData;

		const float L = draw_data->DisplayPos.x;
		const float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
		const float T = draw_data->DisplayPos.y;
		const float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;

		const mat4 proj = glm::ortho( L, R, B, T );
		glNamedBufferSubData( Mats, 0, sizeof( mat4 ), glm::value_ptr( proj ) ); // reuse Mats (already bound to 0)

		GLuint bound = -1;

		glBindVertexArray( ImGuiVAO );

		for( int n = 0; n < draw_data->CmdListsCount; n++ )
		{
			const ImDrawList *cmd_list = draw_data->CmdLists[n];

			void *data = g_BufAllocator->allocAligned(
				cmd_list->VtxBuffer.Size * sizeof( ImDrawVert ) +
				( cmd_list->IdxBuffer.Size + 1 ) * sizeof( ImDrawIdx ),
				sizeof(ImDrawVert) );

			intptr_t ElementsOffset = 
				AlignVal( (intptr_t)( cmd_list->VtxBuffer.Size * sizeof( ImDrawVert ) + g_BufAllocator->GetCurrentOffset() ), sizeof( ImDrawIdx ) )
				-  g_BufAllocator->GetCurrentOffset();

			void *elementData = (char*)data + ElementsOffset;
			intptr_t startElements = ElementsOffset + g_BufAllocator->GetCurrentOffset();

			void *vertexData = data;
			intptr_t startVertexIdx = g_BufAllocator->GetCurrentOffset() / sizeof( ImDrawVert );

			memcpy( vertexData, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof( ImDrawVert ) );
			memcpy( elementData, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof( ImDrawIdx ) );

			g_BufAllocator->Flush();

			//glNamedBufferSubData( g_BufAllocator->ID(), g_BufAllocator->GetCurrentOffset(), cmd_list->VtxBuffer.Size * sizeof( ImDrawVert ), cmd_list->VtxBuffer.Data );
			//glNamedBufferSubData( g_BufAllocator->ID(), startElements, cmd_list->IdxBuffer.Size * sizeof( ImDrawIdx ), cmd_list->IdxBuffer.Data );

			//glMemoryBarrier( GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_ELEMENT_ARRAY_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT );

			//if( bound != g_BufAllocator->ID() )
			//{
				bound = g_BufAllocator->ID();
				glVertexArrayElementBuffer( ImGuiVAO, bound );
				glVertexArrayVertexBuffer( ImGuiVAO, 0, bound, 0, sizeof( ImDrawVert ) );
			//}

			for( int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++ )
			{
				const ImDrawCmd *pcmd = &cmd_list->CmdBuffer[cmd_i];

				if( pcmd->UserCallback )
				{
					TRACE( DebugLevel::Warning, "ImGui requested user callback!" );
					continue;
				}

				const ImVec4 cr = pcmd->ClipRect;

				glScissor(
					(int)cr.x, (int)(FData->WndSize.y - cr.w),
					(int)(cr.z - cr.x), (int)(cr.w - cr.y) );
				glBindTextureUnit( 0, (GLuint)reinterpret_cast<intptr_t>(pcmd->GetTexID()) );

				glDrawElementsBaseVertex(
					GL_TRIANGLES,
					(GLsizei)pcmd->ElemCount,
					sizeof( ImDrawIdx ) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT,
					(ImDrawIdx*)startElements + pcmd->IdxOffset,
					(GLint)( startVertexIdx + pcmd->VtxOffset ) );
			}
		}

		glVertexArrayElementBuffer( ImGuiVAO, 0 );
		glVertexArrayVertexBuffer( ImGuiVAO, 0, 0, 0, 0 );
		glBindTextureUnit( 0, 0 );

		glBindVertexArray( 0 );
		glScissor( 0, 0, FData->WndSize.x, FData->WndSize.y );

		FINISH();
	}
	


	LowPassFilter( tText, chrono::duration<float>( chrono::high_resolution_clock::now() - t1 ).count() * 1'000'000 );

	MSPF = 1000.f * chrono::duration<float>( chrono::high_resolution_clock::now() - FrameStartTime ).count();
}

/*
void GameRenderHandler::ShadowMapAllocator_t::AddFreeBlock( int pos, int len )
{
	FreeBlocks[pos] = len;

	auto p = FreeBlocks.find( pos );
	auto next = p; next++;
	auto prev = p; prev--;

	bool a = prev != FreeBlocks.end() && prev->first + prev->second == pos;
	bool b = next != FreeBlocks.end() && pos + len == next->first;

	if( a || b )
	{
		int st = pos, ln = len;

		if( a ) st = prev->first, ln += prev->second;
		if( b ) ln += next->second;

		if( a && b ) FreeBlocks.erase( prev, std::next( next ) );
		else if( a ) FreeBlocks.erase( prev, std::next( p ) );
		else if( b ) FreeBlocks.erase( p, std::next( next ) );

		FreeBlocks[st] = ln;
	}
}

void GameRenderHandler::ShadowMapAllocator_t::RemoveFreeBlock( int pos, int cnt )
{
	auto a = FreeBlocks.upper_bound( pos );
	a--;

	int start = a->first;
	int len = a->second;
	int alignedLen = len - ( pos - start );

	assert( alignedLen >= cnt );

	if( pos == start )	FreeBlocks.erase( start );
	else				a->second = pos - start;

	if( cnt != alignedLen ) FreeBlocks[pos + cnt] = alignedLen - cnt;
}

int GameRenderHandler::ShadowMapAllocator_t::Alloc( int cnt, int align )
{
	auto algn = [align]( int v ) { return ( ( v + ( align - 1 ) ) / align ) * align; };

	for( auto &a : FreeBlocks )
	{
		int alignedStart = algn( a.first );
		int alignedLen = a.second - ( alignedStart - a.first );

		if( cnt <= alignedLen )
		{
			RemoveFreeBlock( alignedStart, cnt );
			AllocatedBlocks[alignedStart] = cnt;

			return alignedStart;
		}
	}

	return -1;//-( algn( ShadowMapCount + cnt ) );
}

int GameRenderHandler::ShadowMapAllocator_t::Free( int pos )
{
	assert( AllocatedBlocks.find( pos ) != AllocatedBlocks.end() );

	int l = AllocatedBlocks[pos];
	AllocatedBlocks.erase( pos );

	AddFreeBlock( pos, l );
	return l;
}
*/
