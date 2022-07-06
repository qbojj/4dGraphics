#include "stdafx.h"
#include "Shader.h"

#include "MeshExecution.h"
#include "Debug.h"
#include "CommonUtility.h"

#ifndef _WIN32
#include <sys/stat.h>
#endif

using namespace std;
namespace fs = filesystem;

// constructor generates the shader on the fly
// ------------------------------------------------------------------------

vector<fs::path> GLShader::includeDirs;

// returns 0 if file doesn't exist
static time_t GetFileWriteTime(const char* filePath)
{
    struct stat st;
    return stat(filePath, &st) == -1 ? 0 : st.st_mtime;
}

static bool checkProgramErrors( const GLProgramId &program )
{
    int success;
    char infoLog[1024];
    glGetProgramiv( program, GL_LINK_STATUS, &success );
    if( !success )
    {
        char *msg = infoLog;
        GLsizei s;

        glGetProgramiv( program, GL_INFO_LOG_LENGTH, &s );
        if( s > 1024 ) msg = (char *)malloc( s );

        glGetProgramInfoLog( program, s, NULL, msg );
        TRACE( DebugLevel::Error, "program link error: %s\n", msg );

        if( s > 1024 ) free( msg );
        return false;
    }

    return true;
}

static bool checkShaderErrors( const char *path, const GLShaderId &shader )
{
    int success;
    char infoLog[1024];
    glGetShaderiv( shader, GL_COMPILE_STATUS, &success );
    if( !success )
    {
        char *msg = infoLog;
        GLsizei s;

        glGetShaderiv( shader, GL_INFO_LOG_LENGTH, &s );
        if( s > 1024 ) msg = (char *)malloc( s );

        glGetShaderInfoLog( shader, s, NULL, msg );
        TRACE( DebugLevel::Error, "Shader compilation error: ('%s') %s\n", path, msg );

        if( s > 1024 ) free( msg );
        return false;
    }

    return true;
}

/*
bool GLProgram::LoadFromCode( const char *vShaderCode, const char *fShaderCode )
{
    clear();

    // 2. compile shaders
    GLShaderId vertex, fragment;
    // vertex shader
    vertex = glCreateShader( GL_VERTEX_SHADER );
    glShaderSource( vertex, 1, &vShaderCode, NULL );
    glCompileShader( vertex );
    if( !checkShaderErrors( vertex ) ) return false;
    // fragment Shader
    fragment = glCreateShader( GL_FRAGMENT_SHADER );
    glShaderSource( fragment, 1, &fShaderCode, NULL );
    glCompileShader( fragment );
    if( !checkShaderErrors( fragment ) ) return false;
    // shader Program
    ID = glCreateProgram();
    glAttachShader( ID, vertex );
    glAttachShader( ID, fragment );
    glLinkProgram( ID );
    glDetachShader( ID, vertex );
    glDetachShader( ID, fragment );

    if( !checkProgramErrors( ID ) ) { clear(); return false; }

    return true;
}
*/

constexpr unsigned int CorrectBinMagicNum = 0x1c7e6fdcu;
struct BinaryFileFormat_t
{
    struct BinaryHeader_t
    {
        unsigned int magicNum;
        GLsizei len;
        GLenum binForm;
        bool separable;
        GLbitfield shadersAttached;
    } header;

    unsigned char data[1]; // fits all of shader data (dynamic size)
};

