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
    node->addRef( futureId );
}

Future::Future( const Future& other ) :
    futureId( other.futureId ), node( other.node ) {
    node->addRef( futureId );
}

Future::Future( Future&& other ) :
    futureId( other.futureId ), node( other.node ) {
    node->addRef( futureId );
}

Future::~Future() {
    node->remove( futureId );
}

void Future::then( FutureFunction fn ) {
    node->registerCallback( getId(), fn );
}

const Buffer& Future::value() const {
    return node->findResult( futureId );
}

void Node::infraProcessEvent( const NodeQItem& item ) {
    auto it = futureMap.find( item.id );
    if( it != futureMap.end() ) {
        it->second.result = item.b;
        it->second.fn();
		FutureFunction fn = it->second.fn;
		it->second.fn = FutureFunction();
    }
}

const Buffer& Node::findResult( FutureId id ) const {
    auto it = futureMap.find( id );
    if( it != futureMap.end() )
        return it->second.result;
    AASSERT0( 0 );
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

void FS::sampleAsyncEvent( Node* node, FutureId id ) {
    _sleep( 10000 );
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


