#include "stdafx.h"
#include "GameCore.h"

#include "Debug.h"

#include "optick.h"

//#define MULTI_THREADED

#ifdef MULTI_THREADED
#include <atomic>
//#include <semaphore>
#endif

using namespace std;

void ErrorCallback( int code, const char *description )
{
	const char *msg;

	switch( code )
	{
	case GLFW_NO_ERROR: msg = "NO ERROR"; break;
	case GLFW_NOT_INITIALIZED: msg = "NOT INITIALIZED"; break;
	case GLFW_NO_CURRENT_CONTEXT: msg = "NO CURRENT CONTEXT"; break;
	case GLFW_INVALID_ENUM: msg = "INVALID ENUM"; break;
	case GLFW_INVALID_VALUE: msg = "INVALID VALUE"; break;
	case GLFW_OUT_OF_MEMORY: msg = "OUT OF MEMORY"; break;
	case GLFW_API_UNAVAILABLE: msg = "API UNAVAILABLE"; break;
	case GLFW_VERSION_UNAVAILABLE: msg = "VERSION UNAVAILABLE"; break;
	case GLFW_PLATFORM_ERROR: msg = "PLATFORM ERROR"; break;
	case GLFW_FORMAT_UNAVAILABLE: msg = "FORMAT UNAVAILABLE"; break;
	case GLFW_NO_WINDOW_CONTEXT: msg = "NO WINDOW CONTEXT"; break;
	default: msg = "UNKNOWN"; break;
	}

	TRACE( DebugLevel::Error, "GLFW error occured: %s => %s\n", msg, description );
}

using namespace chrono;
//using namespace chrono_literals;

//#define SPINLOCK 0

class WindowDataContainer
{
public:
#ifdef MULTI_THREADED
	atomic<int> currentFrameData = 0;
	void *FData[2] = { NULL };
#endif

	GLFWwindow *window = NULL;
	GameEngine *pGameEngine = NULL;

#ifdef MULTI_THREADED 
	mutex mtInputLock;

	atomic_flag 
		RequestUpdateFlag = ATOMIC_FLAG_INIT,
		UpdateReadyFlag = ATOMIC_FLAG_INIT;

	// start locked
	//std::binary_semaphore
	//	RequestUpdateFlag{ 0 },
	//	UpdateReadyFlag{ 0 };

	atomic<bool> bInputInitialized = false;
	atomic<int> iDestroyed = 0;

	//atomic<bool> bClose = false;
	atomic_flag bClose = ATOMIC_FLAG_INIT;

	//high_resolution_clock::time_point tRequestUpdate, tUpdateReady;
	// returns 1 if window should close
	inline bool WaitRequestUpdate()
	{
		/*
		if( !bRequestUpdate )
		{
			
#if SPINLOCK
			while( !ShouldClose() && !bRequestUpdate ) this_thread::yield();
#else
			unique_lock lock( mtRequestUpdate );
			cvRequestUpdate.wait( lock, [this]() { return ShouldClose() || bRequestUpdate; } );
#endif

			float dt = duration<float>( high_resolution_clock::now() - tRequestUpdate ).count() * 1000; // ms
			if( dt > 1 ) TRACE( DebugLevel::Debug, "WaitRequestUpdate %fms\n", dt );
		}
		*/

		OPTICK_CATEGORY( OPTICK_FUNC, Optick::Category::Wait );
		RequestUpdateFlag.wait( false, std::memory_order::acquire );
		RequestUpdateFlag.clear( std::memory_order::release );
		//RequestUpdateFlag.acquire();
		return ShouldClose();
	}

