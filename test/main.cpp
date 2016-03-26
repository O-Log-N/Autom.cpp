#include <exception>

#include "../include/aassert.h"
#include "../include/aconsole.h"

using namespace std;
using namespace autom;

int main()
{
    try
    {
        console.trace( "Starting..." );
        console.assignNewConsole( unique_ptr<FileConsole>(new FileConsole(cout)) );
        console.info( "Hello, {0}{1}", "World",'!' );
        ATRACE3( "TRACE LEVEL IS {0}, line {1}", console.traceLevel(), __LINE__ );
        throw std::runtime_error( "42" );
    }
    catch( const std::exception& e )
    {
        console.error( "Exception '{0}'", e.what() );
        return 1;
    }
    catch( ... )
    {
        console.error( "Unknown exception!" );
        return 2;
    }

    return 0;
}
