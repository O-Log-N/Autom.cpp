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

CTryStep CTryStep::ccatch( ExHandlerFunction handler ) {
    CCode::setExhandlerChain( step, handler );
    return *this;
}

struct IifFunctor {
    const Future<bool> b;
    AStep* head;
    AStep* c1;
    AStep* e1;

    explicit IifFunctor( const Future<bool>& b_, AStep* c_ ) : b( b_ ), c1( c_ ) {
        e1 = c1->endOfChain();
        head = new AStep;
        head->debugOpCode = AStep::COND;
        head->fn = *this;
    };
    void operator() ( const std::exception * ex ) {
        AASSERT4( head->debugOpCode == AStep::COND );
        if( b.value() ) {
            // insert active branch in execution chain
            auto tmp = head->next;
            head->next = c1;
            e1->next = tmp;
            head->debugDumpChain( "iif new exec chain:" );
        } else {
            CCode::deleteChain( c1, nullptr );
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
        AASSERT4( head->debugOpCode == AStep::COND );
        // insert active branch into exec list
        AStep* active, *passive, *end;
        if( b.value() ) {
            active = c1;
            passive = c2;
            end = e1;
        } else {
            active = c2;
            passive = c1;
            end = e2;
        }
        // insert active branch in execution chain
        auto tmp = head->next;
        head->next = active;
        end->next = tmp;
        CCode::deleteChain( passive, nullptr );
    }
};

void CIfStep::infraEelseImpl( AStep* c2 ) {
    AASSERT4( this );
    AASSERT4( step );
    AASSERT4( !step->next );
    AASSERT4( step->debugOpCode == AStep::COND );
    AASSERT4( c2 );

    auto iif = step->fn.target< IifFunctor >();

    iif->c1->debugDumpChain( "====IFELSE FIRST BRANCH" );
    c2->debugDumpChain( "====IFELSE SCOND BRANCH" );

    EelseFunctor func( iif->b );
    func.c1 = iif->c1;
    func.c2 = c2;
    func.head = step;
    func.e1 = func.c1->endOfChain();
    func.e2 = c2->endOfChain();

    step->fn = func;
}

CIfStep CCode::infraIifImpl( const Future<bool>& b, AStep* c ) {
    AASSERT4( c );
    c->debugDumpChain( "iifImpl" );
    return CIfStep( IifFunctor( b, c ).head );
}

struct WhileFunctor {
    const Future<bool> b;
    AStep* head;
    AStep* c1;
    AStep* e1;

    explicit WhileFunctor( const Future<bool>& b_, AStep* c_ ) : b( b_ ), c1( c_ ) {
        AASSERT4( c1 );
        e1 = c1->endOfChain();
        head = new AStep;
        head->debugOpCode = AStep::LOOP;
        e1->nextExec = head;
        head->fn = *this;
    };
    void operator() ( const std::exception * ex ) {
        AASSERT4( head->debugOpCode == AStep::LOOP );
        if( !e1->next ) {
            AASSERT4( !e1->next );
            e1->next = head->next;
            head->next = c1;
        }
        if( b.value() ) {
            CCode::addRefChain( c1, e1 );
            head->refCount++;
        } else {
            head->nextExec = e1->next;
            CCode::deleteChain( c1, e1 );
        }
    }
};

CStep CCode::infraWhileImpl( const Future<bool>& b, AStep* c ) {
    AASSERT4( c );
    c->debugDumpChain( "wwhileImpl" );
    return CStep( WhileFunctor( b, c ).head );
}

struct WaitFunctor {
    AStep* a;

