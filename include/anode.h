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

//#include <mutex>
//#include <condition_variable>
//#include <deque>
#include <set>
#include <unordered_map>

#include "abuffer.h"
//#include "aconsole.h"
#include "../libsrc/infra/infraconsole.h"
#include "aassert.h"
#include "../3rdparty/libuv/include/uv.h"

namespace autom {

class Node;
class FS;
using FutureFunction = std::function< void( void ) >;
using FutureId = unsigned int;

struct NodeQItem {
    FutureId id;
    Node* node;
    NetworkBuffer b;
};

class InfraFutureBase {
  public:
    FutureFunction fn;
    int refCount;
    const void* externId;

    virtual ~InfraFutureBase() {}
    virtual void setResult( const NetworkBuffer& b ) = 0;
    virtual void debugDump() const = 0;
};

template< typename T >
class InfraFuture : public InfraFutureBase {
    T result;

  public:
    const T& getResult() const {
        return result;
    }
    void setResult( const NetworkBuffer& b ) override {
        result.fromNetwork( b );
    }
    void debugDump() const {
        INFRATRACE0( "    refcnt {} '{}'", refCount, result.toString() );
    }
};

template< typename T >
class Future {
    FutureId futureId;
    Node* node;
    InfraFuture< T >* infraPtr;

  public:
    explicit Future( Node* );
    Future( const Future& );
    Future( Future&& );
    Future& operator=( const Future& );
    ~Future();
    void then( const FutureFunction& );
    FutureId getId() const {
        return futureId;
    }
    const T& value() const;
};

template< typename T >
Future< T >::Future( Node* node_ ) : node( node_ ) {
    futureId = node->nextFutureId();
    infraPtr = static_cast<InfraFuture< T >*>( node->insertInfraFuture( futureId, new InfraFuture< T > ) );
    infraPtr->refCount = 1;
}

template< typename T >
Future< T >::Future( const Future& other ) :
    futureId( other.futureId ), node( other.node ), infraPtr( other.infraPtr ) {
    AASSERT4( infraPtr == node->findInfraFuture( futureId ) );
    infraPtr->refCount++;
}

template< typename T >
Future< T >& Future< T >::operator=( const Future& other ) {
    futureId = other.futureId;
    node = other.node;
    infraPtr = other.infraPtr;
    AASSERT4( infraPtr == node->findInfraFuture( futureId ) );
    infraPtr->refCount++;
    return *this;
}

template< typename T >
Future< T >::Future( Future&& other ) :
    futureId( other.futureId ), node( other.node ), infraPtr( other.infraPtr ) {
    AASSERT4( infraPtr == node->findInfraFuture( futureId ) );
    infraPtr->refCount++;
}

template< typename T >
Future< T >::~Future() {
    // TODO: check ( infraPtr == node->findInfraFuture( futureId ) )
    infraPtr->refCount--;
}

template< typename T >
void Future< T >::then( const FutureFunction& fn ) {
    AASSERT4( infraPtr == node->findInfraFuture( futureId ) );
    infraPtr->fn = fn;
}

template< typename T >
const T& Future< T >::value() const {
    AASSERT4( infraPtr == node->findInfraFuture( futureId ) );
    return infraPtr->getResult();
}

class Node {
    using FutureMap = std::unordered_map< FutureId, std::unique_ptr< InfraFutureBase > >;
    FutureMap futureMap;
    FutureId nextFutureIdCount = 0;

  public:
    FS* parentFS;

    InfraFutureBase* insertInfraFuture( FutureId id, InfraFutureBase* inf ) {
        auto p = futureMap.insert( FutureMap::value_type( id, inf ) );
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
            if( it->second->refCount <= 0 ) {
                it = futureMap.erase( it );
            } else {
                ++it;
            }
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
/*
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
*/
class FS {
    std::set< Node* > nodes;
    uv_loop_t uvLoop;

    bool isEmpty() const;

  public:
    FS() {
        uv_loop_init( &uvLoop );
    }
    ~FS() {
        uv_loop_close( &uvLoop );
    }

    void run();
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

    static Future< Buffer > readFile( Node* node, const char* path );
    static bool listen( Node* node, int port );
    static bool connect( Node* node, int port );
};

class NodeOne : public Node {
  public:
    void run() override {
        std::string fname( "path1" );
        Future< Buffer > data = FS::readFile( this, fname.c_str() );
        data.then( [ = ]() {
            infraConsole.log( "READ1: file {}---{}", fname.c_str(), data.value().toString() );
            Future< Buffer > data2 = FS::readFile( this, "path2" );
            data2.then( [ = ]() {
                infraConsole.log( "READ2: {} : {}", data.value().toString(), data2.value().toString() );
                Future< Buffer > data3 = FS::readFile( this, "path3" );
                data3.then( [ = ]() {
                    infraConsole.log( "READ3: {} : {}", data.value().toString(), data3.value().toString() );
                } );
            } );
        } );
    }
};

class NodeServer : public Node {
  public:
    void run() override {
        if( FS::listen( this, 7000 ) ) {
            INFRATRACE0( "listen 7000" );
        } else if( FS::listen( this, 7001 ) ) {
            INFRATRACE0( "listen 7001" );
        }
    }
};

}
#endif
