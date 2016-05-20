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

class AStep {
  public:
    enum { NONE = ' ', WAIT = 'w', EXEC = 'e', COND = 'c' };
    char debugOpCode;
    InfraFutureBase* infraPtr;
    std::function< void( const std::exception* ) > fn;
    AStep* next;

    AStep() {
        debugOpCode = NONE;
        infraPtr = nullptr;
        next = nullptr;
    }
    explicit AStep( std::function< void( const std::exception* ) > fn_ ) {
        debugOpCode = EXEC;
        infraPtr = nullptr;
        fn = fn_;
        next = nullptr;
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
    void debugPrint( const char* prefix ) const {
        INFRATRACE0( "{} {} '{}' infra->{} next->{}", prefix, ( void* )this, debugOpCode, ( void* )infraPtr, ( void* )next );
    }
    void debugPrintChain( const char* prefix ) const {
        debugPrint( prefix );
        if( next )
            next->debugPrintChain( "" );
    }
};

class CStep {
  public:
    AStep* step;

    CStep() : step( nullptr ) {}
    CStep( AStep* p ) : step( p ) {}
    CStep( std::function< void( void ) > fn ) {
        step = new AStep( [ = ]( const std::exception* ) {
            fn();
        } );
    }
    CStep( const CStep& ) = default;
    CStep( CStep&& ) = default;
    CStep& operator=( CStep&& ) = default;

    CStep ccatch( std::function< void( const std::exception* ) > fn ) {
        return *this;
    }

    static CStep chain( std::function< void( void ) > fn ) {
        CStep s( fn );
        s.step->debugPrint( "chain 1" );
        return s;
    }
    static CStep chain( CStep s ) {
        s.step->debugPrint( "chain 2" );
        return s;
    }
    template< typename... Ts >
    static CStep chain( std::function< void( void ) > fn, Ts&&... Vals ) {
        CStep s( fn );
        s.step->next = chain( Vals... ).step;
        s.step->debugPrint( "chain 2" );
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
    AStep* branch;
    const Future<bool>* condition;

    CIfStep() : branch( nullptr ), condition( nullptr ) {}
    CIfStep( AStep* p ) : branch( nullptr ), condition( nullptr ), CStep( p ) {}
    CIfStep( std::function< void( void ) > fn ) : condition( nullptr ), branch( nullptr ), CStep( fn ) {}
    CIfStep( const CIfStep& ) = default;
    CIfStep( CIfStep&& ) = default;
    CIfStep& operator=( CIfStep&& ) = default;

  private:
    void infraEelseImpl( CIfStep* first, AStep* second );

  public:
    CStep eelse( std::function< void( void ) > fn ) {
        CStep s( fn );
        infraEelseImpl( this, s.step );
        return *this;
    }
    CStep eelse( CStep s ) {
        AASSERT4( step->debugOpCode == AStep::COND );
        infraEelseImpl( this, s.step );
        return *this;
    }
    template< typename... Ts >
    CStep eelse( std::function< void( void ) > fn, Ts... Vals ) {
        CStep s( fn );
        s.step->next = chain( Vals... ).step;
        infraEelseImpl( this, s.step );
        return *this;
    }
    template< typename... Ts >
    CStep eelse( CStep s, Ts... Vals ) {
        AASSERT4( step->debugOpCode == AStep::COND );
        s.step->next = chain( Vals... ).step;
        infraEelseImpl( this, s.step );
        return *this;
    }
};

class CCode {
  public:
    CCode( const CStep& s ) {
        s.step->debugPrintChain( "main\n" );
        exec( s.step );
    }

    static void exec( const AStep* s );

    static CStep ttry( CStep s ) {
        s.step->debugPrint( "ttry 1" );
        return s;
    }
    static CStep ttry( std::function< void( void ) > fn ) {
        CStep s( fn );
        s.step->debugPrint( "ttry 2" );
        return s;
    }
    template< typename... Ts >
    static CStep ttry( CStep s, Ts&&... Vals ) {
        s.step->next = ttry( Vals... ).step;
        s.step->debugPrint( "ttry 3" );
        return s;
    }
    template< typename... Ts >
    static CStep ttry( std::function< void( void ) > fn, Ts&&... Vals ) {
        CStep s = ttry( fn );
        s.step->next = ttry( Vals... ).step;
        s.step->debugPrint( "ttry 4" );
        return s;
    }
    static CStep waitFor( const Future<Timer>& future );

  private:
    static CIfStep infraIifImpl( const Future<bool>& b, AStep* c );

  public:
    template< typename... Ts >
    static CIfStep iif( const Future<bool>& b, std::function< void( void ) > fn, Ts&&... Vals ) {
        CStep s( fn );
        s.step->next = CStep::chain( Vals... ).step;
        s.step->debugPrint( "iif 1" );
        return infraIifImpl( b, s.step );
    }
    template< typename... Ts >
    static CIfStep iif( const Future<bool>& b, CStep s, Ts&&... Vals ) {
        s.step->next = CStep::chain( Vals... ).step;
        s.step->debugPrint( "iif 2" );
        return infraIifImpl( b, s.step );
    }
};

}

#endif