	inline bool WaitUpdateReady()
	{
		/*
		if( !bUpdateReady )
		{
#if SPINLOCK
			while( !ShouldClose() && !bUpdateReady ) this_thread::yield();
#else
			unique_lock lock( mtUpdateReady );
			cvUpdateReady.wait( lock, [this]() { return ShouldClose() || bUpdateReady; } );
#endif

			float dt = duration<float>( high_resolution_clock::now() - tUpdateReady ).count() * 1000; // ms
			if( dt > 1 ) TRACE( DebugLevel::Debug, "WaitUpdateReady %fms\n", dt );
		}
		*/

		OPTICK_CATEGORY( OPTICK_FUNC, Optick::Category::Wait );
		UpdateReadyFlag.wait( false, std::memory_order::acquire ); // wait for change from false
		UpdateReadyFlag.clear( std::memory_order::release );
		//UpdateReadyFlag.acquire();
		return ShouldClose();
	}

	inline const void *GetCurrentFrameData() const { return FData[currentFrameData]; };

	//inline bool IsNextFrameReady() { return bUpdateReady.test(); }
	inline const void *SwapFrameData()
	{
		//assert( bUpdateReady.exchange( false ) );
		currentFrameData ^= 1;

		MessageRequestUpdate();

		return GetCurrentFrameData();
	}

	inline bool ShouldClose() { return bClose.test( std::memory_order::relaxed ); };
	inline void Close()
	{
		//if( ShouldClose() ) return;
		/*
#if !SPINLOCK
		mtUpdateReady.lock();
		mtRequestUpdate.lock();
#endif
		bClose = true;
#if !SPINLOCK
		mtUpdateReady.unlock();
		mtRequestUpdate.unlock();

		cvRequestUpdate.notify_all();
		cvUpdateReady.notify_all();
#endif
	*/
		bClose.test_and_set( std::memory_order::release );
		//RequestUpdateFlag.release();
		//UpdateReadyFlag.release();
		RequestUpdateFlag.test_and_set( std::memory_order::release );
		UpdateReadyFlag.test_and_set( std::memory_order::release );
		//
		UpdateReadyFlag.notify_all();
		RequestUpdateFlag.notify_all();
	};

	inline void MessageRequestUpdate()
	{
		/*
#if !SPINLOCK
		mtRequestUpdate.lock();
#endif
		bRequestUpdate = true;
		tRequestUpdate = high_resolution_clock::now();
#if !SPINLOCK
		mtRequestUpdate.unlock();
		cvRequestUpdate.notify_one();
#endif
		*/

		RequestUpdateFlag.test_and_set( std::memory_order::release );
		RequestUpdateFlag.notify_one();
		//RequestUpdateFlag.release();
	}

	inline void MessageUpdateReady()
	{
		/*
#if !SPINLOCK
		mtUpdateReady.lock();
#endif
		bUpdateReady = true;
		tUpdateReady = high_resolution_clock::now();
#if !SPINLOCK
		mtUpdateReady.unlock();
		cvUpdateReady.notify_one();
#endif
		*/
		UpdateReadyFlag.test_and_set( std::memory_order::release );
		UpdateReadyFlag.notify_one();
		//UpdateReadyFlag.release();
	}

	inline void SignalDestroyAndWaitAllDestroyed()
	{
		iDestroyed.fetch_add(1, std::memory_order::release);
		iDestroyed.notify_all();

		int old;

		while ((old = iDestroyed.load(std::memory_order::acquire)) != 3)
			iDestroyed.wait(old, std::memory_order::relaxed);
	}

#endif
};

bool GameEngine::Init( InputHandler *i, GameHandler *g, RenderHandler *r )
{
	delete m_pInputHandler;
	delete m_pGameHandler;
	delete m_pRenderHandler;

	m_pInputHandler = i;
	m_pGameHandler = g;
	m_pRenderHandler = r;

	if( !i || !g || !r ) { TRACE( DebugLevel::FatalError, "All handlers must be specified\n" ); return false; }

	glfwSetErrorCallback(ErrorCallback);
	if( !glfwInit( ) ) { TRACE( DebugLevel::FatalError, "Cannot init glfw\n" ); return false; }

	m_bInitialized = true;

	return true;
}

