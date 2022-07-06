#include "stdafx.h"
#include "Debug.h"

#include <fstream>
#include <iostream>
using namespace std;

DebugLevel LogLevel = IS_DEBUG ? DebugLevel::Debug : DebugLevel::Log;

static ofstream DebugFile( "log.txt", ios_base::out | ios_base::trunc );

static void printToFile( const char *s )
{
	if( DebugFile ) 
	{
		DebugFile << s; 
		//DebugFile.flush();
	}
	//FILE *f = fopen( "log.txt", "a" );
	//if( f )
	//{
	//	fputs( s, f );
	//	fclose( f );
	//}
}

#ifdef USE_WIN_DEBUG
#include "Windows.h"
static void PRINT_DEBUG( const char *s ) { printToFile( s ); OutputDebugStringA( s ); cerr << s; }
#else
static void PRINT_DEBUG( const char *s ) { printToFile( s ); cerr << s; }
#endif

void OutputDebug(DebugLevel l, const char* fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	OutputDebugV(l, fmt, va);
	va_end(va);
}

void OutputDebugV( DebugLevel l, const char *fmt, va_list va )
{
	if( l < LogLevel ) return;

	char buf[1024];
	char *dat = buf;

	va_list va2;
	va_copy(va2, va);
	int si = stbsp_vsnprintf( dat, 1024, fmt, va2 );
	va_end(va2);

	if( si >= 1024 )
	{
		dat = (char *)malloc( si + 1 );

		stbsp_vsprintf( dat, fmt, va );
	}

	PRINT_DEBUG( dat );
	
#ifdef USE_WIN_DEBUG
	if( l == DebugLevel::FatalError ) MessageBoxA( NULL, dat, "Fatal Error!", MB_OK );
#endif

	if( si >= 1024 ) free( dat );
}
