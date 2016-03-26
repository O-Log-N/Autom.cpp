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
class Console {
		int softTraceLevel = ATRACE_LVL_DEFAULT;
		int nMessages = 0;

	public:
    enum WRITELEVEL //NOT using enum class here to enable shorter console.write(Console::INFO,...);
    {
        TRACE = 0, INFO = 1, NOTICE = 2, WARN = 3, ERROR = 4, CRITICAL = 5, ALERT = 6
    };

    virtual void formattedWrite( WRITELEVEL lvl, const char* s ) = 0;
    virtual ~Console() {
		}

    int traceLevel() const {
        return softTraceLevel;
		}
    int messageCount() const {
        return nMessages;
		}

		template< typename... ARGS >
    void write( WRITELEVEL lvl, const char* formatStr, const ARGS& ... args ) {
			std::string s = fmt::format( formatStr, args... );
			++nMessages;
        formattedWrite(lvl,s.c_str());
		}

		void time() {}
		void timeEnd() {}
	};

class DefaultConsole : public Console
{
    const char* _traceMarker( WRITELEVEL lvl ) const {
        switch( lvl )
	{
        case TRACE:
            return "TRACE";
        case INFO:
            return "INFO";
        case NOTICE:
            return "NOTICE";
        case WARN:
            return "WARN";
        case ERROR:
            return "ERROR";
        case CRITICAL:
            return "CRITICAL";
        case ALERT:
            return "ALERT";
        default:
            assert(false);//TODO: HAREASSERT
	}
	}

public:
    void formattedWrite( WRITELEVEL lvl, const char* s ) override {
        fmt::print(lvl >= ERROR ? std::cerr : std::cout,"{}: {}\n",_traceMarker(lvl),s);
		}
	};
	
class FileConsole : public Console
	{
		std::ostream& os;

    const char* _traceMarker( WRITELEVEL lvl ) const {
        switch( lvl )
        {
        case TRACE:
            return "TRACE";
        case INFO:
            return "INFO";
        case NOTICE:
            return "NOTICE";
        case WARN:
            return "WARN";
        case ERROR:
            return "ERROR";
        case CRITICAL:
            return "CRITICAL";
        case ALERT:
            return "ALERT";
        default:
            assert(false);//TODO: HAREASSERT
        }
    }

	public:
		FileConsole(std::ostream& os_)
		: os(os_) {
		}

    void formattedWrite( WRITELEVEL lvl, const char* s ) override {
        fmt::print(os,"{}: {}\n",traceMarker( lvl ),s);
		}
	};

	class ConsoleWrapper
	{
    Console* consolePtr = nullptr;
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
    void assignNewConsole( std::unique_ptr<Console> newConsole ) {
			int nMsg = 0;
			if(consolePtr) {
				nMsg = consolePtr->messageCount();
				delete consolePtr;
			}
			
			consolePtr = newConsole.release();
			prevConsolesMessages += nMsg;
			if( prevConsolesMessages )
            consolePtr->write( Console::INFO, "autom::ConsoleWrapper::assignNewConsole(): {0} message(s) has been sent to previous Console(s)", prevConsolesMessages );
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
    void write( Console::WRITELEVEL lvl, const char* formatStr, const ARGS& ... args )
		{
			_ensureInit();
        consolePtr->write( lvl, formatStr, args... );
		}
		
		ConsoleWrapper( const ConsoleWrapper& ) = delete;
		ConsoleWrapper& operator =( const ConsoleWrapper& ) = delete;
		~ConsoleWrapper() {
			if(consolePtr && !forever) {
				delete consolePtr;
				consolePtr = nullptr;
			}
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
		autom::console.write(autom::Console::TRACE,__VA_ARGS__);\
	} while(0)
#else
#define ATRACE4(...)
#endif

#if ( ATRACE_LVL_MAX >= 3 )
#define ATRACE3(...)\
do {\
	if(console.traceLevel()>=3)\
		autom::console.write(autom::Console::TRACE,__VA_ARGS__);\
	} while(0)
#else
#define ATRACE3(...)
#endif

#if ( ATRACE_LVL_MAX >= 2 )
#define ATRACE2(...)\
do {\
	if(console.traceLevel()>=2)\
		autom::console.write(autom::Console::TRACE,__VA_ARGS__);\
	} while(0)
#else
#define ATRACE2(...)
#endif

#if ( ATRACE_LVL_MAX >= 1 )
#define ATRACE1(...)\
do {\
	if(console.traceLevel()>=1)\
		autom::console.write(autom::Console::TRACE,__VA_ARGS__);\
	} while(0)
#else
#define ATRACE1(...)
#endif

#define ATRACE0(...)\
do {\
	if(console.traceLevel()>=0)\
		autom::console.write(autom::Console::TRACE,__VA_ARGS__);\
	} while(0)

#endif