GameEngine::~GameEngine()
{
	if( m_pGameHandler   ) delete m_pGameHandler;
	if( m_pInputHandler  ) delete m_pInputHandler;
	if( m_pRenderHandler ) delete m_pRenderHandler;

	glfwSetErrorCallback(NULL);
	if( m_bInitialized ) { glfwTerminate(); }
}

bool GameEngine::Start()
{
	if( !m_bInitialized ) { TRACE( DebugLevel::FatalError, "Game engine must be initialized before starting\n" ); return false; }

	glfwWindowHint( GLFW_DOUBLEBUFFER, GLFW_TRUE );
	glfwWindowHint( GLFW_CLIENT_API, GLFW_OPENGL_API );
	glfwWindowHint( GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE );

	glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, 4 );
	glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 6 );
	glfwWindowHint( GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE );
	glfwWindowHint( GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE );

	const GLFWvidmode *mode = glfwGetVideoMode( glfwGetPrimaryMonitor() );

	glfwWindowHint( GLFW_REFRESH_RATE, GLFW_DONT_CARE );

	glfwWindowHint( GLFW_RED_BITS, GLFW_DONT_CARE );
	glfwWindowHint( GLFW_GREEN_BITS, GLFW_DONT_CARE );
	glfwWindowHint( GLFW_BLUE_BITS, GLFW_DONT_CARE );

	glfwWindowHint( GLFW_ALPHA_BITS, GLFW_DONT_CARE );

	glfwWindowHint( GLFW_DEPTH_BITS, GLFW_DONT_CARE );
	glfwWindowHint( GLFW_STENCIL_BITS, GLFW_DONT_CARE );

	glfwWindowHint( GLFW_SAMPLES, GLFW_DONT_CARE );
	glfwWindowHint( GLFW_SRGB_CAPABLE, GLFW_TRUE );

	GLFWwindow *window = glfwCreateWindow( mode->width, mode->height, "GameEngine", NULL, NULL );
	if( !window ) { TRACE( DebugLevel::FatalError, "Cannot create window\n" ); return false; }

	WindowDataContainer windowData;
	windowData.pGameEngine = this;
	windowData.window = window;

#ifdef MULTI_THREADED
	thread gameThread( [&]() { GameLoop( &windowData ); } );
	thread renderThread( [&]() { RenderLoop( &windowData ); } );

	InputLoop( &windowData );
	
	if( !windowData.ShouldClose() ) {
		TRACE( DebugLevel::Warning, "InputLoop exited when window should not close\n" ); windowData.Close();
	}

	gameThread.join();
	renderThread.join();
#else
	EngineLoop( &windowData );
#endif
	glfwDestroyWindow( window );
	return true;
}

