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
class TcpServerConn;
class Timer {
    void* handle;
};

using FutureFunction = std::function< void( void ) >;
using FutureId = unsigned int;

struct NodeQItem {
    FutureId id;
    Node* node;
};

struct NodeQBuffer : public NodeQItem {
    NetworkBuffer b;
	FutureId closeId;
};

struct NodeQTimer : public NodeQItem {
    bool repeat;
};

struct NodeQAccept : public NodeQItem {
    uv_tcp_t* stream;
};

class InfraFutureBase {
  public:
    FutureFunction fn;
    int refCount;

    virtual ~InfraFutureBase() {}
    virtual void debugDump() const = 0;
};

template< typename T >
class InfraFuture : public InfraFutureBase {
    T result;

  public:
    const T& getResult() const {
        return result;
    }
    T& getResult() {
        return result;
    }
    void debugDump() const {
        INFRATRACE0( "    refcnt {}", refCount );
    }
};

template< typename T >
class Future {
    FutureId futureId;
    Node* node;
    InfraFuture< T >* infraPtr;

  public:
    explicit Future( Node* );
    Future();
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

template<typename T>
inline Future<T>::Future() {
    futureId = 0;
    node = nullptr;
    infraPtr = nullptr;
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

template< typename T >
class MultiFuture {
	FutureId futureId;
	Node* node;
	InfraFuture< T >* infraPtr;

public:
	explicit MultiFuture( Node* node );
	MultiFuture();
	MultiFuture( const MultiFuture& ) = default;
	MultiFuture( MultiFuture&& ) = default;
	MultiFuture& operator=( const MultiFuture& ) = default;

	void onEach( const FutureFunction& f );
	FutureId getId() const {
		return futureId;
	}
	const T& value() const;
};

template< typename T >
MultiFuture< T >::MultiFuture( Node* node_ ) : node( node_ ) {
	futureId = node->nextFutureId();
	infraPtr = static_cast<InfraFuture< T >*>( node->insertInfraFuture( futureId, new InfraFuture< T > ) );
	infraPtr->refCount = 0;
}

template<typename T>
inline MultiFuture<T>::MultiFuture() {
	futureId = 0;
	node = nullptr;
	infraPtr = nullptr;
}

template< typename T >
void MultiFuture< T >::onEach( const FutureFunction& fn ) {
	AASSERT4( infraPtr == node->findInfraFuture( futureId ) );
	infraPtr->fn = fn;
}

template< typename T >
const T& MultiFuture< T >::value() const {
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

    void infraProcessTimer( const NodeQTimer& item );
    void infraProcessTcpAccept( const NodeQAccept& item );
    void infraProcessTcpRead( const NodeQBuffer& item );
	void infraProcessTcpClosed( const NodeQBuffer& item );

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

	static Future< Timer > startTimer( Node* node, unsigned secDelay );
	static MultiFuture< Timer > startTimer( Node* node, unsigned secDelay, unsigned secRepeat );
	static MultiFuture< TcpServerConn > createServer( Node* node, int port );
    static bool connect( Node* node, int port );
};

class NodeOne : public Node {
  public:
    void run() override {
        auto data = FS::startTimer( this, 2 );
        data.then( [ = ]() {
            infraConsole.log( "TIMER1" );
            auto data2 = FS::startTimer( this, 5 );
            data2.then( [ = ]() {
                infraConsole.log( "TIMER2" );
                auto data3 = FS::startTimer( this, 10 );
                data3.then( [ = ]() {
                    infraConsole.log( "TIMER3" );
                } );
            } );
        } );
    }
};

class TcpServerConn {
    friend class Node;
    uv_stream_t* stream;

  public:
    Future< Buffer > read( Node* ) const;
	Future< bool > end( Node* ) const;
};

class NodeServer : public Node {
  public:
    void run() override {
        int toConnect = 0;
        auto s = FS::createServer( this, 7000 );
        if( s.getId() ) {
            INFRATRACE0( "listen 7000" );
            toConnect = 7001;
        } else {
            s = FS::createServer( this, 7001 );
            if( s.getId() ) {
                INFRATRACE0( "listen 7001" );
                toConnect = 7000;
            }
        }

        if( toConnect ) {
            s.onEach( [ = ]() {
                infraConsole.log( "Connection accepted" );
                auto fromNet = s.value().read( this );
                fromNet.then( [ = ]() {
                    INFRATRACE0( "Received '{}'", fromNet.value().toString() );
                } );
				auto end = s.value().end( this );
				end.then( [=]() {
					INFRATRACE0( "Disconnected" );
				} );
            } );

            auto data = FS::startTimer( this, 10, 5 );
            data.onEach( [ = ]() {
                INFRATRACE0( "connecting {}", toConnect );
                FS::connect( this, toConnect );
            } );
        }
    }
};

}
#endif
