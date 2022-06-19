#pragma once

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
    
void OutputDebug( DebugLevel, const char *, ... );
void ClearLastInvocationsLog();

#define TRACE( level, format, ... ) OutputDebug( level, "'%s' (%d): " format, __FILE__, __LINE__, ## __VA_ARGS__ )
