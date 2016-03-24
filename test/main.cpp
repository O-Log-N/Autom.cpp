#include <exception>

#include "../include/aassert.h"
#include "../include/aconsole.h"

using namespace std;
using namespace autom;

ConsoleWrapper console;

static int divide( int a, int b )
{
	return a / b;
}

int main()
{
	try
	{
		console.log( "Starting..." );
		console.assign( new Console );
		console.log( "Hello, {0}{1}", "World",'!' );
		AATRACE3( "TRACE LEVEL IS {0}, line {1}", console.traceLevel(), __LINE__ );
		throw std::runtime_error( "42" );

		return 0;
	}
	catch( const std::exception& e )
	{
		console.log( "Exception '{0}'", e.what() );
		return 1;
	}
	catch( ... )
	{
		console.log( "Unknown exception!" );
		return 2;
	}
}