bool GLProgram::LoadFromBinary( const char *pth )
{
    OPTICK_EVENT();

    string data = GetFileString( pth, true );
    if( data == "" ) return false;

    int error = 0;

    {
        constexpr size_t hSize = sizeof( BinaryFileFormat_t::BinaryHeader_t ); 
        if( data.size() < hSize ) { error = 1; goto cleanUp; }

        BinaryFileFormat_t *file = (BinaryFileFormat_t *)data.data();

        if( file->header.magicNum != CorrectBinMagicNum ) { error = 2; goto cleanUp; }
        if( data.size() - hSize != (size_t)file->header.len ) { error = 3; goto cleanUp; }
        if( file->header.separable != separable ) { error = 4; goto cleanUp; }
        if( file->header.shadersAttached != shadersAttached ) { error = 5; goto cleanUp; }

        glProgramBinary( ID, file->header.binForm, file->data, file->header.len );

        GLint success;
        glGetProgramiv( ID, GL_LINK_STATUS, &success );

        return success;
    }

cleanUp:
    TRACE( DebugLevel::Error, "Error: binary shader file '%s' has %s\n",
        pth,
        error == 1 ? "smaller size than size of file header" :
        error == 2 ? "invalid magic number" :
        error == 3 ? "invalid length" : 
        error == 4 ? "different separable settings" :
        error == 5 ? "different shader stages attached" : "UNKNOWN ERROR"
         );

    return false;
}

bool GLProgram::SaveBinary( const char *binaryPath ) const
{
    OPTICK_EVENT();

    if( !ID ) return false;
    GLint len;
    glGetProgramiv( ID, GL_PROGRAM_BINARY_LENGTH, &len );
    if( len == 0 ) return false; // not linked successfully

    size_t binFileSize = sizeof( BinaryFileFormat_t::BinaryHeader_t ) + len;
    unique_ptr<BinaryFileFormat_t> binData( (BinaryFileFormat_t*)calloc( 1, binFileSize ) );

    binData->header.len = len;
    binData->header.magicNum = CorrectBinMagicNum;
    binData->header.separable = separable;
    binData->header.shadersAttached = shadersAttached;

    glGetProgramBinary( ID, len, NULL, &binData->header.binForm, binData->data );

    FILE *f = fopen( binaryPath, "w" );//ofstream f( binaryPath, ios_base::out | ios_base::binary );
    if( !f ) { TRACE( DebugLevel::Error, "Error: binary shader file '%s' cannot be written\n", binaryPath ); return false; }
    //f.write( (char *)binData.get(), binFileSize );
    //f.close();
    fwrite( (char *)binData.get(), 1, binFileSize, f );
    fclose( f );

    return true;
}
/*
bool GLProgram::Load( const Path &vertexPath, const Path &fragmentPath, bool bAllowBinary )
{
   // OutputDebug( "Load shader vert:\"%s\", frag: \"%s\"\n", vertexPath.data(), fragmentPath.data() );
    fs::path binPath;

    if( bAllowBinary )
    {
        fs::path
            vpth = vertexPath.pth(),
            fpth = fragmentPath.pth();

        fs::path dir = vpth;
        dir.remove_filename();

        fs::path vpth2 = vpth, fpth2 = fpth;

        fs::path binDir = dir / "binaries";
        binPath = binDir  / ( vpth2.filename().string() + '\'' + fpth2.filename().string() + ".sbin" );

        if( !fs::exists( binDir ) )
            fs::create_directories( binDir );

        if( fs::exists( binPath ) )
        {
            fs::file_time_type t = fs::last_write_time( binPath );

            if( t > fs::last_write_time( vpth ) &&
                t > fs::last_write_time( fpth ) )
            {
                //OutputDebug( "Load shader from bin: \"%s\"\n", binPath.string().c_str() );
                if( LoadFromBinary( binPath ) ) return InitUniforms();

                fs::remove( binPath );
            }
        }
    }
    
    {
        string vertexCode;
        string fragmentCode;

        if( !GetFileString( vertexCode, vertexPath ) )
        { TRACE( Error, "Error: shader file %s cannot be read\n", vertexPath.data() ); return false; }

        if( !GetFileString( fragmentCode, fragmentPath ) )
        { TRACE( Error, "Error: shader file %s cannot be read\n", fragmentPath.data() ); return false; }

        bool ok = LoadFromCode( vertexCode.c_str(), fragmentCode.c_str() );

        if( bAllowBinary && ok ) SaveBinary( binPath );
        return ok && InitUniforms();
    }
}
*/

