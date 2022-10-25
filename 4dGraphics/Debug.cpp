#include "Debug.h"

#include <fstream>
#include <iostream>

#define STB_SPRINTF_STATIC
#define STB_SPRINTF_IMPLEMENTATION
#include <stb_sprintf.h>
#include <mutex>
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

static std::mutex DebugMutex;

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

    {
        std::lock_guard lock( DebugMutex );
	    PRINT_DEBUG( dat );
    }
#ifdef USE_WIN_DEBUG
	if( l == DebugLevel::FatalError ) MessageBoxA( NULL, dat, "Fatal Error!", MB_OK );
#endif

	if( si >= 1024 ) free( dat );
}

const char* VulkanResultErrorCause(VkResult res)
{
	switch (res)
	{
    case VK_SUCCESS: return "Success";
    case VK_NOT_READY: return "NotReady";
    case VK_TIMEOUT: return "Timeout";
    case VK_EVENT_SET: return "EventSet";
    case VK_EVENT_RESET: return "EventReset";
    case VK_INCOMPLETE: return "Incomplete";
    case VK_ERROR_OUT_OF_HOST_MEMORY: return "ErrorOutOfHostMemory";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "ErrorOutOfDeviceMemory";
    case VK_ERROR_INITIALIZATION_FAILED: return "ErrorInitializationFailed";
    case VK_ERROR_DEVICE_LOST: return "ErrorDeviceLost";
    case VK_ERROR_MEMORY_MAP_FAILED: return "ErrorMemoryMapFailed";
    case VK_ERROR_LAYER_NOT_PRESENT: return "ErrorLayerNotPresent";
    case VK_ERROR_EXTENSION_NOT_PRESENT: return "ErrorExtensionNotPresent";
    case VK_ERROR_FEATURE_NOT_PRESENT: return "ErrorFeatureNotPresent";
    case VK_ERROR_INCOMPATIBLE_DRIVER: return "ErrorIncompatibleDriver";
    case VK_ERROR_TOO_MANY_OBJECTS: return "ErrorTooManyObjects";
    case VK_ERROR_FORMAT_NOT_SUPPORTED: return "ErrorFormatNotSupported";
    case VK_ERROR_FRAGMENTED_POOL: return "ErrorFragmentedPool";
    case VK_ERROR_UNKNOWN: return "ErrorUnknown";
    case VK_ERROR_OUT_OF_POOL_MEMORY: return "ErrorOutOfPoolMemory";
    case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "ErrorInvalidExternalHandle";
    case VK_ERROR_FRAGMENTATION: return "ErrorFragmentation";
    case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS: return "ErrorInvalidOpaqueCaptureAddress";
    case VK_ERROR_PIPELINE_COMPILE_REQUIRED_EXT: return "PipelineCompileRequired";
    case VK_ERROR_SURFACE_LOST_KHR: return "ErrorSurfaceLostKHR";
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "ErrorNativeWindowInUseKHR";
    case VK_SUBOPTIMAL_KHR: return "SuboptimalKHR";
    case VK_ERROR_OUT_OF_DATE_KHR: return "ErrorOutOfDateKHR";
    case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "ErrorIncompatibleDisplayKHR";
    case VK_ERROR_VALIDATION_FAILED_EXT: return "ErrorValidationFailedEXT";
    case VK_ERROR_INVALID_SHADER_NV: return "ErrorInvalidShaderNV";
#if defined( VK_ENABLE_BETA_EXTENSIONS )
    case VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR: return "ErrorImageUsageNotSupportedKHR";
    case VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR: return "ErrorVideoPictureLayoutNotSupportedKHR";
    case VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR: return "ErrorVideoProfileOperationNotSupportedKHR";
    case VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR: return "ErrorVideoProfileFormatNotSupportedKHR";
    case VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR: return "ErrorVideoProfileCodecNotSupportedKHR";
    case VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR: return "ErrorVideoStdVersionNotSupportedKHR";
#endif /*VK_ENABLE_BETA_EXTENSIONS*/
    case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT: return "ErrorInvalidDrmFormatModifierPlaneLayoutEXT";
    case VK_ERROR_NOT_PERMITTED_KHR: return "ErrorNotPermittedKHR";
#if defined( VK_USE_PLATFORM_WIN32_KHR )
    case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT: return "ErrorFullScreenExclusiveModeLostEXT";
#endif /*VK_USE_PLATFORM_WIN32_KHR*/
    case VK_THREAD_IDLE_KHR: return "ThreadIdleKHR";
    case VK_THREAD_DONE_KHR: return "ThreadDoneKHR";
    case VK_OPERATION_DEFERRED_KHR: return "OperationDeferredKHR";
    case VK_OPERATION_NOT_DEFERRED_KHR: return "OperationNotDeferredKHR";
    case VK_ERROR_COMPRESSION_EXHAUSTED_EXT: return "ErrorCompressionExhaustedEXT";
    default: return "Unknown/Invalid";
	}
}