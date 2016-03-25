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
        console.trace( "Hello, {0}{1}", "World",'!' );
        ATRACE3( "TRACE LEVEL IS {0}, line {1}", console.traceLevel(), __LINE__ );
        throw std::runtime_error( "42" );

        return 0;
    }
    catch( const std::exception& e )
    {
        console.trace( "Exception '{0}'", e.what() );
        return 1;
    }
    catch( ... )
    {
        console.trace( "Unknown exception!" );
        return 2;
    }
}
