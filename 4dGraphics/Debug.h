#pragma once

#include <stdarg.h>
#include <debug-trap/debug-trap.h>
#include <volk.h>

#if defined(_WIN32) && 1
#define USE_WIN_DEBUG 
#endif

#if !defined(PSNIP_NDEBUG) || (PSNIP_NDEBUG == 0)
#define IS_DEBUG 1
#define IS_NOT_DEBUG 0
#define DEBUG_ONLY(a) a
#else
#define IS_DEBUG 0
#define IS_NOT_DEBUG 1
#define DEBUG_ONLY(a)
#endif

enum class DebugLevel : unsigned { Debug, Log, Warning, Error, FatalError, PrintAlways = (unsigned)-1 };
extern DebugLevel LogLevel;
    
void OutputDebugV( DebugLevel, const char*, va_list );
void OutputDebug( DebugLevel, const char *, ... );

const char* VulkanResultErrorCause(VkResult);

#define TRACE( level, format, ... ) OutputDebug( level, "%s(%u): " format, __FILE__, __LINE__ __VA_OPT__(,) __VA_ARGS__ )

#define VK_ASSERT(value) psnip_dbg_assert(value);
#define VK_CHECK(expr) do{                      \
    VkResult result_check_ = (expr);            \
    VK_ASSERT( result_check_ >= 0 );            \
    if( result_check_ < 0 ) { return false; };  \
}while(0)

#define VK_CHECK_RET(expr) do{                          \
    VkResult result_check_ = (expr);                    \
    VK_ASSERT( result_check_ >= 0 );                    \
    if( result_check_ < 0 ) { return result_check_; };  \
}while(0)