string GLProgram::GetBinPath() const
{
    fs::path dir = shaders[0].data->ShaderFilePath;
    dir.remove_filename();
    dir /= "binaries";
    if( !fs::exists( dir ) ) fs::create_directories( dir );

    bool sep = false;
    string name = "";
    for( const GLShader &s : shaders )
    {
        if( sep ) 
        { 
            name += ";"; 
            sep = true; 
        }
        
        name += fs::path(s.data->ShaderFilePath).filename().string();
    }
    if( separable ) name += ".sep";
    name += ".sbin";

    return ( dir / name ).string();
}

constexpr GLbitfield GLShader::ShaderTypeToShaderTypeBit( GLenum type )
{
#define DEF_TYPE(a) case GL_ ## a ## _SHADER: return GL_ ## a ## _SHADER_BIT;
    switch( type )
    {
        DEF_TYPE( VERTEX )
        DEF_TYPE( TESS_CONTROL )
        DEF_TYPE( TESS_EVALUATION )
        DEF_TYPE( GEOMETRY )
        DEF_TYPE( FRAGMENT )
        DEF_TYPE( COMPUTE )
    }

    assert( !"shader type %x is invalid" );
    return 0;
#undef DEF_TYPE
}

static string SkipComments( const string &_line, bool &InMultilineComment )
{
    string res = _line;
    if( InMultilineComment )
    {
        size_t longCommentEndPos = res.find( "*/" );
        if( longCommentEndPos != string::npos ) InMultilineComment = false, res = res.substr( longCommentEndPos + 2 );
        else return "";
    }

    bool ok = false;

    while( !ok )
    {
        ok = true;

        size_t lineCommentPos = res.find( "//" );
        size_t longCommentStartPos = res.find( "/*" );

        if( lineCommentPos != string::npos &&
            (longCommentStartPos == string::npos || lineCommentPos < longCommentStartPos)
            )
            res.resize( lineCommentPos );
        else
        {

            if( longCommentStartPos != string::npos )
            {
                size_t longCommentEndPos = res.find( "*/" );
                if( longCommentEndPos != string::npos ) ok = false, res = res.substr( 0, longCommentStartPos ) + res.substr( longCommentEndPos + 2 );
                else
                {
                    InMultilineComment = true;
                    res = res.substr( 0, longCommentStartPos );
                }
            }
        }
    }

    return res;
}

static void skipWhitespaces( const char *&s ) { while( isspace( *s ) ) ++s; }

fs::path GLShader::ChoseShaderPath( const fs::path &name, const fs::path &cwd, bool *ok )
{
    if (ok) *ok = true;
    fs::path checked = ( cwd / name );

    if( fs::is_regular_file(checked) ) return checked.lexically_normal();

    for (const fs::path& dir : includeDirs)
    {
        checked = (dir / name);
        if (fs::is_regular_file(checked)) return checked.lexically_normal();
    }

    if (ok) *ok = false;
    return "";
}

