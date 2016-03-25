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
	class ConsoleBase {
		int softTraceLevel = ATRACE_SOFT_DEFAULT;

	public:
		virtual void formattedTrace( const char* s ) = 0;
		virtual void formattedInfo( const char* s ) = 0;
		virtual void formattedNotice( const char* s ) = 0;
		virtual void formattedWarn( const char* s ) = 0;
		virtual void formattedError( const char* s ) = 0;
		virtual void formattedCritical( const char* s ) = 0;
		virtual void formattedAlert( const char* s ) = 0;
		virtual ~ConsoleBase() {
		}

		int traceLevel() const { return softTraceLevel; }

		template< typename... ARGS >
		void trace( const char* formatStr, const ARGS& ... args )
		{
			std::string s = fmt::format( formatStr, args... );
			formattedTrace(s.c_str());
		}
		void trace0() {}
		void trace1() {}
		void trace2() {}
		void trace3() {}
		template< typename... ARGS >
		void trace4( const char* formatStr, const ARGS& ... args )
		{
			if( traceLevel() >= 4 )
				trace( formatStr, args... );
		}

		template< typename... ARGS >
		void log( const char* formatStr, const ARGS& ... args ) {
			info( formatStr, args... );
		}
		template< typename... ARGS >
		void info( const char* formatStr, const ARGS& ... args ) {
			std::string s = fmt::format( formatStr, args... );
			formattedInfo(s.c_str());
		}
		void notice() {}
		void warn() {}
		void error() {}
		void critical() {}
		void alert() {}

		void time() {}
		void timeEnd() {}
	};

	class DefaultConsole : public ConsoleBase
	{
	public:
		void formattedTrace( const char* s ) override {
			fmt::print(cout,"TRACE: {}\n",s);
		}
		void formattedInfo( const char* s ) override {
			fmt::print(cout,"INFO: {}\n",s);
		}
		void formattedNotice( const char* s ) override {
			fmt::print(cout,"NOTICE: {}\n",s);
		}
		void formattedWarn( const char* s ) override {
			fmt::print(cout,"WARNING: {}\n",s);
		}
		void formattedError( const char* s ) override {
			fmt::print(cerr,"ERROR: {}\n",s);
		}
		void formattedCritical( const char* s ) override {
			fmt::print(cerr,"CRITICAL: {}\n",s);
		}
		void formattedAlert( const char* s ) override {
			fmt::print(cerr,"ALERT: {}\n",s);
		}
	};
	
	class FileConsole : public ConsoleBase
	{
		std::ostream& os;
	public:
		FileConsole(std::ostream& os)
		: os(os_) {
		}

		void formattedTrace( const char* s ) override {
			fmt::print(os,"TRACE: {}\n",s);
		}
		void formattedInfo( const char* s ) override {
			fmt::print(os,"INFO: {}\n",s);
		}
		void formattedNotice( const char* s ) override {
			fmt::print(os,"NOTICE: {}\n",s);
		}
		void formattedWarn( const char* s ) override {
			fmt::print(os,"WARNING: {}\n",s);
		}
		void formattedError( const char* s ) override {
			fmt::print(os,"ERROR: {}\n",s);
		}
		void formattedCritical( const char* s ) override {
			fmt::print(os,"CRITICAL: {}\n",s);
		}
		void formattedAlert( const char* s ) override {
			fmt::print(os,"ALERT: {}\n",s);
		}
	};

	class ConsoleWrapper
	{
		class ConsoleWrapperDeleter_ {
			bool forever = false;
			
			public:
			void _keepForever() {
				forever = true;
			}
			void operator()(ConsoleBase* c) {
				if(!forever)
					delete c;
			}
		};

		std::unique_ptr<ConsoleBase,ConsoleWrapperDeleter_> console;
		int lostMessages = 0;

	public:
		void assign( std::unique_ptr<ConsoleBase> console_ )
		{
			console = console_.release();
			if( lostMessages )
			{
				console->warn( "autom::ConsoleWrapper: {0} message(s) have been lost before ConsoleWrapper was initialized", lostMessages );
				lostMessages = 0;
			}
		}
		void _keepForever()
		//CAUTION: this function MAY cause memory leaks when used on non-GLOBAL objects
		//  on GLOBAL objects it is fine, and MAY be useful
		//  to allow tracing within global destructors
		//  without worrying about global destructor call order
		{
			console.get_deleter()._keepForever();
		}

		int traceLevel() const
		{
			return console ? console->traceLevel() : ATRACE_LVL_MAX;
		}

		template< typename... ARGS >
		void trace( const char* formatStr, const ARGS& ... args )
		{
			if( console )
				console->trace( formatStr, args... );
			else
				++lostMessages;
		}
		
		ConsoleWrapper( const ConsoleWrapper& ) = delete;
		ConsoleWrapper& operator =( const ConsoleWrapper& ) = delete;
	};
	
	extern ConsoleWrapper console;
}

#if ( ATRACE_LVL_MAX >= 4 )
#define AATRACE4(...)\
do {\
	if(console.traceLevel()>=4)\
		console.trace(__VA_ARGS__);\
	} while(0)
#else
#define AATRACE4(...)
#endif

#if ( ATRACE_LVL_MAX >= 3 )
#define AATRACE3(...)\
do {\
	if(console.traceLevel()>=3)\
		console.trace(__VA_ARGS__);\
	} while(0)
#else
#define AATRACE3(...)
#endif

#if ( ATRACE_LVL_MAX >= 2 )
#define AATRACE2(...)\
do {\
	if(console.traceLevel()>=2)\
		console.trace(__VA_ARGS__);\
	} while(0)
#else
#define AATRACE2(...)
#endif

#if ( ATRACE_LVL_MAX >= 1 )
#define AATRACE1(...)\
do {\
	if(console.traceLevel()>=1)\
		console.trace(__VA_ARGS__);\
	} while(0)
#else
#define AATRACE1(...)
#endif

#define AATRACE0(...)\
do {\
	console.trace(__VA_ARGS__);\
	} while(0)

#endif
