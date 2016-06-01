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

#ifndef CCODE_H
#define CCODE_H

#include "aassert.h"
#include "future.h"
#include "../libsrc/infra/infraconsole.h"
#include "timer.h"

namespace autom {

using StepFunction = std::function< void( void ) >;
using ExHandlerFunction = std::function< void( const std::exception& ) >;


class AStep {
    bool stepReady;

  public:
    enum { NONE = ' ', WAIT = 'w', EXEC = 'e', COND = 'c' };
    char debugOpCode;
    InfraFutureBase* infraPtr;
    FutureFunction fn;
    ExHandlerFunction exHandler;
    AStep* next;

    AStep() {
        debugOpCode = NONE;
        infraPtr = nullptr;
        next = nullptr;
        stepReady = false;
    }
    explicit AStep( FutureFunction fn_ ) {
        debugOpCode = EXEC;
        infraPtr = nullptr;
        next = nullptr;
        fn = fn_;
        stepReady = false;
    }
    AStep( AStep&& other );
    AStep( const AStep& other ) = default;
    ~AStep() = default;

    AStep* endOfChain() {
        auto p = this;
        while( p->next )
            p = p->next;
        return p;
    }
    void setStepReady() {
        stepReady = true;
    }
    bool isStepReady() const {
        return stepReady;
    }
    void debugDump( const char* prefix ) const {
        INFRATRACE4( "{} {} '{}' infra->{} next->{}", prefix, ( void* )this, debugOpCode, ( void* )infraPtr, ( void* )next );
    }
    void debugDumpChain( const char* prefix ) const {
        debugDump( prefix );
        if( next )
            next->debugDumpChain( "" );
    }
};

class CStep {
  public:
    AStep* step;

    CStep() : step( nullptr ) {}
    CStep( AStep* p ) : step( p ) {}
    CStep( StepFunction fn ) {
        step = new AStep( [ = ]( const std::exception* ) {
            fn();
        } );
    }
    CStep( const CStep& ) = default;
    CStep( CStep&& ) = default;
    CStep& operator=( CStep&& ) = default;

    CStep ccatch( ExHandlerFunction handler ) {
        for( auto it = this->step; it; it = it->next )
            it->exHandler = handler;
        return *this;
    }

    static CStep chain( StepFunction fn ) {
        CStep s( fn );
        s.step->debugDump( "chain 1" );
        return s;
    }
    static CStep chain( CStep s ) {
        s.step->debugDump( "chain 2" );
        return s;
    }
    template< typename... Ts >
    static CStep chain( StepFunction fn, Ts&&... Vals ) {
        CStep s( fn );
        s.step->next = chain( Vals... ).step;
        s.step->debugDump( "chain 2" );
        return s;
    }
    template< typename... Ts >
    static CStep chain( CStep s, Ts&&... Vals ) {
        AASSERT4( nullptr == s.step->next );
        s.step->next = chain( Vals... ).step;
        return s;
    }
};

class CIfStep : public CStep {
  public:
    CIfStep() {}
    CIfStep( AStep* p ) : CStep( p ) {}
    CIfStep( StepFunction fn ) : CStep( fn ) {}
    CIfStep( const CIfStep& ) = default;
    CIfStep( CIfStep&& ) = default;
    CIfStep& operator=( CIfStep&& ) = default;

  private:
    void infraEelseImpl( AStep* second );

  public:
    CStep eelse( StepFunction fn ) {
        CStep s( fn );
        infraEelseImpl( s.step );
        return *this;
    }
    CStep eelse( CStep s ) {
        AASSERT4( step->debugOpCode == AStep::COND );
        infraEelseImpl( s.step );
        return *this;
    }
    template< typename... Ts >
    CStep eelse( StepFunction fn, Ts... Vals ) {
        CStep s( fn );
        s.step->next = chain( Vals... ).step;
        infraEelseImpl( s.step );
        return *this;
    }
    template< typename... Ts >
    CStep eelse( CStep s, Ts... Vals ) {
        AASSERT4( step->debugOpCode == AStep::COND );
        s.step->next = chain( Vals... ).step;
        infraEelseImpl( s.step );
        return *this;
    }
};

class CCode {
  public:
    CCode( const CStep& s ) {
        s.step->debugDumpChain( "main\n" );
        exec( s.step );
    }

    static void exec( AStep* s );
    static void deleteChain( AStep* s );

    static CStep ttry( CStep s ) {
        s.step->debugDump( "ttry 1" );
        return s;
    }
    static CStep ttry( StepFunction fn ) {
        CStep s( fn );
        s.step->debugDump( "ttry 2" );
        return s;
    }
    template< typename... Ts >
    static CStep ttry( CStep s, Ts&&... Vals ) {
        s.step->next = ttry( Vals... ).step;
        s.step->debugDump( "ttry 3" );
        return s;
    }
    template< typename... Ts >
    static CStep ttry( StepFunction fn, Ts&&... Vals ) {
        CStep s = ttry( fn );
        s.step->next = ttry( Vals... ).step;
        s.step->debugDump( "ttry 4" );
        return s;
    }
    static CStep waitFor( const FutureBase& future );

  private:
    static CIfStep infraIifImpl( const Future<bool>& b, AStep* c );

  public:
    static CIfStep iif( const Future<bool>& b, StepFunction fn ) {
        CStep s( fn );
        s.step->debugDump( "iif 0" );
        return infraIifImpl( b, s.step );
    }
    static CIfStep iif( const Future<bool>& b, CStep s ) {
        s.step->debugDump( "iif 1" );
        return infraIifImpl( b, s.step );
    }
    template< typename... Ts >
    static CIfStep iif( const Future<bool>& b, StepFunction fn, Ts&&... Vals ) {
        CStep s( fn );
        s.step->next = CStep::chain( Vals... ).step;
        s.step->debugDump( "iif 2" );
        return infraIifImpl( b, s.step );
    }
    template< typename... Ts >
    static CIfStep iif( const Future<bool>& b, CStep s, Ts&&... Vals ) {
        s.step->next = CStep::chain( Vals... ).step;
        s.step->debugDump( "iif 3" );
        return infraIifImpl( b, s.step );
    }
};

}

#endif