bool GLShader::ResolveIncludes( 
    vector<string> &sourceList, 
    const char *filePath,
    vector<FileInfo_t> &deps
)
{
    OPTICK_EVENT();

    constexpr bool ResolveIncludesDebug = IS_DEBUG;

    sourceList.clear();

    unordered_map<string, int> filesToVert;
    vector<vector<int>> dependencyGraph;
    vector<string> SourceStrs;

    int filesCnt = 1;

    int glslVer = 110;
    bool compat = false;

    fs::path filePath2 = fs::absolute(filePath);
    string FormatedPath = filePath2.string();

    if (!fs::is_regular_file(filePath2))
    {
        TRACE(DebugLevel::Error, "Error: shader file '%s' does not exist or is not a regular file\n", FormatedPath.c_str());
        return false;
    }

    filesToVert[FormatedPath] = 0;
    deps = { { FormatedPath, 0 } };

    for( size_t SourceToParse = 0; SourceToParse < deps.size(); SourceToParse++ )
    {
        string currentPath = deps[SourceToParse].path;
        dependencyGraph.emplace_back();

        fs::path includeDir = fs::path(currentPath).parent_path();

        string source = GetFileString( currentPath.c_str(), false );
        if( source == "" ) return false;

        deps[SourceToParse].wtime = GetFileWriteTime( currentPath.c_str() );
        if( deps[SourceToParse].wtime == 0 )
        {
            TRACE( DebugLevel::Error, "Error: cannot read file's last write time '%s'\n", currentPath.c_str() );
            return false;
        }

        istringstream input( move( source ) );
        ostringstream output;

        bool InLongComment = false;

        bool IsFirstNonCommentLine = true;
        bool IsNextNonCommentLine = true;
        string fileLine;
        for( size_t line_number = 1; std::getline( input, fileLine ); line_number++ )
        {
            //if( fileLine.find_first_not_of( " \t\r\n" ) == string::npos ) continue;
            IsFirstNonCommentLine = IsNextNonCommentLine;
            string commentLessLine = SkipComments( fileLine, InLongComment );

            const char *s = &commentLessLine[0];
            skipWhitespaces( s );
            if( !*s ) IsNextNonCommentLine = false;

            bool LineProcessed = false;

            if( *s++ == '#' )
            {
                skipWhitespaces( s );
                if( strncmp( s, "include", 7 ) == 0 )
                {
                    s += 7;
                    skipWhitespaces( s );

                    const char *start = NULL, *end = NULL;

                    bool bad = *s != '\"';
                    if( !bad )
                    {
                        end = start = s + 1;

                        while( *end != '\"' && *end ) end++;

                        if( !*end ) bad = true;
                    }

                    if( bad )
                    {
                        TRACE( DebugLevel::Error, "Error: include is not in '\" \"' ( line %d in ('%s') )\n", line_number, currentPath.c_str() );
                        return false;
                    }

                    string includeName( start, end - start );

                    bool FileOK;
                    string include_file = ChoseShaderPath( includeName, includeDir, &FileOK).string();

                    //string include_file = fs::path( includeDir ).append( includeName ).lexically_normal().string();

                    if (!FileOK)
                    {
                        TRACE(DebugLevel::Error, "Error: include file '%s' could not be found ( line %d in ('%s') )\n", includeName.c_str(), line_number, currentPath.c_str());
                        return false;
                    }

                    int idx;

                    auto p = filesToVert.find( include_file );
                    if( p == filesToVert.end() ) // check if it was not already included
                    {
                        //sourcesToParse.push( include_file );
                        filesToVert[include_file] = idx = filesCnt++;
                        deps.push_back( { move(include_file), 0 } );
                    }
                    else idx = p->second;

                    dependencyGraph.back().push_back( idx );

                    LineProcessed = true;
                }
                else if( strncmp( s, "version", 7) == 0 )
                {
                    s += 7;

                    if( !IsFirstNonCommentLine )
                    {
                        TRACE( DebugLevel::Error, 
                            "Error: #version appear before anything else in a shader"
                            ", save for whitespace and comments! (line %d in '%s')\n", 
                            line_number, currentPath.c_str() );
                        return false;
                    }

                    int ver; char name[16];
                    int cnt = sscanf( s, "%d %15s", &ver, name );
                    int profile = 0;

                    if( cnt == 0 )
                    {
                        TRACE( DebugLevel::Error,
                            "Error: #version must have at least version number afterwards (line %d in '%s')\n",
                            line_number, currentPath.c_str() );
                        return false;
                    }

                    if( cnt == 2 )
                    {
                        if( strcmp( name, "core" ) == 0 ) profile = 0;
                        else if( strcmp( name, "compatibility" ) == 0 ) profile = 1;
                        else
                        {
                            TRACE( DebugLevel::Error,
                                "Error: #version profile must be either 'core' or 'compatibility' (line %d in '%s')\n",
                                line_number, currentPath.c_str() );
                            return false;
                        }
                    }

                    if( profile == 1 ) compat = true;
                    glslVer = max( ver, glslVer );

                    LineProcessed = true;
                }
            }
            
            if( LineProcessed ) output << "// ";
            output << (ResolveIncludesDebug ? fileLine : commentLessLine);
            if( line_number == 1 ) output << " // FILE = " << currentPath;
            output << "\n";
        }

        //if constexpr( ResolveIncludesDebug ) output << "// END FILE: \"" << filePath << "\"\n";
        SourceStrs.push_back( move( output ).str() );
    }

    assert( filesCnt == SourceStrs.size() && filesCnt == dependencyGraph.size() );

    //vector<int> sourcePermutation( filesCnt );
    //iota( sourcePermutation.begin(), sourcePermutation.end(), 0 );

    //TODO: RESOLVE DEPENDENCY GRAPH

    //sourceList.resize( filesCnt );
    //for( int i = 0; i < filesCnt; i++ ) sourceList[i] = move( SourceStrs[sourcePermutation[filesCnt-i-1]] );

    struct ToposortData_t
    {
        vector<unsigned char> marks;
        const vector<vector<int>> &graph;
        vector<int> outPerm;
    } toposortDat
    {
        .marks = vector<unsigned char>( filesCnt, 0 ),
        .graph = dependencyGraph,
        .outPerm = {}
    };

    using dfsProc = bool(*)(int, void*, ToposortData_t &);
    dfsProc dfs = []( int u, void*dfs, ToposortData_t &dat ) -> bool
    {
        if( dat.marks[u] == 2 ) return true;
        if( dat.marks[u] == 1 ) return false; // there is a cycle

        dat.marks[u] = 1;
        for( int v : dat.graph[u] ) if( ! ((dfsProc)dfs)( v, dfs, dat ) ) return false;
        dat.marks[u] = 2;
        dat.outPerm.push_back( u );

        return true;
    };

    bool next = true;
    while( next )
    {
        next = false;

        for( int i = 0; i < filesCnt; i++ )
        {
            if( toposortDat.marks[i] == 0 )
            {
                next = true;
                if( !dfs( i, (void*)dfs, toposortDat ) ) goto IncludeCycle;
            }
        }
    }

    {
        ostringstream outGeneratedFile;
        outGeneratedFile << "#version " << glslVer << " " << (compat ? "compatibility" : "core") << "\n";
        sourceList.push_back( move( outGeneratedFile ).str() );
    }

    for( int q : toposortDat.outPerm )
        sourceList.push_back( move( SourceStrs[q] ) );

    return true;

IncludeCycle:
    TRACE( DebugLevel::Error, "Includes have a cycle ('%s')\n", filePath );
    return false;
}

