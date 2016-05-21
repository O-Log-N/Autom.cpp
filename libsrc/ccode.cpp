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

void CIfStep::infraEelseImpl( AStep* second ) {
    AASSERT4( this );
    AASSERT4( step );
    AASSERT4( !step->next );
    AASSERT4( step->debugOpCode == AStep::COND );
    AASSERT4( condition );
    AASSERT4( branch );
    AASSERT4( second );

    AStep* f = branch;
    AStep* head = step;
    auto e1 = f->endOfChain();
    auto e2 = second->endOfChain();
    const Future<bool>& b = *condition;
    f->debugDumpChain( "====IFELSE FIRST BRANCH" );
    second->debugDumpChain( "====IFELSE SCOND BRANCH" );
    step->fn = [b, f, head, second, e1, e2]( const std::exception * ex ) {
        // insert active branch into exec list
        AStep* active, *passive, *end;
        if( b.value() ) {
            INFRATRACE0( "====IFELSE POSITIVE====" );
            active = f;
            passive = second;
            end = e1;
        } else {
            INFRATRACE0( "====IFELSE NEGATIVE====" );
            active = second;
            passive = f;
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
            if( tmp->infraPtr )
                tmp->infraPtr->cleanup();
            tmp->debugDump( "    deleting passive branch:" );
            delete tmp;
        }
    };
}

CStep CCode::waitFor( const FutureBase& future ) {
    AStep* a = new AStep;
    a->debugOpCode = AStep::WAIT;
    a->infraPtr = future.infraGetPtr();
    CStep s;
    s.step = a;
    future.then( [a]( const std::exception* ) {
        AASSERT4( a->infraPtr->isDataReady() );
        exec( a );
    } );
    s.step->debugDumpChain( "waitFor" );
    return s;
}

CIfStep CCode::infraIifImpl( const Future<bool>& b, AStep* c ) {
    AASSERT4( c );
    AStep* head = new AStep;
    head->debugOpCode = AStep::COND;

    auto end = c->endOfChain();
    c->debugDumpChain( "iifImpl" );

    head->fn = [head, c, end, b]( const std::exception * ex ) {
        // insert active branch into exec list
        if( b.value() ) {
            INFRATRACE0( "====IF POSITIVE====" );
            // insert active branch in execution chain
            auto tmp = head->next;
            head->next = c;
            end->next = tmp;
            head->debugDumpChain( "iif new exec chain:" );
        } else {
            INFRATRACE0( "====IF NEGATIVE====" );
            AStep* passive = c;
            while( passive ) {
                auto tmp = passive;
                passive = passive->next;
                if( tmp->infraPtr )
                    tmp->infraPtr->cleanup();
                tmp->debugDump( "    deleting iif branch:" );
                delete tmp;
            }
        }
    };
    CIfStep s;
    s.step = head;
    s.branch = c;
    s.condition = &b;
    return s;
}

void CCode::exec( const AStep* s ) {
    while( s ) {
        if( s->infraPtr ) {
            AASSERT4( AStep::WAIT == s->debugOpCode );
            AASSERT4( s->infraPtr->refCount > 0 );
            if( s->infraPtr->isDataReady() ) {
                INFRATRACE0( "Processing event {} ...", ( void* )s->infraPtr );
            } else {
                INFRATRACE0( "Waiting event {} ...", ( void* )s->infraPtr );
                s->infraPtr->setStepReady();
                return;
            }
        } else {
            AASSERT4( ( AStep::EXEC == s->debugOpCode ) || ( AStep::COND == s->debugOpCode ) );
            s->fn( nullptr );
        }

        auto tmp = s;
        s = s->next;
        tmp->debugDump( "    deleting main" );
        delete tmp;
    }
}

}