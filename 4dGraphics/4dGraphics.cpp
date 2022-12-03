#include "GameCore.h"
#include "GameInputHandler.h"
#include "GameRenderHandler.h"
#include "GameTickHandler.h"

#include "Debug.h"
#include "optick.h"
#include <stdlib.h>
#include <filesystem>
#include <chrono>
#include <thread>

#include "Shader.h"

void* ImGuiAlloc(size_t size, void* dat)
{
	if( dat ) (*(int*)dat)++;
	return malloc(size);
}

void ImGuiFree(void* ptr, void* dat)
{
	if (ptr)
	{
		if(dat) (*(int*)dat)--;
		free(ptr);
	}
}

int Entry( int argc, const char *argv[] )
{
	OPTICK_APP( "4dGraphics" );

	for( int i = 1; i < argc; i++ )
	{
		std::string_view s{ argv[i] };

		if( s == "-d" || s == "--debug-level" )
		{
			DebugLevel lev = DebugLevel::Log;
			if( i+1 < argc )
			{
				i++;
				
				switch( argv[i][0] )
				{
				case 'd': lev = DebugLevel::Debug; break;
				case 'l': lev = DebugLevel::Log; break;
				case 'w': lev = DebugLevel::Warning; break;
				case 'e': lev = DebugLevel::Error; break;
				case 'f': lev = DebugLevel::FatalError; break;
				case 'q': lev = DebugLevel::PrintAlways; break;
				default: break;
				}
			}
			
			LogLevel = lev;
		}
		else 
		{
			OutputDebug(DebugLevel::PrintAlways, "Unknown option %s\n", argv[i]);
			return -1;
		}
	}

	srand( (unsigned int)time( NULL ) );

	{
		time_t rawtime;
		struct tm *timeinfo;
		char buffer[80];

		time( &rawtime );
		timeinfo = localtime( &rawtime );

		strftime( buffer, sizeof( buffer ), "%d-%m-%Y %H:%M:%S", timeinfo );

		TRACE( DebugLevel::Log, "Program started (%s)\n", buffer );
		TRACE( DebugLevel::Log, "Compiled with: %u\n", __cplusplus );
		TRACE( DebugLevel::Log, "Debug: %s\n", IS_DEBUG ? "true" : "false");
	}

	TRACE( DebugLevel::Log, "working dir: %s\n", std::filesystem::current_path().string().c_str() );
	GameEngine engine;
	
	bool ok = false;
	
	if( engine.Init( 
		new GameInputHandler(),
		new GameTickHandler(),
		new GameRenderHandler() )
		)
	{
		int allocCnt = 0;
		ImGui::SetAllocatorFunctions(ImGuiAlloc, ImGuiFree, &allocCnt);

		if (ImGui::CreateContext() == NULL)
		{
			TRACE(DebugLevel::FatalError, "Cannot initialize imgui context");
			return 0;
		}

		if (!glslang::InitializeProcess())
		{
			ImGui::DestroyContext();
			TRACE(DebugLevel::FatalError, "Cannot initialize glslang context");
			return 0;
		}

		ok = engine.Start();
		glslang::FinalizeProcess();
		ImGui::DestroyContext();

		if( allocCnt != 0 )
			TRACE( DebugLevel::Error, "ImGui made %d %s %s than frees\n", 
				abs( allocCnt ), 
				abs( allocCnt ) > 1 ? "allocations" : "allocation", 
				allocCnt > 0 ? "more" : "less" );
	}

	return ok ? 0 : 1;
}

#ifdef _WIN32
typedef struct HINSTANCE__ *HINSTANCE;
typedef char CHAR;
typedef CHAR *LPSTR;

int __stdcall WinMain(
	HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR lpCmdLine,
	int nShowCmd
)
{
	(void)hInstance, hPrevInstance, lpCmdLine, nShowCmd;
	
	std::vector<std::string> argvMem;
	char sep = '\0';
	char *start = nullptr;

	char *s = lpCmdLine;

	const char *separators = "\'\""

	while( *s )
	{
		bool endStr = false;
		if( *s == sep ) endStr = true;
		else if( isblank(*s) )
		{
			if( !sep && start ) endStr = true;
		}
		else if( !start ) // not blank and not separator
		{
			if( strchr( separators, *s ) )
			{
				start = s+1;
				sep = *s
			}
			else start = s;
		}

		if( endStr )
		{
			if( start != s ) argvMem.push_back(std::string(start,s));
			start = nullptr;
			sep = '\0';
		}

		s++;
	}

	std::vector<const char *> argv{"{binpath}"};	
	for( auto &s : argvMem ) { argv.push_back(s.data()); };
	
	return Entry( (int)argv.size(), argv.data() );
}
#endif

int main(int argc, const char *argv[])
{
	return Entry( argc, argv );
}