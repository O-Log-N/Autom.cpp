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
        auto idt1 = infraConsole.timeWithLabel();
        auto idt2 = infraConsole.timeWithLabel();
        infraConsole.info( "Hello, {0}{1}", "World",'!' );
        infraConsole.timeEnd( idt1, "1" );
        ATRACE3( "TRACE LEVEL IS {0}, line {1}", infraConsole.traceLevel(), __LINE__ );
        infraConsole.timeEnd( idt2, "2" );
        int a = 2, b = 3;
        AASSERT( b > a );
        AASSERT( a > b, "---{}---{}---{}---", 1, 2, 3 );
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
