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
		console.assign( new Console );
		console.log( "Hello, {0}!", "World" );
		AATRACE3( "TRACE LEVEL IS {0}, line {1}", ATRACELEVEL, divide( 10, 2 ));
		throw std::exception( "42" );

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
