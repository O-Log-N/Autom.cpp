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
 		auto idt2 = console.timeWithLabel();
		auto idt1 = console.timeWithLabel();
		console.info( "Hello, {0}{1}", "World",'!' );
        ATRACE3( "TRACE LEVEL IS {0}, line {1}", console.traceLevel(), __LINE__ );
		console.timeEnd( idt1, "1" );
		console.timeEnd( idt2, "2" );
		throw AssertionError( "a>b", __FILE__, __LINE__, "error #{}", 3 );
    }
    catch( const std::exception& e )
    {
        console.error( "std::exception '{}'", e.what() );
        return 1;
    }
    catch( ... )
    {
        console.error( "Unknown exception!" );
        return 2;
    }

    return 0;
}
