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
#include <chrono>
#include <unordered_map>
#include <mutex>

#ifndef ATRACE_LVL_MAX
#define ATRACE_LVL_MAX 4 // Compile time max trace level
#endif

//NB: MSVC doesn't support single-parameter static_assert() :-(
static_assert( ATRACE_LVL_MAX >= 0, "ATRACE_LVL_MAX >= 0" );
static_assert( ATRACE_LVL_MAX <= 4, "ATRACE_LVL_MAX <= 4" );

#ifndef ATRACE_LVL_DEFAULT
#define ATRACE_LVL_DEFAULT 2 // Default value for runtime trace level
#endif

//NB: MSVC doesn't support single-parameter static_assert() :-(
static_assert( ATRACE_LVL_DEFAULT >= 0, "ATRACE_LVL_DEFAULT >= 0" );
static_assert( ATRACE_LVL_DEFAULT <= ATRACE_LVL_MAX, "ATRACE_LVL_DEFAULT <= ATRACE_LVL_MAX" );

namespace autom
{
class Console {
    //Note for Infrastructure-level Developers:
    //  Console as such is thread-agnostic
    //  ALL the thread sync is a responsibility of 'wrappers'
public:
    enum WRITELEVEL //NOT using enum class here to enable shorter console.write(Console::INFO,...);
    {
        TRACE = 0, INFO = 1, NOTICE = 2, WARN = 3, ERROR = 4, CRITICAL = 5, ALERT = 6
    };

    class TimeLabel {
    public:
        size_t idx;
        TimeLabel( size_t u ) : idx( u ) {}
    };

private:
    static const size_t ATIMENONE = static_cast<size_t>(-1);
    struct PrivateATimeStoredType {
        size_t nextFree = ATIMENONE;
        std::chrono::time_point<std::chrono::high_resolution_clock> began;
    };

    int softTraceLevel = ATRACE_LVL_DEFAULT;
    int nMessages = 0;//statistics-only; NON-STRICT in MT environments
    size_t firstFreeATime = ATIMENONE;
    std::vector<PrivateATimeStoredType> aTimes;//for Autom-style timeWithLabel()/timeEnd()
    //'sparse' vector with firstFreeATime forming single-linked list
    //in nextFree items
#ifndef ASTRIP_NODEJS_COMPAT
    std::unordered_map<std::string,std::chrono::time_point<std::chrono::high_resolution_clock>> njTimes;
#endif
public:
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

    TimeLabel timeWithLabel();
    void timeEnd(TimeLabel label, const char* text);
    //usage pattern for Autom-style time tracing:
    //auto label = timeWithLabel();
    //... code to be benchmarked
    //timeEnd(label,"My Time Label A");

#ifndef ASTRIP_NODEJS_COMPAT
    //{ NODE.JS COMPATIBILITY HELPERS
    void time(const char* label);
    void timeEnd(const char* label);
    //usage pattern for Node.js-style time tracing
    // (LESS EFFICIENT THAN Autom-style):
    //time("My Time Label A");
    //... code to be benchmarked
    //timeEnd("My Time Label A");

    template< typename... ARGS >
    void error( const char* formatStr, const ARGS& ... args ) {
        write(ERROR,formatStr, args...);
    }
    template< typename... ARGS >
    void info( const char* formatStr, const ARGS& ... args ) {
        write(INFO,formatStr, args...);
    }
    template< typename... ARGS >
    void log( const char* formatStr, const ARGS& ... args ) {
        write(INFO,formatStr, args...);
    }
    template< typename... ARGS >
    void trace( const char* formatStr, const ARGS& ... args ) {
        write(TRACE,formatStr, args...);
    }
    template< typename... ARGS >
    void warn( const char* formatStr, const ARGS& ... args ) {
        write(WARN,formatStr, args...);
    }
    //} NODE.JS COMPATIBILITY HELPERS
#endif
};

class DefaultConsole : public Console
{
public:
    void formattedWrite( WRITELEVEL lvl, const char* s ) override;
};

class FileConsole : public Console
{
    std::ostream& os;

public:
    FileConsole(std::ostream& os_)
        : os(os_) {
    }

