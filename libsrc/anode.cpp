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

#include "../include/anode.h"
#include "../include/aassert.h"
#include "../include/net.h"
#include "../include/timer.h"
#include "infra/nodecontainer.h"

using namespace autom;

void Node::infraProcessTimer( const NodeQTimer& item ) {
    auto it = futureMap.find( item.id );
    if( it != futureMap.end() ) {
        it->second->fn();
        it->second->cleanup();
        futureCleanup();
    }
}

void Node::infraProcessTcpAccept( const NodeQAccept& item ) {
    auto it = futureMap.find( item.id );
    if( it != futureMap.end() ) {
        auto f = static_cast<InfraFuture< TcpServerConn >*>( it->second.get() );
        f->getResult().stream = ( uv_stream_t * )item.stream;
        it->second->fn();
        it->second->cleanup();
        futureCleanup();
    }
}

void Node::infraProcessTcpRead( const NodeQBuffer& item ) {
    auto it = futureMap.find( item.id );
    if( it != futureMap.end() ) {
        auto f = static_cast<InfraFuture< Buffer >*>( it->second.get() );
        f->getResult().fromNetwork( item.b );
        it->second->fn();
        it->second->cleanup();
        futureCleanup();
    }
}

void Node::infraProcessTcpClosed( const NodeQBuffer& item ) {
    auto it = futureMap.find( item.closeId );
    if( it != futureMap.end() ) {
        it->second->fn();
        it->second->cleanup();
        futureCleanup();
    }
}

void Node::infraProcessTcpConnect( const NodeQConnect& item ) {
    auto it = futureMap.find( item.id );
    if( it != futureMap.end() ) {
        auto f = static_cast<InfraFuture< TcpClientConn >*>( it->second.get() );
        f->getResult().stream = item.stream;
        it->second->fn();
        it->second->cleanup();
        futureCleanup();
    }
}

InfraFutureBase* Node::insertInfraFuture( FutureId id, InfraFutureBase* inf ) {
    auto p = futureMap.insert( FutureMap::value_type( id, std::unique_ptr<InfraFutureBase>( inf ) ) );
    AASSERT4( p.second, "Duplicated FutureId" );
    return p.first->second.get();
}

InfraFutureBase* Node::findInfraFuture( FutureId id ) {
    auto it = futureMap.find( id );
    if( it != futureMap.end() )
        return it->second.get();
    return nullptr;
}

void Node::futureCleanup() {
    for( auto it = futureMap.begin(); it != futureMap.end(); ) {
        AASSERT4( it->second->refCount >= 0 );
        if( ( !it->second->multi ) && ( it->second->refCount <= 0 ) ) {
            it = futureMap.erase( it );
        } else {
            ++it;
        }
    }
}

bool Node::isEmpty() const {
    for( auto& it : futureMap ) {
        if( ( !it.second->multi ) && it.second->refCount )
            return false;
    }
    return true;
}

void Node::debugDump() const {
    INFRATRACE0( "futures {}", futureMap.size() );
    for( auto& it : futureMap ) {
        INFRATRACE0( "  #{}", it.first );
        it.second->debugDump();
    }
}

