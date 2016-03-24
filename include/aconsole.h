#ifndef ACONSOLE_H
#define ACONSOLE_H

#include <memory>
#include "../3rdparty/cppformat/cppformat/format.h"

#ifndef ATRACELEVEL
#define ATRACELEVEL 2
#endif
static_assert( ATRACELEVEL >= 0, "ATRACELEVEL >= 0" );
static_assert( ATRACELEVEL <= 4, "ATRACELEVEL <= 4" );

namespace autom
{
	class Console
	{
	public:

		void trace0() {}
		void trace1() {}
		void trace2() {}
		void trace3() {}
		void trace4() {}

		template< typename... ARGS >
		void info( const char* formatStr, const ARGS& ... args ) { fmt::print( formatStr, args... ); }
		void notice() {}
		void warn() {}
		void error() {}
		void critical() {}
		void alert() {}

		void time() {}
		void timeEnd() {}
	};

	class ConsoleWrapper
	{
		std::unique_ptr< Console > console;

	public:
		void assign( std::unique_ptr< Console > console_ )
		{
			// TODO: AASSERT0( ! console );
			console = std::move( console_ );
		}
		template< typename... ARGS >
		void log( const char* formatStr, const ARGS& ... args ) { console->info( formatStr, args... ); }
		static int traceLevel() { return ATRACELEVEL; }

		ConsoleWrapper() { console = NULL; };
		ConsoleWrapper( const ConsoleWrapper& ) = delete;
		ConsoleWrapper& operator =( const ConsoleWrapper& ) = delete;
	};
}

#define AATRACE3(...)\
do {\
	if(console.traceLevel()>=3)\
		console.log(__VA_ARGS__);\
	} while(0)

#endif
