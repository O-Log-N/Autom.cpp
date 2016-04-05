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
#include "aassert.h"

namespace autom {

class Node;
class FS;
using FutureFunction = std::function< void( void ) >;
using FutureId = unsigned int;

struct NodeQItem {
    FutureId id;
    Node* node;
    Buffer b;
};

class Future {
    FutureId futureId;
    Node* node;

  public:
    Future( Node* );
    Future( const Future& );
    Future( Future&& );
    Future& operator=( const Future& ) = delete;
    virtual ~Future();
    void then( FutureFunction );
    FutureId getId() const {
        return futureId;
    }
    const Buffer& value() const;
};

class Node {
    struct InfraFuture {
        FutureFunction fn;
        int refCount;
        Buffer result;
    };
    using FutureMap = std::unordered_map< FutureId, InfraFuture >;
    FutureMap futureMap;
    FutureId nextFutureIdCount = 0;

  public:
    FS* parentFS;
    void registerCallback( FutureId id, const FutureFunction fn ) {
        auto it = futureMap.find( id );
        if( it != futureMap.end() ) {
            it->second.fn = fn;
        }
    }
    void addRef( FutureId id ) {
        auto it = futureMap.find( id );
        if( it == futureMap.end() ) {
            InfraFuture inf;
            inf.refCount = 1;
            futureMap.insert( FutureMap::value_type( id, inf ) );
        } else {
            it->second.refCount++;
        }
    }

    void remove( FutureId id ) {
        auto it = futureMap.find( id );
        if( it != futureMap.end() ) {
            it->second.refCount--;
            if( it->second.refCount <= 0 )
                futureMap.erase( it );
        } else {
            AASSERT0( 0 );
        }
    }

    void debugDump() const {
        INFRATRACE0( "futures {}", futureMap.size() );
        for( auto it = futureMap.cbegin(); it != futureMap.cend(); ++it )
            INFRATRACE0( "  #{} refcnt {} '{}'", it->first, it->second.refCount, it->second.result.toString() );
    }

    void infraProcessEvent( const NodeQItem & item );
    FutureId nextFutureId() {
        return ++nextFutureIdCount;
    }
    const Buffer& findResult( FutureId id ) const;
    virtual void run() = 0;
};

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
    void run();
    void pushEvent( const NodeQItem& item ) {
        inputQueue.push( item );
    }
    void addNode( Node* node ) {
        nodes.insert( node );
        node->parentFS = this;
        node->run();
    }
//    void removeNode( Node* node ) {
//       nodes.erase( node );
//    }

    void debugDump( int line ) const {
        INFRATRACE0( "line {} Nodes: {} ----------", line, nodes.size() );
        for( auto it = nodes.begin(); it != nodes.end(); ++it ) {
            ( *it )->debugDump();
        }
    }

    static void sampleAsyncEvent( Node* node, FutureId id );
    static Future readFile( Node* node, const char* path );
};

class NodeOne : public Node {
  public:
    void run() override {
        std::string fname( "some-file-path" );
        Future data = FS::readFile( this, fname.c_str() );
        data.then( [ = ]() {
            infraConsole.log( "READ1: file {}---{}", fname.c_str(), data.value().toString() );
            Future data2 = FS::readFile( this, "some-another-path" );
            data2.then( [ = ]() {
                infraConsole.log( "READ2: {} : {}", data.value().toString(), data2.value().toString() );
				parentFS->debugDump( __LINE__ );
			} );
        } );
    }
};
}

#endif
