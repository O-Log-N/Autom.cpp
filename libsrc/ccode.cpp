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
			CCode::deleteChain( c1, false );
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
		CCode::deleteChain( passive, false );
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
        head->fn = *this;
    };
    void operator() ( const std::exception * ex ) {
		AASSERT4( head->debugOpCode == AStep::LOOP );
		while( b.value() ) {
			CCode::execLoop( c1 );
        }
		CCode::deleteChain( c1, false );
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
                a = CCode::deleteChain( a, true );
                CCode::exec( a );
            } else {
                CCode::deleteChain( a, false );
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
            auto iif = s->fn.target< IifFunctor >();
            if( iif ) {
                setExhandlerChain( iif->c1, handler );
            } else {
                auto eelse = s->fn.target< EelseFunctor >();
                if( eelse ) {
                    setExhandlerChain( eelse->c1, handler );
                    setExhandlerChain( eelse->c2, handler );
                }
            }
        }
        s = s->next;
    }
}

AStep* CCode::deleteChain( AStep* s, bool ex ) {
    int exId = s ? s->exId : 0;
    while( s ) {
        if( ex )
            if( !s->exHandler || ( s->exId != exId ) )
                return s;
        if( s->infraPtr ) {
            AASSERT4( AStep::WAIT == s->debugOpCode );
            AASSERT4( s->infraPtr->refCount > 0 );
            if( !s->infraPtr->isDataReady() )
                s->infraPtr->cleanup();
            s->infraPtr->refCount--;
        } else {
            AASSERT4( ( AStep::EXEC == s->debugOpCode ) || ( AStep::COND == s->debugOpCode ) );
            auto iif = s->fn.target< IifFunctor >();
            if( iif ) {
                deleteChain( iif->c1, false );
            } else {
                auto eelse = s->fn.target< EelseFunctor >();
                if( eelse ) {
                    deleteChain( eelse->c1, false );
                    deleteChain( eelse->c2, false );
                }
            }
        }

        auto tmp = s;
        s = s->next;
        tmp->debugDump( "    deleting after exception" );
        delete tmp;
    }
    return nullptr;
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
                    s = deleteChain( s, true );
                    continue;
                } else {
                    deleteChain( s, false );
                    return;
                }
            }
        }

        auto tmp = s;
        s = s->next;
        tmp->debugDump( "    deleting main" );
        delete tmp;
    }
}

void CCode::execLoop( AStep* s ) {
	while( s ) {
		if( s->infraPtr ) {
			AASSERT4( AStep::WAIT == s->debugOpCode );
			AASSERT4( s->infraPtr->refCount > 0 );
			if( s->infraPtr->isDataReady() ) {
				s->infraPtr->refCount--;
				INFRATRACE4( "Processing event {} cnt {} ...", (void*)s->infraPtr, s->infraPtr->refCount );
			} else {
				INFRATRACE4( "Waiting event {} cnt {} ...", (void*)s->infraPtr, s->infraPtr->refCount );
				s->setStepReady();
				return;
			}
		} else {
			AASSERT4( ( AStep::EXEC == s->debugOpCode ) || ( AStep::COND == s->debugOpCode ) || ( AStep::LOOP == s->debugOpCode ) );
			try {
				s->fn( nullptr );
			}
			catch( const std::exception& x ) {
				INFRATRACE4( "exception: {}", x.what() );
				return;
			}
		}
		s = s->next;
	}
}

}