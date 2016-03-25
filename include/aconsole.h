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

#ifndef ATRACE_LVL_MAX
#define ATRACE_LVL_MAX 4 // Compile time max trace level
#endif
static_assert( ATRACE_LVL_MAX >= 0, "ATRACE_LVL_MAX >= 0" );
static_assert( ATRACE_LVL_MAX <= 4, "ATRACE_LVL_MAX <= 4" );

#ifndef ATRACE_SOFT_DEFAULT
#define ATRACE_SOFT_DEFAULT 2 // Default value for runtime trace level
#endif
static_assert( ATRACE_SOFT_DEFAULT >= 0, "ATRACE_SOFT_DEFAULT >= 0" );
static_assert( ATRACE_SOFT_DEFAULT <= ATRACE_LVL_MAX, "ATRACE_SOFT_DEFAULT <= ATRACE_LVL_MAX" );

namespace autom
{
	class Console
	{
		int softTraceLevel = ATRACE_SOFT_DEFAULT;

	public:
		int traceLevel() const { return softTraceLevel; }

		void trace0() {}
		void trace1() {}
		void trace2() {}
		void trace3() {}
		template< typename... ARGS >
		void trace4( const char* formatStr, const ARGS& ... args )
		{
			if( traceLevel() >= 4 )
				info( formatStr, args... );
		}

		template< typename... ARGS >
		void info( const char* formatStr, const ARGS& ... args )
		{
			fmt::print( formatStr, args... );
			fmt::print( "\n" );
		}
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
		Console* console = nullptr;
		int lostMessages = 0;

	public:
		void assign( Console* console_ )
		{
			console = console_;
			if( lostMessages )
			{
				console->trace4( "Lost messages: {0}", lostMessages );
				lostMessages = 0;
			}
		}

		int traceLevel() const
		{
			return console ? console->traceLevel() : ATRACE_LVL_MAX;
		}

		template< typename... ARGS >
		void log( const char* formatStr, const ARGS& ... args )
		{
			if( console )
				console->info( formatStr, args... );
			else
				++lostMessages;
		}
		
		ConsoleWrapper( const ConsoleWrapper& ) = delete;
		ConsoleWrapper& operator =( const ConsoleWrapper& ) = delete;
	};
}

#if ( ATRACE_LVL_MAX >= 4 )
#define AATRACE4(...)\
do {\
	if(console.traceLevel()>=4)\
		console.log(__VA_ARGS__);\
	} while(0)
#else
#define AATRACE4(...)
#endif

#if ( ATRACE_LVL_MAX >= 3 )
#define AATRACE3(...)\
do {\
	if(console.traceLevel()>=3)\
		console.log(__VA_ARGS__);\
	} while(0)
#else
#define AATRACE3(...)
#endif

#if ( ATRACE_LVL_MAX >= 2 )
#define AATRACE2(...)\
do {\
	if(console.traceLevel()>=2)\
		console.log(__VA_ARGS__);\
	} while(0)
#else
#define AATRACE2(...)
#endif

#if ( ATRACE_LVL_MAX >= 1 )
#define AATRACE1(...)\
do {\
	if(console.traceLevel()>=1)\
		console.log(__VA_ARGS__);\
	} while(0)
#else
#define AATRACE1(...)
#endif

#define AATRACE0(...)\
do {\
	console.log(__VA_ARGS__);\
	} while(0)

#endif
