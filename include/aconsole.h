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

#include <unordered_map>
#include <chrono> // TODO: still needed for njTimes, aTimes

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

namespace autom {
class Console {
        //Note for Infrastructure-level Developers:
        //  Console as such is thread-agnostic
        //  ALL the thread sync is a responsibility of 'wrappers'
        using TimePoint = std::chrono::high_resolution_clock::time_point;

    public:
        enum WRITELEVEL { //NOT using enum class here to enable shorter console.write(Console::INFO,...);
            TRACE = 0, INFO = 1, NOTICE = 2, WARN = 3, ERROR = 4, CRITICAL = 5, ALERT = 6
            };

        struct TimeLabel {
            size_t idx;
            TimeLabel( size_t u ) : idx( u ) {}
            };

    private:
        static const size_t ATIMENONE = static_cast<size_t>(-1);
        struct PrivateATimeStoredType {
            size_t nextFree = ATIMENONE;
            TimePoint began;
            };

        int softTraceLevel = ATRACE_LVL_DEFAULT;
        int nMessages = 0;//statistics-only; NON-STRICT in MT environments
        size_t firstFreeATime = ATIMENONE;
        std::vector<PrivateATimeStoredType> aTimes;//for Autom-style timeWithLabel()/timeEnd()
        //'sparse' vector with firstFreeATime forming single-linked list
        //in nextFree items
#ifndef ASTRIP_NODEJS_COMPAT
        std::unordered_map<std::string,TimePoint> njTimes;
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

class NodeConsole : public Console {
    public:
        void formattedWrite( WRITELEVEL lvl, const char* s ) override;
    };

class NodeConsoleWrapper {
        //per-object Console wrapper
        std::unique_ptr<Console> consolePtr;

    public:
        NodeConsoleWrapper() {
            consolePtr = std::unique_ptr<Console>( new NodeConsole() );
            }
        void assignNewConsole( std::unique_ptr<Console> newConsole ) {
            consolePtr = std::move( newConsole );
            }

        int traceLevel() {
            return consolePtr->traceLevel();
            }

        template< typename... ARGS >
        void write( Console::WRITELEVEL lvl, const char* formatStr, const ARGS& ... args ) {
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
#define TRACE4(...)\
do {\
	if(console.traceLevel()>=4)\
		console.write(autom::Console::TRACE,__VA_ARGS__);\
	} while(0)
#else
#define ATRACE4(...) ((void)0)
#endif

#if ( ATRACE_LVL_MAX >= 3 )
#define ATRACE3(...)\
do {\
	if(console.traceLevel()>=3)\
		console.write(autom::Console::TRACE,__VA_ARGS__);\
	} while(0)
#else
#define ATRACE3(...) ((void)0)
#endif

#if ( ATRACE_LVL_MAX >= 2 )
#define ATRACE2(...)\
do {\
	if(console.traceLevel()>=2)\
		console.write(autom::Console::TRACE,__VA_ARGS__);\
	} while(0)
#else
#define ATRACE2(...) ((void)0)
#endif

#if ( ATRACE_LVL_MAX >= 1 )
#define ATRACE1(...)\
do {\
	if(console.traceLevel()>=1)\
		console.write(autom::Console::TRACE,__VA_ARGS__);\
	} while(0)
#else
#define ATRACE1(...) ((void)0)
#endif

#if ( ATRACE_LVL_MAX >= 0 )
#define ATRACE0(...)\
do {\
	if(console.traceLevel()>=0)\
		console.write(autom::Console::TRACE,__VA_ARGS__);\
	} while(0)
#else
#define ATRACE0(...) ((void)0)
#endif

#endif
