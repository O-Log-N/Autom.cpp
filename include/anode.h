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

class InfraFutureBase {
  public:
    FutureFunction fn;
    int refCount;

    InfraFutureBase() {
        refCount = 1;
    }
    virtual ~InfraFutureBase() {}
    virtual void debugDump() const = 0;
};

class InfraFuture : public InfraFutureBase {
    Buffer result;

  public:
    const Buffer& getResult() const {
        return result;
    }
    void setResult( const Buffer& b ) {
        result = b;
    }
    void debugDump() const {
        INFRATRACE0( "    refcnt {} '{}'", refCount, result.toString() );
    }
};

class Future {
    FutureId futureId;
    Node* node;
    InfraFuture* infraPtr;

  public:
    Future( Node* );
    Future( const Future& );
    Future( Future&& );
    Future& operator=( const Future& );
    ~Future();
    void then( const FutureFunction& );
    FutureId getId() const {
        return futureId;
    }
    const Buffer& value() const;
};

class Node {
    using FutureMap = std::unordered_map< FutureId, std::unique_ptr< InfraFutureBase > >;
    FutureMap futureMap;
    FutureId nextFutureIdCount = 0;

  public:
    FS* parentFS;

    InfraFutureBase* insertInfraFuture( FutureId id ) {
        auto p = futureMap.insert( FutureMap::value_type( id, std::unique_ptr< InfraFutureBase >( new InfraFuture ) ) );
        AASSERT4( p.second, "Duplicated FutureId" );
        return p.first->second.get();
    }

    InfraFutureBase* findInfraFuture( FutureId id ) {
        auto it = futureMap.find( id );
        if( it != futureMap.end() )
            return it->second.get();
        return nullptr;
    }

    void futureCleanup() {
        for( auto it = futureMap.begin(); it != futureMap.end(); ) {
            AASSERT4( it->second->refCount >= 0 );
            if( it->second->refCount <= 0 )
                it = futureMap.erase( it );
            else
                ++it;
        }
    }

    void infraProcessEvent( const NodeQItem & item );

    FutureId nextFutureId() {
        return ++nextFutureIdCount;
    }

    virtual void run() = 0;

    bool isEmpty() const {
        return futureMap.size() == 0;
    }

    void debugDump() const {
        INFRATRACE0( "futures {}", futureMap.size() );
        for( auto& it : futureMap ) {
            INFRATRACE0( "  #{}", it.first );
            it.second->debugDump();
        }
    }
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

    bool isEmpty() const;

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
    void removeNode( Node* node ) {
        AASSERT4( node->isEmpty() );
        nodes.erase( node );
    }
    void debugDump( int line ) const {
        INFRATRACE0( "line {} Nodes: {} ----------", line, nodes.size() );
        for( auto it : nodes ) {
            it->debugDump();
        }
    }

    static void sampleAsyncEvent( Node* node, FutureId id );
    static Future readFile( Node* node, const char* path );
};

class NodeOne : public Node {
  public:
    void run() override {
        std::string fname( "path1" );
        Future data = FS::readFile( this, fname.c_str() );
        data.then( [ = ]() {
            infraConsole.log( "READ1: file {}---{}", fname.c_str(), data.value().toString() );
            Future data2 = FS::readFile( this, "path2" );
            data2.then( [ = ]() {
                infraConsole.log( "READ2: {} : {}", data.value().toString(), data2.value().toString() );
                Future data3 = FS::readFile( this, "path3" );
                data3.then( [ = ]() {
                    infraConsole.log( "READ3: {} : {}", data.value().toString(), data3.value().toString() );
                } );
            } );
        } );
    }
};
}

#endif