    void formattedWrite( WRITELEVEL lvl, const char* s ) override;
};

class InfraConsoleWrapper
{
    Console* consolePtr = nullptr;
    //we're NOT using std::unique_ptr<> here
    //  to guarantee that for a global InfraConsoleWrapper
    //  consolePtr is set before ANY global object constructor is called
    int prevConsolesMessages = 0;//as in 'messages to previous consoles'
    bool forever = false;
    std::mutex mx;//TODO!: to be moved to pimpl (as a separate issue)

public:
    InfraConsoleWrapper() {
        //it is IMPORTANT to have this constructor even though all the functions
        //  account for consolePtr possible being nullptr
        //This constructor guarantees that before main() we have the consolePtr valid,
        //  and that therefore no issues can arise due to multithreading
        //  (except when assignNewConsole() is explicitly called)
        std::lock_guard<std::mutex> lock(mx);//for the same reason as above - calls before being destoyed
        _ensureInit();
    }
    void assignNewConsole( std::unique_ptr<Console> newConsole ) {
        std::lock_guard<std::mutex> lock(mx);
        int nMsg = 0;
        if(consolePtr) {
            nMsg = consolePtr->messageCount();
            delete consolePtr; // TODO: if( ! forever )
        }

        consolePtr = newConsole.release();
        prevConsolesMessages += nMsg;
        if( prevConsolesMessages )
            consolePtr->write( Console::INFO, "autom::InfraConsoleWrapper::assignNewConsole(): {0} message(s) has been sent to previous Console(s)", prevConsolesMessages );
    }
    void rtfmKeepForever()
    //CAUTION: this function MAY cause memory leaks when used on non-GLOBAL objects
    //  on GLOBAL objects it is fine, and MAY be useful
    //  to allow tracing within global destructors
    //  without worrying about global destructor call order
    {
        std::lock_guard<std::mutex> lock(mx);
        forever = true;
    }

    int traceLevel()
    {
        std::lock_guard<std::mutex> lock(mx);
        _ensureInit();
        return consolePtr->traceLevel();
    }

    template< typename... ARGS >
    void write( Console::WRITELEVEL lvl, const char* formatStr, const ARGS& ... args )
    {
        std::lock_guard<std::mutex> lock(mx);
        _ensureInit();
        consolePtr->write( lvl, formatStr, args... );
    }

    void formattedWrite( Console::WRITELEVEL lvl, const char* s ) {
        std::lock_guard<std::mutex> lock(mx);
        _ensureInit();
        consolePtr->formattedWrite(lvl, s);
    }

    InfraConsoleWrapper( const InfraConsoleWrapper& ) = delete;
    InfraConsoleWrapper& operator =( const InfraConsoleWrapper& ) = delete;
    ~InfraConsoleWrapper() {
        std::lock_guard<std::mutex> lock(mx);//doesn't really make sense, but reduces vulnerability window a bit
        if(consolePtr && !forever) {
            delete consolePtr;
            consolePtr = nullptr;//important here as global InfraConsoleWrapper MAY
            //  outlive its own destructor
        }
    }

    Console::TimeLabel timeWithLabel() {
        std::lock_guard<std::mutex> lock(mx);
        _ensureInit();
        return consolePtr->timeWithLabel();
    }
    void timeEnd(Console::TimeLabel label, const char* text) {
        std::lock_guard<std::mutex> lock(mx);
        _ensureInit();
        consolePtr->timeEnd(label, text);
    }

#ifndef ASTRIP_NODEJS_COMPAT
    //{ NODE.JS COMPATIBILITY HELPERS
    void time(const char* label) {
        std::lock_guard<std::mutex> lock(mx);
        _ensureInit();
        consolePtr->time(label);
    }
    void timeEnd(const char* label) {
        std::lock_guard<std::mutex> lock(mx);
        _ensureInit();
        consolePtr->timeEnd(label);
    }

