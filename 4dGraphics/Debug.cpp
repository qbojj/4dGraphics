#include "Debug.h"

#include <fstream>
#include <iostream>

#define STB_SPRINTF_STATIC
#define STB_SPRINTF_IMPLEMENTATION
#include <stb_sprintf.h>
#include <mutex>

#undef VK_ENABLE_BETA_EXTENSIONS
#include <vulkan/vk_enum_string_helper.h>

using namespace std;

DebugLevel LogLevel = IS_DEBUG ? DebugLevel::Debug : DebugLevel::Log;

static ofstream DebugFile( "log.txt", ios_base::out | ios_base::trunc | ios_base::app );

static void printToFile( const char *s )
{
	if( DebugFile ) 
	{
		DebugFile << s; 
		DebugFile.flush();
	}
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

const char *string_from_VkResult(VkResult r)
{
    return string_VkResult(r);
}

const char *string_from_VkStructureType(VkStructureType t)
{
    return string_VkStructureType(t);
}

namespace vk_err
{
	class ErrorCategoryImpl : public std::error_category
	{
	public:
		virtual const char * name() const noexcept override
		{
			return "VkResult";
		}

		virtual std::string message( int ev ) const override
		{
			return std::to_string( ev );
		}
	};

	const std::error_category &error_cat() noexcept
	{
		static ErrorCategoryImpl category;
		return category;
	}

	[[noreturn]] void throwResultException( VkResult result, const std::string &message )
	{
		switch ( result )
		{
		case VK_ERROR_OUT_OF_HOST_MEMORY: throw OutOfHostMemoryError( message );
		case VK_ERROR_OUT_OF_DEVICE_MEMORY: throw OutOfDeviceMemoryError( message );
		case VK_ERROR_INITIALIZATION_FAILED: throw InitializationFailedError( message );
		case VK_ERROR_DEVICE_LOST: throw DeviceLostError( message );
		case VK_ERROR_MEMORY_MAP_FAILED: throw MemoryMapFailedError( message );
		case VK_ERROR_LAYER_NOT_PRESENT: throw LayerNotPresentError( message );
		case VK_ERROR_EXTENSION_NOT_PRESENT: throw ExtensionNotPresentError( message );
		case VK_ERROR_FEATURE_NOT_PRESENT: throw FeatureNotPresentError( message );
		case VK_ERROR_INCOMPATIBLE_DRIVER: throw IncompatibleDriverError( message );
		case VK_ERROR_TOO_MANY_OBJECTS: throw TooManyObjectsError( message );
		case VK_ERROR_FORMAT_NOT_SUPPORTED: throw FormatNotSupportedError( message );
		case VK_ERROR_FRAGMENTED_POOL: throw FragmentedPoolError( message );
		case VK_ERROR_UNKNOWN: throw UnknownError( message );
		case VK_ERROR_OUT_OF_POOL_MEMORY: throw OutOfPoolMemoryError( message );
		case VK_ERROR_INVALID_EXTERNAL_HANDLE: throw InvalidExternalHandleError( message );
		case VK_ERROR_FRAGMENTATION: throw FragmentationError( message );
		case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS: throw InvalidOpaqueCaptureAddressError( message );
		case VK_ERROR_SURFACE_LOST_KHR: throw SurfaceLostKHRError( message );
		case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: throw NativeWindowInUseKHRError( message );
		case VK_ERROR_OUT_OF_DATE_KHR: throw OutOfDateKHRError( message );
		case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: throw IncompatibleDisplayKHRError( message );
		case VK_ERROR_VALIDATION_FAILED_EXT: throw ValidationFailedEXTError( message );
		case VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR: throw ImageUsageNotSupportedKHRError( message );
		case VK_ERROR_NOT_PERMITTED_KHR: throw NotPermittedKHRError( message );
		default: throw Error( make_error_code( result ), message );
		}
	}
}