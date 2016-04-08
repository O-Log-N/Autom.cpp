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

void Node::infraProcessEvent( const NodeQItem& item ) {
    auto it = futureMap.find( item.id );
    if( it != futureMap.end() ) {
        it->second->setResult( item.b );
        it->second->fn();

        it->second->fn = FutureFunction();
        //The line above effectively destroys existing lambda it->second.fn
        //  First, we CAN do it, as we don't need second.fn anymore at all
        //  Second, we SHOULD do it, to avoid cyclical references from lambda
        //    to our InfraFutures, which will prevent futureCleanup() from
        //    destroying InfraFuture - EVER

        futureCleanup();
    }
}

bool FS::isEmpty() const {
    for( auto it : nodes )
        if( !it->isEmpty() )
            return false;
    return true;
}

void FS::run() {
    NodeQItem item;
    while( true ) {
        while( inputQueue.pop( item ) ) {
            AASSERT4( nodes.end() != nodes.find( item.node ) );
            AASSERT4( item.node->parentFS == this );
            item.node->infraProcessEvent( item );
        }
        debugDump( __LINE__ );
        if( isEmpty() )
            break;
        inputQueue.wait();
    }
}

#include <chrono>
#include <thread>

void FS::sampleAsyncEvent( Node* node, FutureId id ) {
    std::this_thread::sleep_for( std::chrono::seconds( 2 ) );
    std::string s( fmt::format( "Hello Future {}!", id ) );
    NodeQItem item;
    item.id = id;
    item.b = NetworkBuffer( s.c_str() );
    item.node = node;
    node->parentFS->pushEvent( item );
}

Future< Buffer > FS::readFile( Node* node, const char* path ) {
    Future< Buffer > future( node );
    std::thread th( sampleAsyncEvent, node, future.getId() );
    th.detach();
    return future;
}