    explicit WaitFunctor( AStep* a_ ) : a( a_ ) {}
    void operator() ( const std::exception* ex ) {
        AASSERT4( a->debugOpCode == AStep::WAIT );
        AASSERT4( a->infraPtr );
        if( ex ) {
            if( a->exHandler ) {
                a->exHandler( *ex );
                a = CCode::deleteExChain( a );
                CCode::exec( a );
            } else {
                CCode::deleteChain( a, nullptr );
            }
        } else if( a->isStepReady() ) {
            AASSERT4( a->infraPtr->isDataReady() );
            a->debugDump( "callback" );
            CCode::exec( a );
        }
    }
};

CStep CCode::waitFor( const FutureBase& future ) {
    AStep* a = new AStep;
    a->debugOpCode = AStep::WAIT;
    a->infraPtr = future.infraGetPtr();
    a->infraPtr->refCount++;
    future.then( WaitFunctor( a ) );
    a->debugDumpChain( "waitFor" );

    return CStep( a );
}

void CCode::setExhandlerChain( AStep* s, ExHandlerFunction handler ) {
    static int globalId = 0; // TODO: implement
    int id = ++globalId;
    while( s ) {
        if( !s->exHandler ) {
            s->exHandler = handler;
            s->exId = id;
        }
        if( !s->infraPtr ) {
            AASSERT4( ( AStep::EXEC == s->debugOpCode ) || ( AStep::COND == s->debugOpCode ) || ( AStep::LOOP == s->debugOpCode ) );
            if( auto f = s->fn.target< IifFunctor >() ) {
                setExhandlerChain( f->c1, handler );
            } else if( auto f = s->fn.target< EelseFunctor >() ) {
                setExhandlerChain( f->c1, handler );
                setExhandlerChain( f->c2, handler );
            } else if( auto f = s->fn.target< WhileFunctor >() ) {
                setExhandlerChain( f->c1, handler );
            } else {
                AASSERT4( AStep::EXEC == s->debugOpCode );
            }
        }
        s = s->next;
    }
}

void CCode::deleteChain( AStep* s, const AStep* e ) {
    while( s ) {
        if( s->infraPtr ) {
            AASSERT4( AStep::WAIT == s->debugOpCode );
            AASSERT4( s->infraPtr->refCount > 0 );
            if( !s->infraPtr->isDataReady() )
                s->infraPtr->cleanup();
            s->infraPtr->refCount--;
        } else {
            AASSERT4( ( AStep::EXEC == s->debugOpCode ) || ( AStep::COND == s->debugOpCode ) || ( AStep::LOOP == s->debugOpCode ) );
            if( auto f = s->fn.target< IifFunctor >() ) {
                deleteChain( f->c1, e );
            } else if( auto f = s->fn.target< EelseFunctor >() ) {
                deleteChain( f->c1, e );
                deleteChain( f->c2, e );
            } else if( auto f = s->fn.target< WhileFunctor >() ) {
                deleteChain( f->c1, e );
            } else {
                AASSERT4( AStep::EXEC == s->debugOpCode );
            }
        }

        auto tmp = s;
        s = s->next;
        tmp->debugDump( "    deleting after exception" );
        tmp->refCount--;
        AASSERT4( tmp->refCount >= 0 );
        if( tmp->refCount <= 0 )
            delete tmp;
        if( tmp == e )
            return;
    }
}

AStep* CCode::deleteExChain( AStep* s ) {
    int exId = s ? s->exId : 0;
    while( s ) {
        if( !s->exHandler || ( s->exId < exId ) )
            return s;
        if( s->infraPtr ) {
            AASSERT4( AStep::WAIT == s->debugOpCode );
            AASSERT4( s->infraPtr->refCount > 0 );
            if( !s->infraPtr->isDataReady() )
                s->infraPtr->cleanup();
            s->infraPtr->refCount--;
        } else {
            AASSERT4( ( AStep::EXEC == s->debugOpCode ) || ( AStep::COND == s->debugOpCode ) || ( AStep::LOOP == s->debugOpCode ) );
            if( auto f = s->fn.target< IifFunctor >() ) {
                deleteChain( f->c1, nullptr );
            } else if( auto f = s->fn.target< EelseFunctor >() ) {
                deleteChain( f->c1, nullptr );
                deleteChain( f->c2, nullptr );
            } else if( auto f = s->fn.target< WhileFunctor >() ) {
                deleteChain( f->c1, nullptr );
            } else {
                AASSERT4( AStep::EXEC == s->debugOpCode );
            }
        }

        auto tmp = s;
        s = s->next;
        tmp->debugDump( "    deleting after exception" );
        tmp->refCount--;
        AASSERT4( tmp->refCount >= 0 );
        if( tmp->refCount <= 0 )
            delete tmp;
    }
    return nullptr;
}


void CCode::addRefChain( AStep* s, const AStep* e ) {
    while( s ) {
        AASSERT4( s->refCount > 0 );
        s->refCount++;
        if( !s->infraPtr ) {
            AASSERT4( ( AStep::EXEC == s->debugOpCode ) || ( AStep::COND == s->debugOpCode ) || ( AStep::LOOP == s->debugOpCode ) );
            if( auto f = s->fn.target< IifFunctor >() ) {
                addRefChain( f->c1, e );
            } else if( auto f = s->fn.target< EelseFunctor >() ) {
                addRefChain( f->c1, e );
                addRefChain( f->c2, e );
            } else if( auto f = s->fn.target< WhileFunctor >() ) {
                addRefChain( f->c1, e );
            } else {
                AASSERT4( AStep::EXEC == s->debugOpCode );
            }
        }
        if( s == e )
            break;
        s = s->next;
    }
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
            AASSERT4( ( AStep::EXEC == s->debugOpCode ) || ( AStep::COND == s->debugOpCode ) || ( AStep::LOOP == s->debugOpCode ) );
            try {
                s->fn( nullptr );
            } catch( const std::exception& x ) {
                if( s->exHandler ) {
                    s->exHandler( x );
                    s = deleteExChain( s );
                    continue;
                } else {
                    deleteChain( s, nullptr );
                    return;
                }
            }
        }

        auto tmp = s;
        if( s->nextExec )
            s = s->nextExec;
        else
            s = s->next;
        tmp->debugDump( "    deleting main" );
        tmp->refCount--;
        AASSERT4( tmp->refCount >= 0 );
        if( tmp->refCount <= 0 )
            delete tmp;
    }
}

}