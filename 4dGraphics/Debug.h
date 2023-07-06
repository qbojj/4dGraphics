#pragma once

#include <stdarg.h>
#include <debug-trap/debug-trap.h>
#include <volk.h>
#include <string>
#include <stdexcept>

#if defined(_WIN32) && 1
#define USE_WIN_DEBUG 
#endif

#if 1 || !defined(PSNIP_NDEBUG) || (PSNIP_NDEBUG == 0)
#define IS_DEBUG 1
#define IS_NOT_DEBUG 0
#define DEBUG_ONLY(a) a
#else
#define IS_DEBUG 0
#define IS_NOT_DEBUG 1
#define DEBUG_ONLY(a) 
#endif

enum class DebugLevel : unsigned 
{
    Debug, 
    Log, 
    Warning, 
    Error, 
    FatalError, 
    PrintAlways = (unsigned)-1 
};
extern DebugLevel LogLevel;
    
void OutputDebugV( DebugLevel, const char*, va_list );
void OutputDebug( DebugLevel, const char *, ... );

const char *string_from_VkResult(VkResult);
const char *string_from_VkStructureType(VkStructureType);

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

#define VK_CHECK_GOTO_INIT VkResult _result = VK_SUCCESS
#define VK_CHECK_GOTO( c ) do{ _result = (c); if( _result < 0 ) goto _error; }while(0)
#define VK_CHECK_GOTO_HANDLE( val ) _error: VkResult val = _result

namespace vk_err
{
    const std::error_category &error_cat() noexcept;
    std::error_code make_error_code( VkResult e ) noexcept
    {
        return std::error_code( static_cast<int>(e), error_cat() );
    }

    class Error : public std::system_error {
    public:
        Error( std::error_code ec, const std::string &msg )
            : system_error( ec, msg ) {};
    };

    #define def_err_class( name, val )  \
        struct name final : public Error {    \
            name(const std::string &msg) : Error( make_error_code(val), msg ) {} \
        }
    
    def_err_class( OutOfHostMemoryError, VK_ERROR_OUT_OF_HOST_MEMORY );
    def_err_class( OutOfDeviceMemoryError, VK_ERROR_OUT_OF_DEVICE_MEMORY );
    def_err_class( InitializationFailedError, VK_ERROR_INITIALIZATION_FAILED );
    def_err_class( DeviceLostError, VK_ERROR_DEVICE_LOST );
    def_err_class( MemoryMapFailedError, VK_ERROR_MEMORY_MAP_FAILED );
    def_err_class( LayerNotPresentError, VK_ERROR_LAYER_NOT_PRESENT );
    def_err_class( ExtensionNotPresentError, VK_ERROR_EXTENSION_NOT_PRESENT );
    def_err_class( FeatureNotPresentError, VK_ERROR_FEATURE_NOT_PRESENT );
    def_err_class( IncompatibleDriverError, VK_ERROR_INCOMPATIBLE_DRIVER );
    def_err_class( TooManyObjectsError, VK_ERROR_TOO_MANY_OBJECTS );
    def_err_class( FormatNotSupportedError, VK_ERROR_FORMAT_NOT_SUPPORTED );
    def_err_class( FragmentedPoolError, VK_ERROR_FRAGMENTED_POOL );
    def_err_class( UnknownError, VK_ERROR_UNKNOWN );
    def_err_class( OutOfPoolMemoryError, VK_ERROR_OUT_OF_POOL_MEMORY );
    def_err_class( InvalidExternalHandleError, VK_ERROR_INVALID_EXTERNAL_HANDLE );
    def_err_class( FragmentationError, VK_ERROR_FRAGMENTATION );
    def_err_class( InvalidOpaqueCaptureAddressError, VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS );
    def_err_class( SurfaceLostKHRError, VK_ERROR_SURFACE_LOST_KHR );
    def_err_class( NativeWindowInUseKHRError, VK_ERROR_NATIVE_WINDOW_IN_USE_KHR );
    def_err_class( OutOfDateKHRError, VK_ERROR_OUT_OF_DATE_KHR );
    def_err_class( IncompatibleDisplayKHRError, VK_ERROR_INCOMPATIBLE_DISPLAY_KHR );
    def_err_class( ValidationFailedEXTError, VK_ERROR_VALIDATION_FAILED_EXT );
    def_err_class( ImageUsageNotSupportedKHRError, VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR );
    def_err_class( NotPermittedKHRError, VK_ERROR_NOT_PERMITTED_KHR );

    [[noreturn]] void throwResultException( VkResult result, const std::string &message );
}

#define CHECK_THROW(a, msg)                \
    do                                     \
    {                                      \
        if (auto res_ = (a); res_ < 0)     \
            vk_err::throwResultException(res_, msg); \
    } while (0)

// defines in scope tmp_
#define CREATE_THROW(a, dst, msg)            \
    do                                       \
    {                                        \
        std::remove_cvref_t<decltype(dst)> tmp_ = VK_NULL_HANDLE; \
        CHECK_THROW(a, msg);                 \
        dst = tmp_;                          \
    } while (0)

#define ASSERT_THROW(a, msg)               \
    do                                     \
    {                                      \
        if (!(a))                          \
            throw std::runtime_error(msg); \
    } while (0)