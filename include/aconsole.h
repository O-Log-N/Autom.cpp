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

#include <iostream>

#ifndef ATRACE_LVL_MAX
#define ATRACE_LVL_MAX 4 // Compile time max trace level
#endif
static_assert( ATRACE_LVL_MAX >= 0, "ATRACE_LVL_MAX >= 0" );
static_assert( ATRACE_LVL_MAX <= 4, "ATRACE_LVL_MAX <= 4" );

#ifndef ATRACE_LVL_DEFAULT
#define ATRACE_LVL_DEFAULT 2 // Default value for runtime trace level
#endif
static_assert( ATRACE_LVL_DEFAULT >= 0, "ATRACE_LVL_DEFAULT >= 0" );
static_assert( ATRACE_LVL_DEFAULT <= ATRACE_LVL_MAX, "ATRACE_LVL_DEFAULT <= ATRACE_LVL_MAX" );

namespace autom
{
	class ConsoleBase {
		int softTraceLevel = ATRACE_LVL_DEFAULT;
		int nMessages = 0;

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
		int messageCount() const { return nMessages; }

		template< typename... ARGS >
		void trace( const char* formatStr, const ARGS& ... args )
		{
			std::string s = fmt::format( formatStr, args... );
			++nMessages;
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
			++nMessages;
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
			fmt::print(std::cout,"TRACE: {}\n",s);
		}
		void formattedInfo( const char* s ) override {
			fmt::print(std::cout,"INFO: {}\n",s);
		}
		void formattedNotice( const char* s ) override {
			fmt::print(std::cout,"NOTICE: {}\n",s);
		}
		void formattedWarn( const char* s ) override {
			fmt::print(std::cout,"WARNING: {}\n",s);
		}
		void formattedError( const char* s ) override {
			fmt::print(std::cout,"ERROR: {}\n",s);
		}
		void formattedCritical( const char* s ) override {
			fmt::print(std::cout,"CRITICAL: {}\n",s);
		}
		void formattedAlert( const char* s ) override {
			fmt::print(std::cout,"ALERT: {}\n",s);
		}
	};
	
	class FileConsole : public ConsoleBase
	{
		std::ostream& os;
	public:
		FileConsole(std::ostream& os_)
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
		ConsoleBase* consolePtr = nullptr;
			//we're NOT using std::unique_ptr<> here
			//  to guarantee that for a global ConsoleWrapper
			//  consolePtr is set before ANY global object constructor is called
		int prevConsolesMessages = 0;//as in 'messages to previous consoles'
		bool forever = false;

	public:
		ConsoleWrapper() {
			//it is IMPORTANT to have this constructor even though all the functions
			//  account for consolePtr possible being nullptr
			//This constructor guarantees that before main() we have the consolePtr valid,
			//  and that therefore no issues can arise due to multithreading
			//  (except when assignNewConsole() is explicitly called)
			_ensureInit();
		}
		void assignNewConsole( std::unique_ptr<ConsoleBase> newConsole ) {
			int nMsg = 0;
			if(consolePtr) {
				nMsg = consolePtr->messageCount();
				delete consolePtr;
			}
			
			consolePtr = newConsole.release();
			prevConsolesMessages += nMsg;
			if( prevConsolesMessages )
				consolePtr->info( "autom::ConsoleWrapper::assignNewConsole(): {0} message(s) has been sent to previous Console(s)", prevConsolesMessages );
		}
		void rtfmKeepForever()
		//CAUTION: this function MAY cause memory leaks when used on non-GLOBAL objects
		//  on GLOBAL objects it is fine, and MAY be useful
		//  to allow tracing within global destructors
		//  without worrying about global destructor call order
		{
			forever = true;
		}

		int traceLevel()
		{
			_ensureInit();
			return consolePtr->traceLevel();
		}

		template< typename... ARGS >
		void trace( const char* formatStr, const ARGS& ... args )
		{
			_ensureInit();
			consolePtr->trace( formatStr, args... );
		}
		
		ConsoleWrapper( const ConsoleWrapper& ) = delete;
		ConsoleWrapper& operator =( const ConsoleWrapper& ) = delete;
		~ConsoleWrapper() {
			if(consolePtr && !forever)
				delete consolePtr;
		}
		
		private:
		void _ensureInit() {
			if(!consolePtr)
				consolePtr = new DefaultConsole();
		}
	};
	
	extern ConsoleWrapper console;
}

#if ( ATRACE_LVL_MAX >= 4 )
#define ATRACE4(...)\
do {\
	if(console.traceLevel()>=4)\
		console.trace(__VA_ARGS__);\
	} while(0)
#else
#define ATRACE4(...)
#endif

#if ( ATRACE_LVL_MAX >= 3 )
#define ATRACE3(...)\
do {\
	if(console.traceLevel()>=3)\
		console.trace(__VA_ARGS__);\
	} while(0)
#else
#define ATRACE3(...)
#endif

#if ( ATRACE_LVL_MAX >= 2 )
#define ATRACE2(...)\
do {\
	if(console.traceLevel()>=2)\
		console.trace(__VA_ARGS__);\
	} while(0)
#else
#define ATRACE2(...)
#endif

#if ( ATRACE_LVL_MAX >= 1 )
#define ATRACE1(...)\
do {\
	if(console.traceLevel()>=1)\
		console.trace(__VA_ARGS__);\
	} while(0)
#else
#define ATRACE1(...)
#endif

#define ATRACE0(...)\
do {\
	console.trace(__VA_ARGS__);\
	} while(0)

#endif
