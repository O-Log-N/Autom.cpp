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
class TcpClientConn;

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
};

struct NodeQAccept : public NodeQItem {
    uv_tcp_t* stream;
};

struct NodeQConnect : public NodeQItem {
    uv_stream_t* stream;
};


class InfraFutureBase {
  public:
    FutureFunction fn;
    int refCount;
    bool multi;

    virtual ~InfraFutureBase() {}
    void cleanup() {
        if( multi )
            return;

        fn = FutureFunction();
        //The line above effectively destroys existing lambda it->second.fn
        //  First, we CAN do it, as we don't need second.fn anymore at all
        //  Second, we SHOULD do it, to avoid cyclical references from lambda
        //    to our InfraFutures, which will prevent futureCleanup() from
        //    destroying InfraFuture - EVER
        refCount--;
    }
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
        INFRATRACE0( "    refcnt {} {}", refCount, multi );
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
    auto f = new InfraFuture< T >;
    f->refCount = 1;
    f->multi = false;
    infraPtr = static_cast<InfraFuture< T >*>( node->insertInfraFuture( futureId, f ) );
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
    infraPtr->refCount++;
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
    auto f = new InfraFuture< T >;
    f->refCount = 0;
    f->multi = true;
    infraPtr = static_cast<InfraFuture< T >*>( node->insertInfraFuture( futureId, f ) );
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

    FutureId nextFutureId() {
        return ++nextFutureIdCount;
    }

    InfraFutureBase* insertInfraFuture( FutureId id, InfraFutureBase* inf );
    InfraFutureBase* findInfraFuture( FutureId id );
    void futureCleanup();

    void infraProcessTimer( const NodeQTimer& item );
    void infraProcessTcpAccept( const NodeQAccept& item );
    void infraProcessTcpRead( const NodeQBuffer& item );
    void infraProcessTcpClosed( const NodeQBuffer& item );
    void infraProcessTcpConnect( const NodeQConnect& item );

    virtual void run() = 0;

    bool isEmpty() const;
    void debugDump() const;
};

class Timer {
    void* handle;
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
    static Future< TcpClientConn > connect( Node* node, int port );
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
    class Disconnected {};
    uv_stream_t* stream;

  public:
    Future< Buffer > read( Node* ) const;
    Future< Disconnected > end( Node* ) const;
};

class TcpClientConn {
    friend class Node;
    class WriteCompleted {};
    uv_stream_t* stream;

  public:
    Future< WriteCompleted > write( Node*, void* buff, size_t sz ) const;
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
                end.then( [ = ]() {
                    INFRATRACE0( "Disconnected" );
                } );
            } );

            auto data = FS::startTimer( this, 10, 5 );
            data.onEach( [ = ]() {
                INFRATRACE0( "connecting {}", toConnect );
                auto c = FS::connect( this, toConnect );
                c.then( [ = ]() {
                    INFRATRACE0( "Writing..." );
                    c.value().write( this, "bom bom", 8 );
                } );
            } );
        }
    }
};

}
#endif