bool GLShader::lessThan( const GLShader &a, const GLShader &b )
{
    constexpr GLenum ShaderStagesInDescendingOrder[]{
        GL_VERTEX_SHADER,
        GL_TESS_CONTROL_SHADER,
        GL_TESS_EVALUATION_SHADER,
        GL_GEOMETRY_SHADER,
        GL_FRAGMENT_SHADER,
        GL_COMPUTE_SHADER
    };
    constexpr size_t siz = sizeof( ShaderStagesInDescendingOrder ) / sizeof( GLenum );

    return
        find( ShaderStagesInDescendingOrder, ShaderStagesInDescendingOrder + siz, a.data->type ) <
        find( ShaderStagesInDescendingOrder, ShaderStagesInDescendingOrder + siz, b.data->type );
}

bool GLShader::AddIncudeDir( const char *dir )//; , const std::string &GLSLNameDir )
{
    OPTICK_EVENT();

    
    fs::path p = fs::absolute( dir ).lexically_normal();
    if( !fs::is_directory( p ) )
    {
        TRACE( DebugLevel::Error, "I/O error: SetIncudeDir '%s' does not exist or is not a dir\n", dir );
        return false;
    }

    includeDirs.push_back( p );

    return true;
    /*
    if( !GLAD_GL_ARB_shading_language_include )
    {
        TRACE( DebugLevel::Error, "Error: RegisterIncudeDir used when GL_ARB_shading_language_include is not avaiable\n" );
        return false;
    }

    fs::path p = filePath.c_str();
    if( !fs::is_directory( p ) )
    {
        TRACE( DebugLevel::Error, "Error: shader include dir '%s' is not a directory\n", filePath.c_str() );
        return false;
    }
    fs::directory_iterator endIt; // default construction yields past-the-end

    bool EverythingOK = true;

    for( fs::directory_iterator f( p ); f != endIt; f++ )
    {
        Path filePath = f->path();
        if( f->exists() )
        {
            if( f->is_directory() ) RegisterIncudeDir( filePath, GLSLNameDir + Path( f->path().filename() ) );
            else
            {
                std::string content;
                if( GetFileString( content, filePath ) )
                {
                    std::string GLSLFilePath = GLSLNameDir + Path( f->path().filename() );
                    glNamedStringARB( GL_SHADER_INCLUDE_ARB, 
                        (GLint)GLSLFilePath.length(), (const GLchar*)GLSLFilePath.c_str(),
                        (GLint)content.length(), (const GLchar *)content.c_str() );
                }
                else EverythingOK = false, TRACE( DebugLevel::Error, "Error: shader include file '%s' can not be read\n", filePath.c_str() );
            }
        }
        else EverythingOK = false, TRACE( DebugLevel::Error, "Error: shader include file '%s' does not exist\n", filePath.c_str() );
    }

    return EverythingOK;
    */
}

