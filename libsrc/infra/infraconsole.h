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

#ifndef INFRACONSOLE_H_INCLUDED
#define INFRACONSOLE_H_INCLUDED

class InfraConsoleWrapper
{
    Console* consolePtr = nullptr;
    //we're NOT using std::unique_ptr<> here
    //  to guarantee that for a global InfraConsoleWrapper
    //  consolePtr is set before ANY global object constructor is called
    int prevConsolesMessages = 0;//as in 'messages to previous consoles'
    bool forever = false;
    std::mutex mx;

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
            delete consolePtr;
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

#endif
