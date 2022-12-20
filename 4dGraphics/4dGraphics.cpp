#include "GameHandler.h"

#include "Debug.h"
#include "optick.h"
#include <stdlib.h>
#include <filesystem>
#include <chrono>
#include <thread>

#include <SDL2/SDL_main.h>
#include "Shader.h"

#include "cppHelpers.hpp"

#ifdef __cplusplus
extern "C"
#endif
int main( int argc, char *argv[] )
{
	for( int i = 1; i < argc; i++ )
	{
		std::string_view s{ argv[i] };

		if( s == "-d" || s == "--debug-level" )
		{
			DebugLevel lev = DebugLevel::Log;
			if( i+1 < argc && argv[i+1][0] != '-' )
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
		TRACE( DebugLevel::Log, "working dir: %s\n", std::filesystem::current_path().string().c_str() );
	}

	return MyGameHandler{}.Run();
}