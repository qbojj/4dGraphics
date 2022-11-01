#include "Shader.h"

#include "Debug.h"
#include "CommonUtility.h"
#include <filesystem>
#include <string>
#include <vector>

// for endianness conversions
#include <builtin/builtin.h>

// for export to SPIRV
#include <glslang/SPIRV/GlslangToSpv.h>
#include <spirv-tools/optimizer.hpp>

namespace glslang
{
    // from glslang::glslang-default-resource-limits
    extern const TBuiltInResource DefaultTBuiltInResource;
}

using namespace std;

struct MyIncluder : glslang::TShader::Includer
{
    uint32_t maxInclusionDepth = 64;

    struct MyIncludeResult : public IncludeResult
    {
        static const char *CopyString( const std::string &str )
        {
            size_t len = str.size() + 1;
            char *s = new char[ len ];
            memcpy( s, str.c_str(), len );
            return s;
        }

        MyIncludeResult(
            const std::string &headerName,
            const std::string &data
        ) : IncludeResult(
            headerName,
            CopyString( data ),
            data.size(),
            nullptr )
        {
        }

        MyIncludeResult(
            const std::string &ErrorStr
        ) : IncludeResult(
            "",
            CopyString( ErrorStr ),
            ErrorStr.size(),
            nullptr )
        {
        }

        ~MyIncludeResult()
        {
            if( headerData ) delete[] headerData;
        }
    };

    MyIncludeResult *includeFromBase(
        const char *inclusionPath,
        const char *headerName
    )
    {
        string path = std::filesystem::absolute(
            std::filesystem::path( inclusionPath ) / headerName
        ).string();

        string str = GetFileString( path.c_str(), false );
        if( str == "" ) return nullptr;

        return new MyIncludeResult(
            path,
            str
        );
    }

    MyIncludeResult *includeLocal( const char *headerName,
        const char *includerName,
        size_t inclusionDepth ) override
    {
        if( inclusionDepth > maxInclusionDepth )
            return new MyIncludeResult(
                "recursion limit reached (" + to_string( maxInclusionDepth ) + ")"
            );

        std::string parentDir = std::filesystem::path( includerName ).parent_path().string();
        return includeFromBase( parentDir.c_str(), headerName );
    }

    void releaseInclude( IncludeResult *res ) override
    {
        delete (MyIncludeResult *)res;
    };
};

std::vector<uint32_t> compileShaderToSPIRV( EShLanguage stage, const char *shaderSource, const char *fileName )
{
    if( stage < 0 || stage >= EShLangCount ) return std::vector<uint32_t>();

    glslang::TShader shader( stage );

    std::string path = std::filesystem::absolute( fileName ).string();
    const char *absolutePath = path.c_str();
    int sourceLen = (int)strlen( shaderSource );

    shader.setStringsWithLengthsAndNames(
        &shaderSource, &sourceLen, &absolutePath, 1
    );

    shader.setEnvInput( glslang::EShSourceGlsl, stage, glslang::EShClientVulkan, 100 );
    shader.setEnvClient( glslang::EShClientVulkan, glslang::EShTargetVulkan_1_2 );
    shader.setEnvTarget( glslang::EShTargetSpv, glslang::EShTargetSpv_1_5 );

    MyIncluder includer;

    if( !shader.parse( &glslang::DefaultTBuiltInResource, 100, false, EShMsgDefault, includer ) )
    {
        OutputDebug( DebugLevel::Error,
            "GLSL parsing failed (%s): %s\n%s", absolutePath,
            shader.getInfoLog(),
            shader.getInfoDebugLog()
        );

        return std::vector<uint32_t>();
    }

    glslang::TProgram program;
    program.addShader( &shader );
    if( !program.link( (EShMessages)( (uint32_t)EShMsgSpvRules | EShMsgVulkanRules ) ) )
    {
        OutputDebug( DebugLevel::Error,
            "GLSL linking failed (%s): %s\n%s", absolutePath,
            program.getInfoLog(),
            program.getInfoDebugLog()
        );

        return std::vector<uint32_t>();
    }

    spv::SpvBuildLogger logger{};

    std::vector<uint32_t> spv;
    glslang::GlslangToSpv( *program.getIntermediate(stage), spv, &logger );

    std::string log = logger.getAllMessages();
    if( log != "" ) 
        OutputDebug( spv.size() == 0 ? DebugLevel::Error : DebugLevel::Warning,
            "spirv compilation log: %s\n", log.c_str());

    if( spv.size() == 0 )
    {
        OutputDebug( DebugLevel::Error,
            "Compilation to SPIR-V failed (%s): %s\n%s", absolutePath
        );

        return std::vector<uint32_t>();
    }

    spvtools::Optimizer opt( SPV_ENV_VULKAN_1_2 );
    bool ok = opt.RegisterPerformancePasses().Run( spv.data(), spv.size(), &spv );
    if( !ok ) spv = {};

    return spv;
}

