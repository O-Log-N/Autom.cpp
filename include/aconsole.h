/*******************************************************************************
Copyright (C) 2016 OLogN Technologies AG
	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License version 2 as
	published by the Free Software Foundation.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	You should have received a copy of the GNU General Public License along
	with this program; if not, write to the Free Software Foundation, Inc.,
	51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*******************************************************************************/

#ifndef ACONSOLE_H_INCLUDED
#define ACONSOLE_H_INCLUDED

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
		Console* console;

	public:
		void assign( Console* console_ ) { console = console_; }
		template< typename... ARGS >
		void log( const char* formatStr, const ARGS& ... args ) { console->info( formatStr, args... ); }
		static int traceLevel() { return ATRACELEVEL; }

		ConsoleWrapper() { console = nullptr; };
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