bool GLProgram::CreateInternal()
{
    OPTICK_EVENT();

    if( !ID )
    {
        ID = glCreateProgram();
        if( !ID ) { TRACE( DebugLevel::Error, "Error: glCreateProgram returned invalid handle\n" ); return false; }

        glProgramParameteri( ID, GL_PROGRAM_SEPARABLE, separable ? GL_TRUE : GL_FALSE );
        glProgramParameteri( ID, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE );
    }

    shadersAttached = 0;
    for( GLShader &s : shaders )
        shadersAttached |= GLShader::ShaderTypeToShaderTypeBit( s.data->type );

#if SHADER_USE_BINARY
    string binPath = GetBinPath();

    if( time_t t = GetFileWriteTime( binPath.c_str() ) )
    {
        bool ok = true;

        for( const GLShader &s : shaders )
        {
            // !!! Doesn't work when include file changes and GLShader was created with lazy=true !!!
            if( t < max( GetFileWriteTime( s.data->ShaderFilePath.c_str() ), s.GetLastWriteTime() ) )
            {
                ok = false;
                break;
            }
        }

        if( ok && LoadFromBinary( binPath ) )
        {
            ShadersReadMaxTime = t;
            return InitUniforms();
        }

        fs::remove( binPath );
    }
#endif

    ShadersReadMaxTime = 0;
    bool ok = true;
    for( GLShader &s : shaders )
    {
        s.Reload();
        ok &= s.data->compileStatus == GLShader::ShaderCompileStatus::success;

        ShadersReadMaxTime = max( ShadersReadMaxTime, s.GetLastWriteTime() );
    }

    if( !ok )
    {
        TRACE( DebugLevel::Error, "Error: at least one of attached shaders is not compiled!\n" );
        return false;
    }

    for( const GLShader &s : shaders ) glAttachShader( ID, s.data->ID );

    glLinkProgram( ID );

    for( const GLShader &s : shaders ) glDetachShader( ID, s.data->ID );

    if( !checkProgramErrors( ID ) ) return false;
    return true;// InitUniforms();
}

bool GLProgram::Create( std::vector<GLShader> _shaders, bool _separable )
{
    clear();
    shaders = move(_shaders);
    sort( shaders.begin(), shaders.end(), GLShader::lessThan );
    separable = _separable;

    return CreateInternal();
}

