#include "CommonUtility.h"
//#include "Debug.h"
#include <exception>
#include <system_error>
#include <cstring>
#include <fstream>

using namespace std;

string GetFileString( const string &pth, bool binary )
{
    ifstream file( pth, binary ? ios_base::binary : ios_base::in );
    if( !file ) return "";
    
    file.seekg(0, ios::end);
    streampos length = file.tellg();
    file.seekg(0, ios::beg);

    string buffer(length, '\0');
    file.read(buffer.data(), length);
    
    if( !file ) return "";
    file.close();

    if( binary ) return buffer;

    // text mode -> try removing UTF-8 BOM
    static constexpr unsigned char BOM[] = { 0xEF, 0xBB, 0xBF };
    if( buffer.size() > 3 && !memcmp( buffer.data(), BOM, 3 ) )
        return buffer.substr(3);

    return buffer;
}
