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

#ifndef ANODE_H
#define ANODE_H

#include <mutex>
#include <deque>

#include "abuffer.h"
//#include "aconsole.h"
#include "../libsrc/infra/infraconsole.h"

namespace autom {
#define EV_FILE_DATA_READY 1

class Node;
using FutureFunction = std::function< void( void ) >;
using EventId = unsigned int;

struct NodeQItem {
    EventId id;
    Buffer b;
};

class Future {
    EventId eventId;
    Node* node;

  public:
    Future( Node* node_, EventId id_ ) : node( node_ ), eventId( id_ ) {
    }

    EventId getId() const {
        return eventId;
    }
    void then( FutureFunction fn );
};

class Node {
    using EventMap = std::unordered_map< EventId, FutureFunction >;
    EventMap eventMap;

    NodeQItem currentData;

  public:
    std::recursive_mutex queueMutex;
    std::deque<NodeQItem> nodeQueue;

    void registerFuture( const Future& future, FutureFunction fn ) {
        eventMap.insert( EventMap::value_type( future.getId(), fn ) );
    }
    const Buffer& value() const {
        return currentData.b;
    }
    void infraProcessEvent() {
        for( ;; ) {
            std::lock_guard<std::recursive_mutex> lock( queueMutex );
            if( nodeQueue.size() ) {
                currentData.b.move( nodeQueue.front().b );
                currentData.id = nodeQueue.front().id;
                nodeQueue.pop_front();
                break;
            }
            _sleep( 0 );
        }
        auto it = eventMap.find( currentData.id );
        if( it != eventMap.end() ) {
            it->second();
        }
    }

    virtual void run() = 0;
};

void Future::then( FutureFunction fn ) {
	node->registerFuture( *this, fn );
}

namespace fs {

void sampleAsyncEvent( Node* node ) {
    _sleep( 10000 );
    NodeQItem item;
    item.id = EV_FILE_DATA_READY;
    item.b = Buffer( "Hello Node!" );
    std::lock_guard<std::recursive_mutex> lock( node->queueMutex );
    node->nodeQueue.push_back( item );
}

Future readFile( Node* node, const char* path ) {
    std::thread th( sampleAsyncEvent, node );
    th.detach();
    Future future( node, EV_FILE_DATA_READY );
    return future;
}
}

class NodeOne : public Node {
  public:
    void run() override {
        Future data = fs::readFile( this, "some-file-path" );
        data.then( [ = ]() {
            infraConsole.log( "READ1: {}", value().toString() );
        } );
    }
};

class NodeTwo : public Node {
  public:
    void run() override {
        Future data = fs::readFile( this, "some-another-path" );
        data.then( [ = ]() {
            infraConsole.log( "READ2: {}", value().toString() );
        } );
    }
};
}

#endif