#ifdef MULTI_THREADED
void GameEngine::GameLoop(void* wData)
{
	OPTICK_THREAD( "Game loop thread" );

	WindowDataContainer *windowData = (WindowDataContainer *)wData;
	GameEngine *gameEngine = windowData->pGameEngine;
	GLFWwindow *window = windowData->window;

	try
	{
		windowData->FData[0] = gameEngine->m_pGameHandler->NewFData();
		windowData->FData[1] = gameEngine->m_pGameHandler->NewFData();

		if( gameEngine->m_pGameHandler->OnCreate() )
		{
			//while( !windowData->ShouldClose() && !windowData->bInputInitialized )
			//	this_thread::yield();

			windowData->bInputInitialized.wait( false, std::memory_order::acquire ); // wait for change to true

			while(1)
			{
				OPTICK_CATEGORY( "wait for GPU rendered", Optick::Category::Wait );
				if( windowData->WaitRequestUpdate() ) break; // returns when indowData->ShouldClose()

				windowData->mtInputLock.lock();

				OPTICK_CATEGORY( "Process new Frame", Optick::Category::GameLogic );
				gameEngine->m_pInputHandler->OnPreTick();

				gameEngine->m_pGameHandler->OnTick( windowData->FData[!windowData->currentFrameData], gameEngine->m_pInputHandler );

				windowData->MessageUpdateReady();

				gameEngine->m_pInputHandler->OnPostTick();

				windowData->mtInputLock.unlock();
			}
		}
		else TRACE( DebugLevel::Log, "Game handler OnCreate returned false\n" );
	}
	catch( const std::exception &e ) { TRACE( DebugLevel::FatalError, "Exception caught in game loop: %s\n", e.what() ); }
	catch( ShutdownException ) {}
	catch( ... ) { TRACE( DebugLevel::FatalError, "Unknown Exception caught in game loop\n" ); }

	windowData->Close();

	try
	{
		gameEngine->m_pGameHandler->OnDestroy();
	}
	catch( const std::exception &e ) { TRACE( DebugLevel::FatalError, "Exception caught in game OnDestroy: %s\n", e.what() ); }
	catch( ... ) { TRACE( DebugLevel::FatalError, "Unknown Exception caught in game OnDestroy\n" ); }

	windowData->SignalDestroyAndWaitAllDestroyed();

	try
	{
		for (void*& fdata : windowData->FData)
		{
			gameEngine->m_pGameHandler->DeleteFData( fdata ); 
			fdata = NULL;
		}
	}
	catch( const std::exception &e ) { TRACE( DebugLevel::FatalError, "Exception caught in FData delete: %s\n", e.what() ); }
	catch( ... ) { TRACE( DebugLevel::FatalError, "Unknown Exception caught in FData delete\n" ); }

	try
	{
		delete gameEngine->m_pGameHandler;
	}
	catch( const std::exception &e ) { TRACE( DebugLevel::FatalError, "Exception caught in game destructor: %s\n", e.what() ); }
	catch( ... ) { TRACE( DebugLevel::FatalError, "Unknown Exception caught in game destructor\n" ); }

	gameEngine->m_pGameHandler = NULL;
}

void GameEngine::RenderLoop(void* wData)
{
	OPTICK_THREAD( "Render thread" );

	WindowDataContainer *windowData = (WindowDataContainer *)wData;
	GameEngine *gameEngine = windowData->pGameEngine;

	windowData->MessageRequestUpdate();
	glfwMakeContextCurrent( windowData->window );

	try
	{
		if( gameEngine->m_pRenderHandler->OnCreate() )
		{
			while( 1 )
			{
				OPTICK_CATEGORY( "wait for new tick", Optick::Category::Wait );
				if( windowData->WaitUpdateReady() ) break; // returns when windowData->ShouldClose()

				OPTICK_CATEGORY( "render frame", Optick::Category::Rendering );
				gameEngine->m_pRenderHandler->OnDraw( windowData->SwapFrameData() );

				OPTICK_CATEGORY( "Swap buffers", Optick::Category::Wait );
				glfwSwapBuffers( windowData->window );

			}
		}
		else TRACE( DebugLevel::Log, "Render handler OnCreate returned false\n" );	
	}
	catch( const std::exception &e ) { TRACE( DebugLevel::FatalError, "Exception caught in render loop: %s\n", e.what() ); }
	catch( ShutdownException ) {}
	catch( ... ) { TRACE( DebugLevel::FatalError, "Unknown Exception caught in render loop\n" ); }

	windowData->Close();

	try
	{
		gameEngine->m_pRenderHandler->OnDestroy();
	}
	catch( const std::exception &e ) { TRACE( DebugLevel::FatalError, "Exception caught in render OnDestroy: %s\n", e.what() ); }
	catch( ... ) { TRACE( DebugLevel::FatalError, "Unknown Exception caught in render OnDestroy\n" ); }

	windowData->SignalDestroyAndWaitAllDestroyed();

	try
	{
		delete gameEngine->m_pRenderHandler;
	}
	catch( const std::exception &e ) { TRACE( DebugLevel::FatalError, "Exception caught in render destructor: %s\n", e.what() ); }
	catch( ... ) { TRACE( DebugLevel::FatalError, "Unknown Exception caught in render destructor\n" ); }

	//glbinding::aux::stop();
	gameEngine->m_pRenderHandler = NULL;

	glfwMakeContextCurrent( NULL );
}

