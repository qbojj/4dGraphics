#include "stdafx.h"
#include "GameCore.h"
#include "GameInputHandler.h"
#include "GameRenderHandler.h"
#include "GameTickHandler.h"

#include "Debug.h"
#include "optick.h"
#include <stdlib.h>

//#include <boost/stacktrace.hpp>

#ifdef _WIN32
#include "Windows.h"
#endif

#if defined(USE_WIN_DEBUG) && 0
#define USE_EXCEPTION_HANDLER
#endif

#ifdef USE_EXCEPTION_HANDLER
#include "StackWalker.h"

class MyWalker : public StackWalker
{
public:
	//std::string res;
protected:
	virtual void OnOutput( const char *str )
	{
		//res += str;
		TRACE( DebugLevel::Error, "%s", str );
	}
} walker;

LONG WINAPI UnhandledFilter( _In_ struct _EXCEPTION_POINTERS *ExceptionInfo )
{
	//walker.res.clear();
	walker.ShowCallstack( GetCurrentThread(), ExceptionInfo->ContextRecord );
	TRACE( DebugLevel::Error, "\n\n" );
	//TRACE( DebugLevel::FatalError, "Fatal Error: unhandled exception! Callstack:\n%s\n", walker.res.c_str() );
	//walker.res.clear();
	//boost::stacktrace::basic_stacktrace stack;
	//std::string str = boost::stacktrace::to_string( stack );
	//TRACE( DebugLevel::Error, "Error: unhandled exception!\n" );// %s", str.data() );

	return EXCEPTION_CONTINUE_SEARCH;
}
#endif

int Entry()
{
	OPTICK_APP( "4dGraphics" );

	srand( (unsigned int)time( NULL ) );
	//SetUnhandledExceptionFilter( UnhandledFilter );

#ifdef USE_EXCEPTION_HANDLER
	if( !IsDebuggerPresent() ) AddVectoredExceptionHandler( 0, UnhandledFilter );
	else TRACE( DebugLevel::Log, "Debugger present => did not install exception handler\n" );
#endif

	{
		time_t rawtime;
		struct tm *timeinfo;
		char buffer[80];

		time( &rawtime );
		timeinfo = localtime( &rawtime );

		strftime( buffer, sizeof( buffer ), "%d-%m-%Y %H:%M:%S", timeinfo );

		TRACE( DebugLevel::Log, "Program started (%s)\n", buffer );
		TRACE( DebugLevel::Log, "Compiled with: %u\n", __cplusplus );
	}

	TRACE( DebugLevel::Log, "working dir: %s\n", std::filesystem::current_path().string().c_str() );
	GameEngine engine;
	
	bool ok = false;


	if( engine.Init( 
		new GameInputHandler,
		new GameTickHandler,
		new GameRenderHandler )
		)
	{
		ImGui::CreateContext();
		ok = engine.Start();
		ImGui::DestroyContext();
	}

	return ok ? 0 : 1;
}

#if defined(REQUIRE_WINMAIN)
int WINAPI WinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPSTR lpCmdLine,
	_In_ int nShowCmd
)
{
	return Entry();
}

#else
int main()
{
	return Entry();
}
#endif
