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

using namespace autom;

Future::Future( Node* node_ ) : node( node_ ) {
    futureId = node->nextFutureId();
    infraPtr = &( node->insertInfraFuture( futureId ) );
}

Future::Future( const Future& other ) :
    futureId( other.futureId ), node( other.node ), infraPtr( other.infraPtr ) {
    AASSERT4( infraPtr == node->findInfraFuture( futureId ) );
    infraPtr->refCount++;
}

Future& Future::operator=( const Future& other ) {
    futureId = other.futureId;
    node = other.node;
    infraPtr = other.infraPtr;
    AASSERT4( infraPtr == node->findInfraFuture( futureId ) );
    infraPtr->refCount++;
    return *this;
}

Future::Future( Future&& other ) :
    futureId( other.futureId ), node( other.node ), infraPtr( other.infraPtr ) {
    AASSERT4( infraPtr == node->findInfraFuture( futureId ) );
    infraPtr->refCount++;
}

Future::~Future() {
    infraPtr->refCount--;
}

void Future::then( const FutureFunction& fn ) {
    AASSERT4( infraPtr == node->findInfraFuture( futureId ) );
    infraPtr->fn = fn;
}

const Buffer& Future::value() const {
    AASSERT4( infraPtr == node->findInfraFuture( futureId ) );
    return infraPtr->result;
}

void Node::infraProcessEvent( const NodeQItem& item ) {
    auto it = futureMap.find( item.id );
    if( it != futureMap.end() ) {
        it->second.result = item.b;
        it->second.fn();
        it->second.fn.~FutureFunction();
        futureCleanup();
    }
}

void FS::run() {
    NodeQItem item;
    while( true ) {
        while( inputQueue.pop( item ) ) {
            if( nodes.end() != nodes.find( item.node ) )
                if( item.node->parentFS == this )
                    item.node->infraProcessEvent( item );
        }
        debugDump( __LINE__ );
        inputQueue.wait();
    }
}

#include <chrono>
#include <thread>

void FS::sampleAsyncEvent( Node* node, FutureId id ) {
    std::this_thread::sleep_for( std::chrono::seconds( 10 ) );
    std::string s( fmt::format( "Hello Future {}!", id ) );
    NodeQItem item;
    item.id = id;
    item.b = Buffer( s.c_str() );
    item.node = node;
    node->parentFS->pushEvent( item );
}

Future FS::readFile( Node* node, const char* path ) {
    Future future( node );
    std::thread th( sampleAsyncEvent, node, future.getId() );
    th.detach();
    return future;
}


