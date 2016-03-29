#include <exception>

#include "../include/aassert.h"
#include "../include/aconsole.h"

using namespace std;
using namespace autom;

int main()
{
    try
    {
        infraConsole.trace( "Starting..." );
		infraConsole.assignNewConsole( unique_ptr<FileConsole>(new FileConsole(cout)) );
		infraConsole.info( "Hello, {0}{1}", "World",'!' );
        ATRACE3( "TRACE LEVEL IS {0}, line {1}", infraConsole.traceLevel(), __LINE__ );
        throw AssertionError( "a>b", __FILE__, __LINE__, "error #{}", 3 );
    }
    catch( const std::exception& e )
    {
		infraConsole.error( "std::exception '{}'", e.what() );
        return 1;
    }
    catch( ... )
    {
		infraConsole.error( "Unknown exception!" );
        return 2;
    }

    return 0;
}
