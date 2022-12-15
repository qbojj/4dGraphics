#include "GameCore.h"

#include "Debug.h"

#include "optick.h"
#include <SDL2/SDL.h>
#include <string.h>
#include <exception>
#include <imgui.h>
#include <glslang/Public/ShaderLang.h>

#include <imgui_impl_sdl.h>

ImGuiRIIAContext::ImGuiRIIAContext(ImGuiRIIAContext&&o) : context(o.context) { o.context = nullptr; };
ImGuiRIIAContext::ImGuiRIIAContext(ImFontAtlas*font) : context(ImGui::CreateContext(font)) 
{
    if( !context ) throw std::runtime_error( "Cannot initialize ImGui" );
};
ImGuiRIIAContext::~ImGuiRIIAContext() { ImGui::DestroyContext(context); };

ImGuiRIIAContext &ImGuiRIIAContext::operator=(ImGuiRIIAContext&&o)
{ 
    if( this != &o ) { context = o.context; o.context = nullptr; } 
    return *this; 
};
ImGuiRIIAContext::operator ImGuiContext*() { return context; } 

GLSLRIIAContext::GLSLRIIAContext() : active(true)
{ 
    if( !glslang::InitializeProcess() ) throw std::runtime_error( "Cannot initialize glslang" );
}
GLSLRIIAContext::~GLSLRIIAContext() { if( active ) glslang::FinalizeProcess(); }

GLSLRIIAContext::GLSLRIIAContext(GLSLRIIAContext&&o) : active(o.active) { o.active = false; }

GameEngine::GameEngine( const char *pName, SDL_Window *window )
	: m_szName( pName )
	, m_hWindow( window ? window : Initialize() )
{
	if( !m_hWindow ) throw std::runtime_error( "Could not create window" );
}

GameEngine::~GameEngine()
{
	SDL_DestroyWindow( m_hWindow );
	SDL_Quit();
}

SDL_Window *GameEngine::Initialize()
{
	if( SDL_Init( SDL_INIT_EVENTS | SDL_INIT_VIDEO ) ) return nullptr;

	return SDL_CreateWindow( "4dGraphics", 
		0, 0, 
		0, 0, 
		SDL_WINDOW_VULKAN |
		SDL_WINDOW_RESIZABLE |
		SDL_WINDOW_FULLSCREEN_DESKTOP |
		SDL_WINDOW_ALLOW_HIGHDPI |
		0);
}

/*
void GameEngine::EngineLoop(void* wData)
{
	WindowDataContainer* windowData = (WindowDataContainer*)wData;
	GameEngine* gameEngine = windowData->pGameEngine;
	GLFWwindow* window = windowData->window;

	{
		OPTICK_APP( "4dGraphics" );
	
		void* FData = NULL;
		try
		{
			TRACE( DebugLevel::Log, "Before OnCreate\n" );
			if(
				gameEngine->m_pInputHandler->OnCreate( window ) &&
				gameEngine->m_pGameHandler->OnCreate( window ) &&
				gameEngine->m_pRenderHandler->OnCreate( window )
				)
			{
				TRACE( DebugLevel::Log, "After OnCreate\n" );

				FData = gameEngine->m_pGameHandler->NewFData();

				if( !FData ) throw runtime_error( "couldn't create Frame Data" );

				while( !glfwWindowShouldClose( window ) )
				{
					{
						OPTICK_FRAME_EVENT( Optick::FrameType::CPU );

						{
							OPTICK_CATEGORY( "process messages", Optick::Category::IO );
							glfwPollEvents();
						}

						{
							OPTICK_CATEGORY( "Process new Frame", Optick::Category::GameLogic );
							gameEngine->m_pInputHandler->OnPreTick();
							gameEngine->m_pGameHandler->OnTick( FData, gameEngine->m_pInputHandler );
							gameEngine->m_pInputHandler->OnPostTick();
						}
					}

					
					{
						OPTICK_FRAME_EVENT( Optick::FrameType::Render );

						{
							OPTICK_CATEGORY( "render frame", Optick::Category::Rendering );
							gameEngine->m_pRenderHandler->OnDraw( FData );
						}
					}
				}
			}
			else TRACE( DebugLevel::FatalError, "OnCreate returned false\n" );
		}
		catch( const std::exception &e ) { TRACE( DebugLevel::FatalError, "Exception caught: %s\n", e.what() ); }
		catch( ShutdownException ) {}
		catch( ... ) { TRACE( DebugLevel::FatalError, "Unknown Exception caught\n" ); }

		TRACE( DebugLevel::Log, "Begin closing\n" );
		glfwSetWindowShouldClose( window, true );

		try
		{
			if (FData) gameEngine->m_pGameHandler->DeleteFData(FData);
		}
		catch (const std::exception& e) { TRACE(DebugLevel::FatalError, "Exception caught in FData delete: %s\n", e.what()); }
		catch (...) { TRACE(DebugLevel::FatalError, "Unknown Exception caught in FData delete\n"); }

		try
		{
			gameEngine->m_pInputHandler->OnDestroy( window );
		}
		catch( const std::exception &e ) { TRACE( DebugLevel::FatalError, "Exception caught: %s\n", e.what() ); }
		catch( ... ) { TRACE( DebugLevel::FatalError, "Unknown Exception caught\n" ); }

		try
		{
			gameEngine->m_pGameHandler->OnDestroy();
		}
		catch( const std::exception &e ) { TRACE( DebugLevel::FatalError, "Exception caught: %s\n", e.what() ); }
		catch( ... ) { TRACE( DebugLevel::FatalError, "Unknown Exception caught\n" ); }

		try
		{
			gameEngine->m_pRenderHandler->OnDestroy();
		}
		catch( const std::exception &e ) { TRACE( DebugLevel::FatalError, "Exception caught: %s\n", e.what() ); }
		catch( ... ) { TRACE( DebugLevel::FatalError, "Unknown Exception caught\n" ); }
	
	}
	TRACE( DebugLevel::Log, "After OnDestroy\n" );
	OPTICK_SHUTDOWN();

	try
	{
		delete gameEngine->m_pInputHandler;
		gameEngine->m_pInputHandler = NULL;
	}
	catch( const std::exception &e ) { TRACE( DebugLevel::FatalError, "Exception caught: %s\n", e.what() ); }
	catch( ... ) { TRACE( DebugLevel::FatalError, "Unknown Exception caught\n" ); }

	
	try
	{
		delete gameEngine->m_pGameHandler;
		gameEngine->m_pGameHandler = NULL;
	}
	catch( const std::exception &e ) { TRACE( DebugLevel::FatalError, "Exception caught: %s\n", e.what() ); }
	catch( ... ) { TRACE( DebugLevel::FatalError, "Unknown Exception caught\n" ); }

	try
	{
		delete gameEngine->m_pRenderHandler;
		gameEngine->m_pRenderHandler = NULL;
	}
	catch( const std::exception &e ) { TRACE( DebugLevel::FatalError, "Exception caught: %s\n", e.what() ); }
	catch( ... ) { TRACE( DebugLevel::FatalError, "Unknown Exception caught\n" ); }
}
*/