bool GLProgram::Reload()
{
    for( GLShader &s : shaders )
    {
        if( s.GetLastWriteTime() < ShadersReadMaxTime )
        {
            CreateInternal();
            return true;
        }
    }

    return false;
}
/*
bool GLProgram::InitUniforms()
{

    Uniforms.clear();
    // non-block uniforms
    {
        GLint numUniforms = 0;
        glGetProgramInterfaceiv( ID, GL_UNIFORM, GL_ACTIVE_RESOURCES, &numUniforms );
        const gl::GLenum properties[4] = { GL_BLOCK_INDEX, GL_TYPE, GL_NAME_LENGTH, GL_LOCATION };

        for( int unif = 0; unif < numUniforms; ++unif )
        {
            GLint values[4];
            glGetProgramResourceiv( ID, GL_UNIFORM, unif, 4, properties, 4, NULL, values );

            // Skip any uniforms that are in a block.
            if( values[0] != -1 ) continue;

            std::vector<char> nameData( values[2] );
            glGetProgramResourceName( ID, GL_UNIFORM, unif, (GLsizei)nameData.size(), NULL, &nameData[0] );

            Uniforms[ string( nameData.begin(), nameData.end() - 1 ) ] = { values[3], (GLenum)values[1] };
        }
    }

    return true;
}
*/
bool GLShader::Load( const char *codePath, GLenum shaderType, bool lazy )
{
    clear();
    return LoadInternal( codePath, shaderType, lazy );
}

bool GLShader::NeedsReload() const
{
    if( data->compileStatus == ShaderCompileStatus::lazy ) return true;

    if( data && data->ID )
        for( const FileInfo_t &i : data->Dependencies )
            if( i.wtime < GetFileWriteTime( i.path.c_str() ) )
                return true;

    return false;
}

time_t GLShader::GetLastWriteTime() const
{
    time_t t = 0;

    if( data && data->ID )
        for( const FileInfo_t &i : data->Dependencies )
            t = max( t, i.wtime );

    return t;
}

bool GLShader::Reload()
{
    bool reload = NeedsReload();
    if( reload ) LoadInternal( data->ShaderFilePath.c_str(), data->type, false );
    return reload;
}

bool GLShader::LoadInternal( const char *codePath, GLenum shaderType, bool lazy )
{
    OPTICK_EVENT();

    if( !data )
    {
        data = make_shared<GLShaderData_t>();
        data->ShaderFilePath = fs::absolute( codePath ).string();
        data->type = shaderType;
        data->ID = glCreateShader( shaderType );
        data->compileStatus = ShaderCompileStatus::lazy;

        if( !data->ID )
        {
            TRACE( DebugLevel::Error, "Error: glCreateShader returned invalid handle\n" );
            return false;
        }
    }

    assert( data->ShaderFilePath == fs::absolute( codePath ).string() );

    data->Dependencies = { { data->ShaderFilePath, 0 } };

    if( lazy ) return true;

    vector<string> code;

    if( !ResolveIncludes( code, data->ShaderFilePath.c_str(), data->Dependencies ) )
    {
        TRACE( DebugLevel::Error, "Error: unsuccessful shader include resolve in '%s'\n", codePath );
        data->compileStatus = ShaderCompileStatus::error;
        return false;
    }

    vector<const GLchar *> codeArray( code.size() );
    vector<GLint> sizeArray( code.size() );
    for( int i = 0; i < (int)code.size(); i++ ) codeArray[i] = code[i].c_str(), sizeArray[i] = (GLint)code[i].size();

    glShaderSource( data->ID, (GLsizei)code.size(), codeArray.data(), sizeArray.data() );
    glCompileShader( data->ID );
    
    if( !checkShaderErrors( data->ShaderFilePath.c_str(), data->ID ) )
    {
        if( LogLevel <= DebugLevel::Debug )
        {
            stringstream ShaderSource;
            int line = 0;

            for( size_t i = 0; i < code.size(); i++ )
            {
                const GLchar *str = codeArray[i];

                // insert file line at the start of a line
                ShaderSource << '\n' << setw(3) << ++line << "> ";
                
                // copy to a new line
                //while( *str && *str != '\n' ) ShaderSource << *str++;

                // add file path
                //ShaderSource << "  // file: '" << data->Dependencies[i].path << '\'';

                // and continue with the rest of code and insert file lines
                while( *str )
                {
                    bool insert = *str == '\n';
                    ShaderSource << *str++;

                    if( insert ) ShaderSource << setw(3) << ++line << "> ";
                }
            }

            TRACE( DebugLevel::Error, "Shader source:%s\n", ShaderSource.str().c_str() );
        }

        data->compileStatus = ShaderCompileStatus::error;
        return false;
    }

    data->compileStatus = ShaderCompileStatus::success;
    return true;
}