    template< typename... ARGS >
    void error( const char* formatStr, const ARGS& ... args ) {
        std::lock_guard<std::mutex> lock(mx);
        consolePtr->error(formatStr, args...);
    }
    template< typename... ARGS >
    void info( const char* formatStr, const ARGS& ... args ) {
        std::lock_guard<std::mutex> lock(mx);
        consolePtr->info(formatStr, args...);
    }
    template< typename... ARGS >
    void log( const char* formatStr, const ARGS& ... args ) {
        std::lock_guard<std::mutex> lock(mx);
        consolePtr->log(formatStr, args...);
    }
    template< typename... ARGS >
    void trace( const char* formatStr, const ARGS& ... args ) {
        std::lock_guard<std::mutex> lock(mx);
        consolePtr->trace(formatStr, args...);
    }
    template< typename... ARGS >
    void warn( const char* formatStr, const ARGS& ... args ) {
        std::lock_guard<std::mutex> lock(mx);
        consolePtr->warn(formatStr, args...);
    }
    //} NODE.JS COMPATIBILITY HELPERS
#endif

private:
    void _ensureInit() {
        if(!consolePtr)
            consolePtr = new DefaultConsole();
    }
};

extern InfraConsoleWrapper infraConsole;

class NodeConsole : public Console {
public:
    void formattedWrite( WRITELEVEL lvl, const char* s ) override {
        infraConsole.formattedWrite( lvl, s );
    }
};

class NodeConsoleWrapper
{
    //per-object Console wrapper
    std::unique_ptr<Console> consolePtr;

public:
    NodeConsoleWrapper() {
        consolePtr = std::unique_ptr<Console>( new NodeConsole() );
    }
    void assignNewConsole( std::unique_ptr<Console> newConsole ) {
        consolePtr = std::move( newConsole );
    }

    int traceLevel()
    {
        return consolePtr->traceLevel();
    }

    template< typename... ARGS >
    void write( Console::WRITELEVEL lvl, const char* formatStr, const ARGS& ... args )
    {
        consolePtr->write( lvl, formatStr, args... );
    }

    NodeConsoleWrapper( const NodeConsoleWrapper& ) = delete;
    NodeConsoleWrapper& operator =( const NodeConsoleWrapper& ) = delete;
    ~NodeConsoleWrapper() {
    }

    Console::TimeLabel timeWithLabel() {
        return consolePtr->timeWithLabel();
    }
    void timeEnd(Console::TimeLabel label, const char* text) {
        consolePtr->timeEnd(label, text);
    }

#ifndef ASTRIP_NODEJS_COMPAT
    //{ NODE.JS COMPATIBILITY HELPERS
    void time(const char* label) {
        consolePtr->time(label);
    }
    void timeEnd(const char* label) {
        consolePtr->timeEnd(label);
    }

    template< typename... ARGS >
    void error( const char* formatStr, const ARGS& ... args ) {
        consolePtr->error(formatStr, args...);
    }
    template< typename... ARGS >
    void info( const char* formatStr, const ARGS& ... args ) {
        consolePtr->info(formatStr, args...);
    }
    template< typename... ARGS >
    void log( const char* formatStr, const ARGS& ... args ) {
        consolePtr->log(formatStr, args...);
    }
    template< typename... ARGS >
    void trace( const char* formatStr, const ARGS& ... args ) {
        consolePtr->trace(formatStr, args...);
    }
    template< typename... ARGS >
    void warn( const char* formatStr, const ARGS& ... args ) {
        consolePtr->warn(formatStr, args...);
    }
    //} NODE.JS COMPATIBILITY HELPERS
#endif
};
}


#if ( ATRACE_LVL_MAX >= 4 )
#define ATRACE4(...)\
do {\
	if(infraConsole.traceLevel()>=4)\
		autom::infraConsole.write(autom::Console::TRACE,__VA_ARGS__);\
	} while(0)
#else
#define ATRACE4(...)
#endif

#if ( ATRACE_LVL_MAX >= 3 )
#define ATRACE3(...)\
do {\
	if(infraConsole.traceLevel()>=3)\
		autom::infraConsole.write(autom::Console::TRACE,__VA_ARGS__);\
	} while(0)
#else
#define ATRACE3(...)
#endif

#if ( ATRACE_LVL_MAX >= 2 )
#define ATRACE2(...)\
do {\
	if(infraConsole.traceLevel()>=2)\
		autom::infraConsole.write(autom::Console::TRACE,__VA_ARGS__);\
	} while(0)
#else
#define ATRACE2(...)
#endif

#if ( ATRACE_LVL_MAX >= 1 )
#define ATRACE1(...)\
do {\
	if(infraConsole.traceLevel()>=1)\
		autom::infraConsole.write(autom::Console::TRACE,__VA_ARGS__);\
	} while(0)
#else
#define ATRACE1(...)
#endif

#define ATRACE0(...)\
do {\
	if(infraConsole.traceLevel()>=0)\
		autom::infraConsole.write(autom::Console::TRACE,__VA_ARGS__);\
	} while(0)

#endif