void GameEngine::InputLoop( void *wData )
{
	OPTICK_THREAD( "I/O thread" );

	WindowDataContainer *windowData = (WindowDataContainer *)wData;
	GameEngine *gameEngine = windowData->pGameEngine;
	GLFWwindow *window = windowData->window;

	try
	{
		bool ok = gameEngine->m_pInputHandler->OnCreate(window);

		windowData->bInputInitialized.store(true, std::memory_order::release); // release tick thread from wait
		windowData->bInputInitialized.notify_all();

		if( ok )
		{
			while( !windowData->ShouldClose() )
			{
				OPTICK_CATEGORY( "wait for window messages", Optick::Category::Wait );
				glfwWaitEventsTimeout( 0.5 );

				//auto t1 = high_resolution_clock::now();

				OPTICK_CATEGORY( "process messages", Optick::Category::IO );
				windowData->mtInputLock.lock();
				glfwPollEvents();
				windowData->mtInputLock.unlock();

				//float dt = duration<float>( high_resolution_clock::now() - t1 ).count() * 1000; // ms
				//if( dt > 1 ) TRACE( DebugLevel::Debug, "InputLoop was in glfwPollEvents for %fms\n", dt );

				if( glfwWindowShouldClose( window ) ) break;
			}
		}
		else TRACE( DebugLevel::Log, "Input handler OnCreate returned false\n" );
	}
	catch( const std::exception &e ) { TRACE( DebugLevel::FatalError, "Exception caught in input loop: %s\n", e.what() ); }
	catch( ShutdownException ) {}
	catch( ... ) { TRACE( DebugLevel::FatalError, "Unknown Exception caught in input loop\n" ); }

	windowData->Close();

	try
	{
		gameEngine->m_pInputHandler->OnDestroy( window );
	}
	catch( const std::exception &e ) { TRACE( DebugLevel::FatalError, "Exception caught in input OnDestroy: %s\n", e.what() ); }
	catch( ... ) { TRACE( DebugLevel::FatalError, "Unknown Exception caught in input OnDestroy\n" ); }

	windowData->SignalDestroyAndWaitAllDestroyed();

	try
	{
		delete gameEngine->m_pInputHandler;
	}
	catch( const std::exception &e ) { TRACE( DebugLevel::FatalError, "Exception caught in input destructor: %s\n", e.what() ); }
	catch( ... ) { TRACE( DebugLevel::FatalError, "Unknown Exception caught in input destructor\n" ); }

	gameEngine->m_pInputHandler = NULL;
}
#else // def MULTI_THREADED
void GameEngine::EngineLoop( void *wData )
{
	WindowDataContainer *windowData = (WindowDataContainer *)wData;
	GameEngine *gameEngine = windowData->pGameEngine;
	GLFWwindow *window = windowData->window;

	glfwMakeContextCurrent( window );

	void* FData = NULL;
	try
	{
		if(
			gameEngine->m_pInputHandler->OnCreate( window ) &&
			gameEngine->m_pGameHandler->OnCreate() &&
			gameEngine->m_pRenderHandler->OnCreate()
			)
		{
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

					{
						OPTICK_CATEGORY( "Swap buffers", Optick::Category::Wait );
						glfwSwapBuffers( window );
					}
				}
			}
		}
		else TRACE( DebugLevel::FatalError, "OnCreate returned false\n" );
	}
	catch( const std::exception &e ) { TRACE( DebugLevel::FatalError, "Exception caught: %s\n", e.what() ); }
	catch( ShutdownException ) {}
	catch( ... ) { TRACE( DebugLevel::FatalError, "Unknown Exception caught\n" ); }

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
#endif