std::vector<uint32_t> compileShaderToSPIRVFromFile( EShLanguage stage, const char *fileName )
{
    std::string shaderSource = GetFileString( fileName, false );
    if( !shaderSource.empty() ) return compileShaderToSPIRV( stage, shaderSource.c_str(), fileName );

    return std::vector<uint32_t>();
}

EShLanguage stageFromFilename( const char *fileName )
{
    std::string_view fn = fileName;

    std::string_view ext = fn;

    size_t pos = fn.rfind( '.' );
    if( pos != fn.npos ) ext = fn.substr( pos+1 );
    if( ext == "glsl" && pos != 0 )
    {
        size_t pos2 = fn.rfind( '.', pos - 1 );
        if( pos2 != fn.npos ) ext = fn.substr( pos2 + 1, pos - pos2 - 1 );
    }

    struct ExtToLang {
        std::string_view ext;
        EShLanguage lang;
    };

    const ExtToLang langs[] = {
        "vert", EShLangVertex,
        "tesc", EShLangTessControl,
        "tese", EShLangTessEvaluation,
        "geom", EShLangGeometry,
        "frag", EShLangFragment,
        "comp", EShLangCompute,
        "rgen", EShLangRayGen,
        "rint", EShLangIntersect,
        "rahit",EShLangAnyHit,
        "rchit",EShLangClosestHit,
        "rmiss",EShLangMiss,
        "rcall",EShLangCallable,
        "task", EShLangTaskNV,
        "mesh", EShLangMeshNV
    };

    for( const ExtToLang &lang : langs )
        if( ext == lang.ext )
            return lang.lang;

    return EShLangCount;
}

std::vector<uint32_t> getShaderOrGenerate( EShLanguage stage, const char *shaderSource )
{
    std::filesystem::path SrcPath = shaderSource;
    std::filesystem::path BinPath = SrcPath;
    BinPath += ".spv";

    bool SourceExists = std::filesystem::exists( SrcPath );
    bool BinExists = std::filesystem::exists( BinPath );

    if( !SourceExists && !BinExists ) return std::vector<uint32_t>();

    auto sourceTime = SourceExists ? std::filesystem::last_write_time( SrcPath ) : std::filesystem::file_time_type{};
    auto binaryTime = BinExists ? std::filesystem::last_write_time( BinPath ) : std::filesystem::file_time_type{};

    if( binaryTime >= sourceTime )
    {
        std::string binPathStr = BinPath.string();
        std::string FileData = GetFileString( binPathStr.c_str(), true );
        
        if( 
            ( ( FileData.size() % sizeof( uint32_t ) ) == 0 ) &&    // SPIR-V is a vector of ints
            FileData.size() >= 4 * sizeof( uint32_t )               // header size
            )
        {
            std::vector<uint32_t> SPIRV( FileData.size() / sizeof(uint32_t) );
            memcpy(SPIRV.data(), FileData.data(), FileData.size() );
            uint32_t currnetEndianMagicNumber = 0x07230203;

            uint32_t magicNumber = SPIRV[ 0 ];
            if( magicNumber == psnip_builtin_bswap32( currnetEndianMagicNumber ) )
            {
                for( uint32_t &v : SPIRV )
                    v = psnip_builtin_bswap32( v );
            }

            if( magicNumber == currnetEndianMagicNumber )
            {
                // we have SPIRV in native endianness
                
                return SPIRV;
            }
        }
    }

    // there is no binary file or it does not exist ==> regenerate it

    if( SourceExists )
    {
        std::vector<uint32_t> SPIRV = compileShaderToSPIRVFromFile( stage, shaderSource );
        if( SPIRV.size() != 0 )
        {
            std::ofstream outBin;
            outBin.open( BinPath, std::ios::binary | std::ios::out );
            if( !outBin.fail() )
            {
                for( uint32_t word : SPIRV )
                    outBin.write( (const char *)&word, 4 );

                outBin.close();
            }

            return SPIRV;
        }
    }

    // binary and source files does not exit or are invalid
    return std::vector<uint32_t>();
}