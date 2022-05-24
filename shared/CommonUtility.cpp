#include "CommonUtility.h"
//#include "Debug.h"
#include <exception>
#include <system_error>
#include <cstring>

using namespace std;

string GetFileString( const char *pth, bool binary )
{
    FILE *fp = fopen( pth, binary ? "rb" : "r" );
    if (!fp) return "";
    //if( !fp )
    //{
    //    //TRACE( DebugLevel::Error, "I/O error: Cannot open '%s'\n", pth );
    //    throw std::runtime_error( string() + "I/O error: Cannot open '" + pth + "'\n" );
    //}

    fseek( fp, 0L, SEEK_END );
    const long int size = ftell( fp );
    fseek( fp, 0L, SEEK_SET );

    char *data = (char*)( size + 1 > 1024 ? malloc( size + 1 ) : alloca( size + 1 ) );
    if( !data )
    {
        //TRACE( DebugLevel::Error, "Memory error: cannot allocate file buffer\n", pth );
        fclose( fp );
        //return "";
        throw std::bad_alloc();
    }

    const size_t BytesRead = fread( data, 1, size, fp );
    fclose( fp );

    data[BytesRead] = '\0';

    string res = binary ? string( data, BytesRead ) : string( data );
    if( size + 1 > 1024 ) free( data );

    if( binary ) return res;
    
    static constexpr unsigned char BOM[] = { 0xEF, 0xBB, 0xBF }; // remove UTF-8 bom if present
    if( BytesRead > 3 && !memcmp( res.data(), BOM, 3 ) )
        memset( res.data(), ' ', 3 );

    return res;
}
