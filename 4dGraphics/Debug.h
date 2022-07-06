#pragma once

#include <cstdarg>

#if defined(_WIN32) && 1
#define USE_WIN_DEBUG 
#endif

#if defined(_DEBUG)
#define IS_DEBUG 1
#define IS_NOT_DEBUG 0
#define DEBUG_ONLY(a) a
#else
#define IS_DEBUG 0
#define IS_NOT_DEBUG 1
#define DEBUG_ONLY(a)
#endif

enum class DebugLevel { Debug, Log, Warning, Error, FatalError };
extern DebugLevel LogLevel;
    
void OutputDebugV( DebugLevel, const char*, va_list );
void OutputDebug( DebugLevel, const char *, ... );

#define TRACE( level, format, ... ) OutputDebug( level, "%s(%u): " format, __FILE__, __LINE__ __VA_OPT__(,) __VA_ARGS__ )
