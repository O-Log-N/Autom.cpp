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
#include <condition_variable>
#include <deque>
#include <set>
#include <thread>

#include "abuffer.h"
//#include "aconsole.h"
#include "../libsrc/infra/infraconsole.h"

namespace autom {
#define EV_FILE_DATA_READY 1

class Node;
class FS;
using FutureFunction = std::function< void( void ) >;
using EventId = unsigned int;

struct NodeQItem {
    EventId id;
    Node* node;
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

  public:
    FS* parentFS;
    void registerFuture( const Future& future, FutureFunction fn ) {
        eventMap.insert( EventMap::value_type( future.getId(), fn ) );
    }
    void infraProcessEvent( const NodeQItem& item ) {
        auto it = eventMap.find( item.id );
        if( it != eventMap.end() ) {
            it->second();
        }
    }

    virtual void run() = 0;
};

void Future::then( FutureFunction fn ) {
    node->registerFuture( *this, fn );
}

class FSQ {
    std::deque< NodeQItem > q;
    std::mutex mx;
    std::condition_variable signal;

  public:
    void push( const NodeQItem& src ) {
        std::lock_guard< std::mutex > lock( mx );
        q.push_back( src );
        signal.notify_one();
    }
    bool pop( NodeQItem& dst ) {
        std::lock_guard< std::mutex > lock( mx );
        if( q.size() ) {
            dst = q.front();
            q.pop_front();
            return true;
        }
        return false;
    }
    void wait() {
        std::unique_lock< std::mutex > lock( mx );
        signal.wait( lock );
    }
};

class FS {
    std::set< Node* > nodes;
    FSQ inputQueue;

  public:
    void run() {
        NodeQItem item;
        while( true ) {
            while( inputQueue.pop( item ) ) {
                if( nodes.end() != nodes.find( item.node ) )
                    if( item.node->parentFS == this )
                        item.node->infraProcessEvent( item );
            }
            inputQueue.wait();
        }
    }
    void pushEvent( const NodeQItem& item ) {
        inputQueue.push( item );
    }
    void addNode( Node* node ) {
        nodes.insert( node );
        node->parentFS = this;
        node->run();
    }
    void removeNode( Node* node ) {
		nodes.erase( node );
	}

    static void sampleAsyncEvent( Node* node ) {
        _sleep( 10000 );
        NodeQItem item;
        item.id = EV_FILE_DATA_READY;
        item.b = Buffer( "Hello Node!" );
        item.node = node;
        node->parentFS->pushEvent( item );
    }

    static Future readFile( Node* node, const char* path ) {
        std::thread th( sampleAsyncEvent, node );
        th.detach();
        Future future( node, EV_FILE_DATA_READY );
        return future;
    }
};

class NodeOne : public Node {
  public:
    void run() override {
        Future data = FS::readFile( this, "some-file-path" );
        data.then( [ = ]() {
            infraConsole.log( "READ1: {}", "value().toString()\n" );
			FS::readFile( this, "some-file-path" );
        } );
    }
};

class NodeTwo : public Node {
  public:
    void run() override {
        Future data = FS::readFile( this, "some-another-path" );
        data.then( [ = ]() {
            infraConsole.log( "READ2: {}", "value().toString()\n" );
        } );
    }
};
}

#endif
