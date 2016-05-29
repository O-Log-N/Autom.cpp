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

#include "../include/ccode.h"

namespace autom {

struct IifFunctor {
    const Future<bool> b;
    AStep* head;
    AStep* c1;
    AStep* e1;

    explicit IifFunctor( const Future<bool>& b_ ) : b( b_ ) {};
    void operator() ( const std::exception * ex ) {
        if( b.value() ) {
            INFRATRACE4( "====IF POSITIVE====" );
            // insert active branch in execution chain
            auto tmp = head->next;
            head->next = c1;
            e1->next = tmp;
            head->debugDumpChain( "iif new exec chain:" );
        } else {
            INFRATRACE4( "====IF NEGATIVE====" );
            AStep* passive = c1;
            while( passive ) {
                auto tmp = passive;
                passive = passive->next;
                if( tmp->infraPtr ) {
                    AASSERT4( tmp->debugOpCode == 'w' );
                    AASSERT4( tmp->infraPtr->refCount > 1 );
                    tmp->infraPtr->refCount--;
                    tmp->infraPtr->cleanup();
                }
                tmp->debugDump( "    deleting iif branch:" );
                delete tmp;
            }
        }
    }
};

struct EelseFunctor {
    const Future<bool> b;
    AStep* head;
    AStep* c1;
    AStep* c2;
    AStep* e1;
    AStep* e2;

    explicit EelseFunctor( const Future<bool>& b_ ) : b( b_ ) {}
    void operator() ( const std::exception* ) {
        // insert active branch into exec list
        AStep* active, *passive, *end;
        if( b.value() ) {
            INFRATRACE4( "====IFELSE POSITIVE====" );
            active = c1;
            passive = c2;
            end = e1;
        } else {
            INFRATRACE4( "====IFELSE NEGATIVE====" );
            active = c2;
            passive = c1;
            end = e2;
        }
        // insert active branch in execution chain
        auto tmp = head->next;
        head->next = active;
        end->next = tmp;
        // delete passive branch
        while( passive ) {
            tmp = passive;
            passive = passive->next;
            if( tmp->infraPtr ) {
                AASSERT4( tmp->debugOpCode == 'w' );
                AASSERT4( tmp->infraPtr->refCount > 1 );
                tmp->infraPtr->refCount--;
                tmp->infraPtr->cleanup();
            }
            tmp->debugDump( "    deleting passive branch:" );
            delete tmp;
        }
    }
};

void CIfStep::infraEelseImpl( AStep* c2 ) {
    AASSERT4( this );
    AASSERT4( step );
    AASSERT4( !step->next );
    AASSERT4( step->debugOpCode == AStep::COND );
    AASSERT4( c2 );

    IifFunctor* iif = step->fn.target< IifFunctor >();
    EelseFunctor func( iif->b );
    func.c1 = iif->c1;
    func.c2 = c2;
    func.head = step;
    func.e1 = func.c1->endOfChain();
    func.e2 = c2->endOfChain();
    func.c1->debugDumpChain( "====IFELSE FIRST BRANCH" );
    func.c2->debugDumpChain( "====IFELSE SCOND BRANCH" );

    step->fn = func;
}

CIfStep CCode::infraIifImpl( const Future<bool>& b, AStep* c ) {
    AASSERT4( c );
    c->debugDumpChain( "iifImpl" );

    IifFunctor func( b );
    func.head = new AStep;
    func.head->debugOpCode = AStep::COND;
    func.c1 = c;
    func.e1 = c->endOfChain();
    func.head->fn = func;

    CIfStep s;
    s.step = func.head;
    return s;
}

CStep CCode::waitFor( const FutureBase& future ) {
    AStep* a = new AStep;
    a->debugOpCode = AStep::WAIT;
    a->infraPtr = future.infraGetPtr();
    a->infraPtr->refCount++;
    future.then( [a]( const std::exception * ex ) {
        AASSERT4( a->infraPtr->isDataReady() );
        if( a->isStepReady() ) {
            a->debugDump( "callback" );
            exec( a );
        }
    } );
    CStep s;
    s.step = a;
    s.step->debugDumpChain( "waitFor" );
    return s;
}

void CCode::exec( AStep* s ) {
    while( s ) {
        if( s->infraPtr ) {
            AASSERT4( AStep::WAIT == s->debugOpCode );
            AASSERT4( s->infraPtr->refCount > 0 );
            if( s->infraPtr->isDataReady() ) {
                s->infraPtr->refCount--;
                INFRATRACE4( "Processing event {} cnt {} ...", ( void* )s->infraPtr, s->infraPtr->refCount );
            } else {
                INFRATRACE4( "Waiting event {} cnt {} ...", ( void* )s->infraPtr, s->infraPtr->refCount );
                s->setStepReady();
                return;
            }
        } else {
            AASSERT4( ( AStep::EXEC == s->debugOpCode ) || ( AStep::COND == s->debugOpCode ) );
            try {
                s->fn( nullptr );
            } catch( const std::exception& x ) {
                s->exHandler( x );
            }
        }

        auto tmp = s;
        s = s->next;
        tmp->debugDump( "    deleting main" );
        delete tmp;
    }
}

}