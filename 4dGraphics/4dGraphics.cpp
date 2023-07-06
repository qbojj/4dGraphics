#include "GameHandler.h"

#include "Debug.h"
#include "optick.h"
#include <stdlib.h>
#include <filesystem>
#include <chrono>
#include <thread>
#include <sstream>

#include <SDL2/SDL_main.h>
#include "Shader.h"


#include <argparse/argparse.hpp>
#include "cppHelpers.hpp"

#ifdef __cplusplus
extern "C"
#endif
int main( int argc, char *argv[] )
{
	argparse::ArgumentParser parser;
	
	parser.add_argument("-d","--debug-level")
		.help("set debug level. possible values (d, l, w, e, f, q)")
		.default_value("l");

	parser.add_argument("-V")
		.help("enable vulkan validation layers")
		.default_value(false);
	
	try	{
		parser.parse_args(argc, argv);
	}
	catch( const std::runtime_error &e ) {
		std::stringstream ss;
		ss << e.what() << std::endl;
		ss << parser;
		TRACE( DebugLevel::PrintAlways, "%s", ss.str().c_str() );
		return 1;
	}
	
	switch( parser.get<char>("-d") )
	{
	case 'd': LogLevel = DebugLevel::Debug; break;
	case 'l': LogLevel = DebugLevel::Log; break;
	case 'w': LogLevel = DebugLevel::Warning; break;
	case 'e': LogLevel = DebugLevel::Error; break;
	case 'f': LogLevel = DebugLevel::FatalError; break;
	case 'q': LogLevel = DebugLevel::PrintAlways; break;
	default: TRACE(DebugLevel::PrintAlways, "unsupported debug level %c\n", parser.get<char>("-d") );
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