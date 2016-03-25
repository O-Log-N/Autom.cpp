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
#define ATRACE_LVL_DEFAULT 4 // Default value for runtime trace level
#endif
static_assert( ATRACE_LVL_DEFAULT >= 0, "ATRACE_LVL_DEFAULT >= 0" );
static_assert( ATRACE_LVL_DEFAULT <= ATRACE_LVL_MAX, "ATRACE_LVL_DEFAULT <= ATRACE_LVL_MAX" );

namespace autom
{
class Console {
    int softTraceLevel = ATRACE_LVL_DEFAULT;
    int nMessages = 0;

public:
    enum class TraceLevel : int
    {
        TRACE0 = 0, TRACE1, TRACE2, TRACE3, TRACE4,
        INFO, NOTICE, WARN, ERROR, CRITICAL, ALERT
    };

    virtual void formattedTrace( TraceLevel lvl, const char* s ) = 0;
    virtual ~Console() {
    }

    int traceLevel() const {
        return softTraceLevel;
    }
    int messageCount() const {
        return nMessages;
    }

    template< typename... ARGS >
    void trace( TraceLevel lvl, const char* formatStr, const ARGS& ... args ) {
        std::string s = fmt::format( formatStr, args... );
        ++nMessages;
        formattedTrace(lvl,s.c_str());
    }

    void time() {}
    void timeEnd() {}
};

class DefaultConsole : public Console
{
    const char* traceMarker( TraceLevel lvl ) const
    {
        switch( lvl )
        {
        case TraceLevel::INFO:
            return "INFO";
        case TraceLevel::NOTICE:
            return "NOTICE";
        case TraceLevel::WARN:
            return "WARN";
        case TraceLevel::ERROR:
            return "ERROR";
        case TraceLevel::CRITICAL:
            return "CRITICAL";
        case TraceLevel::ALERT:
            return "ALERT";
        default:
            return "";
        }
    }

public:
    void formattedTrace( TraceLevel lvl, const char* s ) override {
        fmt::print(std::cout,"{}: {}\n",traceMarker(lvl),s);
    }
};

class FileConsole : public Console
{
    std::ostream& os;

    const char* traceMarker( TraceLevel lvl ) const
    {
        switch( lvl )
        {
        case TraceLevel::TRACE0:
            return "T0";
        case TraceLevel::TRACE1:
            return "T1";
        case TraceLevel::TRACE2:
            return "T2";
        case TraceLevel::TRACE3:
            return "T3";
        case TraceLevel::TRACE4:
            return "T4";
        case TraceLevel::INFO:
            return "INFO";
        case TraceLevel::NOTICE:
            return "NOTICE";
        case TraceLevel::WARN:
            return "WARN";
        case TraceLevel::ERROR:
            return "ERROR";
        case TraceLevel::CRITICAL:
            return "CRITICAL";
        case TraceLevel::ALERT:
            return "ALERT";
        default:
            return "";
        }
    }

public:
    FileConsole(std::ostream& os_)
        : os(os_) {
    }

    void formattedTrace( TraceLevel lvl, const char* s ) override {
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
            consolePtr->trace( Console::TraceLevel::INFO, "autom::ConsoleWrapper::assignNewConsole(): {0} message(s) has been sent to previous Console(s)", prevConsolesMessages );
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
    void trace( Console::TraceLevel lvl, const char* formatStr, const ARGS& ... args )
    {
        _ensureInit();
        consolePtr->trace( lvl, formatStr, args... );
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
		console.trace(autom::Console::TraceLevel::TRACE4,__VA_ARGS__);\
	} while(0)
#else
#define ATRACE4(...)
#endif

#if ( ATRACE_LVL_MAX >= 3 )
#define ATRACE3(...)\
do {\
	if(console.traceLevel()>=3)\
		console.trace(autom::Console::TraceLevel::TRACE3,__VA_ARGS__);\
	} while(0)
#else
#define ATRACE3(...)
#endif

#if ( ATRACE_LVL_MAX >= 2 )
#define ATRACE2(...)\
do {\
	if(console.traceLevel()>=2)\
		console.trace(autom::Console::TraceLevel::TRACE2,__VA_ARGS__);\
	} while(0)
#else
#define ATRACE2(...)
#endif

#if ( ATRACE_LVL_MAX >= 1 )
#define ATRACE1(...)\
do {\
	if(console.traceLevel()>=1)\
		console.trace(autom::Console::TraceLevel::TRACE1,__VA_ARGS__);\
	} while(0)
#else
#define ATRACE1(...)
#endif

#define ATRACE0(...)\
do {\
	console.trace(autom::Console::TraceLevel::TRACE0,__VA_ARGS__);\
	} while(0)

#endif
