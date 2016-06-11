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
    friend class CStep;
    friend class CIfStep;
    friend class CTryStep;
    friend class CCode;
    friend struct IifFunctor;
    friend struct EelseFunctor;
    friend struct WaitFunctor;
    friend struct WhileFunctor;

    bool stepReady;
    enum { NONE = ' ', WAIT = 'w', EXEC = 'e', COND = 'c', LOOP = 'l' };
    char debugOpCode;
    InfraFutureBase* infraPtr;
    FutureFunction fn;
    ExHandlerFunction exHandler;
    int exId;
    AStep* next;

  public:
    AStep() {
        debugOpCode = NONE;
        infraPtr = nullptr;
        next = nullptr;
        stepReady = false;
    }
    explicit AStep( FutureFunction fn_ ) {
        AASSERT4( fn_ );
        debugOpCode = EXEC;
        infraPtr = nullptr;
        next = nullptr;
        fn = fn_;
        stepReady = false;
    }
    AStep( AStep&& other );
    AStep( const AStep& other ) = default;
    ~AStep() = default;

  private:
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
        INFRATRACE4( "{} {} '{}'{} infra->{} next->{}", prefix, ( void* )this, debugOpCode, exHandler ? " EX" : "", ( void* )infraPtr, ( void* )next );
    }
    void debugDumpChain( const char* prefix ) const {
        debugDump( prefix );
        if( next )
            next->debugDumpChain( "" );
    }
};

class CStep {
    friend class CIfStep;
    friend class CTryStep;
    friend class CCode;

    AStep* step;

  public:
    explicit CStep( AStep* p ) : step( p ) {}
    explicit CStep( StepFunction fn ) {
        step = new AStep( [ = ]( const std::exception* ) {
            fn();
        } );
    }
    CStep( const CStep& ) = default;
    CStep( CStep&& ) = default;
    CStep& operator=( CStep&& ) = default;

  private:
    static CStep chain( StepFunction fn ) {
        return CStep( fn );
    }
    static CStep chain( CStep s ) {
        return s;
    }
    template< typename... Ts >
    static CStep chain( StepFunction fn, Ts&&... Vals ) {
        CStep s( fn );
        s.step->next = chain( Vals... ).step;
        return s;
    }
    template< typename... Ts >
    static CStep chain( CStep s, Ts&&... Vals ) {
        s.step->endOfChain()->next = chain( Vals... ).step;
        return s;
    }
};

class CIfStep : public CStep {
  public:
    explicit CIfStep( AStep* p ) : CStep( p ) {}
    explicit CIfStep( StepFunction fn ) : CStep( fn ) {}
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

class CTryStep : public CStep {
  public:
    explicit CTryStep( AStep* p ) : CStep( p ) {}
    explicit CTryStep( CStep s ) : CStep( s ) {}
    explicit CTryStep( StepFunction fn ) : CStep( fn ) {}
    CTryStep( const CTryStep& ) = default;
    CTryStep( CTryStep&& ) = default;
    CTryStep& operator=( CTryStep&& ) = default;

    CTryStep ccatch( ExHandlerFunction handler );
};

class CCode {
  public:
    CCode( const CStep& s ) {
        s.step->debugDumpChain( "main\n" );
        exec( s.step );
    }
    CCode( StepFunction fn ) {
        CStep s( fn );
        s.step->debugDumpChain( "main\n" );
        exec( s.step );
    }
    template< typename... Ts >
    CCode( const CStep& s, Ts... Vals ) {
        s.step->next = CStep::chain( Vals... ).step;
        s.step->debugDumpChain( "main\n" );
        exec( s.step );
    }
    template< typename... Ts >
    CCode( StepFunction fn, Ts... Vals ) {
        CStep s( fn );
        s.step->next = CStep::chain( Vals... ).step;
        s.step->debugDumpChain( "main\n" );
        exec( s.step );
    }

    static void exec( AStep* s );
	static void execLoop( AStep* s );
	static AStep* deleteChain( AStep* s, bool ex );
    static void setExhandlerChain( AStep* s, ExHandlerFunction handler );

    static CTryStep ttry( CStep s ) {
        s.step->debugDump( "ttry 1" );
        return CTryStep( s );
    }
    static CTryStep ttry( StepFunction fn ) {
        CTryStep s( fn );
        s.step->debugDump( "ttry 2" );
        return s;
    }
    template< typename... Ts >
    static CTryStep ttry( CStep s, Ts&&... Vals ) {
        s.step->next = ttry( Vals... ).step;
        s.step->debugDump( "ttry 3" );
        return CTryStep( s );
    }
    template< typename... Ts >
    static CTryStep ttry( StepFunction fn, Ts&&... Vals ) {
        CTryStep s = ttry( fn );
        s.step->next = ttry( Vals... ).step;
        s.step->debugDump( "ttry 4" );
        return s;
    }
    static CStep waitFor( const FutureBase& future );

  private:
    static CIfStep infraIifImpl( const Future<bool>& b, AStep* c );
    static CStep infraWhileImpl( const Future<bool>& b, AStep* c );

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

    static CStep wwhile( const Future<bool>& b, StepFunction fn ) {
        CStep s( fn );
        s.step->debugDump( "while 0" );
        return infraWhileImpl( b, s.step );
    }
    static CStep wwhile( const Future<bool>& b, CStep s ) {
        s.step->debugDump( "while 1" );
        return infraWhileImpl( b, s.step );
    }
    template< typename... Ts >
    static CStep wwhile( const Future<bool>& b, StepFunction fn, Ts&&... Vals ) {
        CStep s( fn );
        s.step->next = CStep::chain( Vals... ).step;
        s.step->debugDump( "while 2" );
        return infraWhileImpl( b, s.step );
    }
    template< typename... Ts >
    static CStep wwhile( const Future<bool>& b, CStep s, Ts&&... Vals ) {
        s.step->next = CStep::chain( Vals... ).step;
        s.step->debugDump( "while 3" );
        return infraWhileImpl( b, s.step );
    }
};

}

#define CCODE CCode code([=]()
#define ENDCCODE });
#define TTRY },CCode::ttry([=]()
//NB: no starting } for CCATCH, as it ALWAYS comes after '}'
#define CCATCH(a) ).CTryStep::ccatch([=](a)
#define ENDTTRY ),[=](){
#define AWAIT(a) },CCode::waitFor(a),[=](){
#define IIF(a) },CCode::iif(a,[=]()
//NB: no starting } for EELSE and for ENDIIF, as they ALWAYS come after '}'
#define EELSE ).eelse([=]()
#define ENDIIF ),[=](){
#define WWHILE(a) },CCode::wwhile(a,[=]()
#define ENDWWHILE ),[=](){

#endif
