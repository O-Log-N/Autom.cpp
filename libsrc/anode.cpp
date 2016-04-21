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

void InfraNodeContainer::run() {
    uv_run( &uvLoop, UV_RUN_DEFAULT );
}

static void timerCloseCb( uv_handle_t* handle ) {
    delete ( uv_timer_t* )handle;
}

static void tcpCloseCb( uv_handle_t* handle ) {
    delete ( uv_tcp_t* )handle;
}

static void timerCb( uv_timer_t* handle ) {
    AASSERT4( handle->data );
    auto* item = static_cast<const NodeQTimer*>( handle->data );
    item->node->infraProcessTimer( *item );
    if( !uv_timer_get_repeat( handle ) ) {
        handle->data = nullptr;
        uv_close( ( uv_handle_t* )handle, timerCloseCb );
        delete item;
    }
}

Future< Timer > net::startTimout( Node* node, unsigned secDelay ) {
    Future< Timer > future( node );

    NodeQTimer* item = new NodeQTimer;
    item->id = future.infraGetId();
    item->node = node;

    uv_timer_t* timer = new uv_timer_t;
    uv_timer_init( node->parentFS->infraLoop(), timer );
    timer->data = item;
    uv_timer_start( timer, timerCb, secDelay * 1000, 0 );

    return future;
}

MultiFuture< Timer > net::setInterval( Node* node, unsigned secRepeat ) {
    AASSERT4( secRepeat > 0 );
    MultiFuture< Timer > future( node );

    NodeQTimer* item = new NodeQTimer;
    item->id = future.infraGetId();
    item->node = node;

    uv_timer_t* timer = new uv_timer_t;
    uv_timer_init( node->parentFS->infraLoop(), timer );
    timer->data = item;
    uv_timer_start( timer, timerCb, secRepeat * 1000, secRepeat * 1000 );

    return future;
}

static void allocCb( uv_handle_t* handle, size_t size, uv_buf_t* buff ) {
    buff->len = size;
    buff->base = new char[size];
}

void Node::infraProcessTimer( const NodeQTimer& item ) {
    auto it = futureMap.find( item.id );
    if( it != futureMap.end() ) {
        it->second->fn();
        it->second->cleanup();
        futureCleanup();
    }
}

void Node::infraProcessTcpAccept( const NodeQAccept& item ) {
    auto it = futureMap.find( item.id );
    if( it != futureMap.end() ) {
        auto f = static_cast<InfraFuture< TcpServerConn >*>( it->second.get() );
        f->getResult().stream = ( uv_stream_t * )item.stream;
        it->second->fn();
        it->second->cleanup();
        futureCleanup();
    }
}

void Node::infraProcessTcpRead( const NodeQBuffer& item ) {
    auto it = futureMap.find( item.id );
    if( it != futureMap.end() ) {
        auto f = static_cast<InfraFuture< Buffer >*>( it->second.get() );
        f->getResult().fromNetwork( item.b );
        it->second->fn();
        it->second->cleanup();
        futureCleanup();
    }
}

void Node::infraProcessTcpClosed( const NodeQBuffer& item ) {
    auto it = futureMap.find( item.closeId );
    if( it != futureMap.end() ) {
        it->second->fn();
        it->second->cleanup();
        futureCleanup();
    }
}

void Node::infraProcessTcpConnect( const NodeQConnect& item ) {
    auto it = futureMap.find( item.id );
    if( it != futureMap.end() ) {
        auto f = static_cast<InfraFuture< TcpClientConn >*>( it->second.get() );
        f->getResult().stream = item.stream;
        it->second->fn();
        it->second->cleanup();
        futureCleanup();
    }
}

InfraFutureBase* Node::insertInfraFuture( FutureId id, InfraFutureBase* inf ) {
    auto p = futureMap.insert( FutureMap::value_type( id, inf ) );
    AASSERT4( p.second, "Duplicated FutureId" );
    return p.first->second.get();
}

InfraFutureBase* Node::findInfraFuture( FutureId id ) {
    auto it = futureMap.find( id );
    if( it != futureMap.end() )
        return it->second.get();
    return nullptr;
}

void Node::futureCleanup() {
    for( auto it = futureMap.begin(); it != futureMap.end(); ) {
        AASSERT4( it->second->refCount >= 0 );
        if( ( !it->second->multi ) && ( it->second->refCount <= 0 ) ) {
            it = futureMap.erase( it );
        } else {
            ++it;
        }
    }
}

bool Node::isEmpty() const {
    for( auto& it : futureMap ) {
        if( ( !it.second->multi ) && it.second->refCount )
            return false;
    }
    return true;
}

void Node::debugDump() const {
    INFRATRACE0( "futures {}", futureMap.size() );
    for( auto& it : futureMap ) {
        INFRATRACE0( "  #{}", it.first );
        it.second->debugDump();
    }
}

static void readCb( uv_stream_t* stream, ssize_t nread, const uv_buf_t* buff ) {
    auto item = ( NodeQBuffer* )stream->data;
    if( nread < 0 ) {
        item->node->infraProcessTcpClosed( *item );
        uv_close( ( uv_handle_t* )stream, tcpCloseCb );
        delete item;
    } else {
        item->b.assign( buff->base, nread );
        item->node->infraProcessTcpRead( *item );
    }
    delete[] buff->base;
}

MultiFuture< Buffer > TcpServerConn::read( Node* node ) const {
	MultiFuture< Buffer > future( node );
    NodeQBuffer* item = new NodeQBuffer;
    item->id = future.infraGetId();
    item->node = node;

    stream->data = item;
    uv_read_start( stream, allocCb, readCb );
    return future;
}

Future< TcpServerConn::Disconnected > TcpServerConn::end( Node* node ) const {
    Future< TcpServerConn::Disconnected > future( node );
    AASSERT4( stream->data );
    ( ( NodeQBuffer* )( stream->data ) )->closeId = future.infraGetId();
    return future;
}

static void acceptCb( uv_stream_t* server, int status ) {
    uv_tcp_t* serverConn = new uv_tcp_t;
    uv_tcp_init( server->loop, serverConn );
    if( uv_accept( server, ( uv_stream_t * )serverConn ) == 0 ) {
        AASSERT4( server->data );
        auto item = static_cast<NodeQAccept*>( server->data );
        item->stream = serverConn;
        item->node->infraProcessTcpAccept( *item );
    } else {
        uv_close( ( uv_handle_t * )serverConn, tcpCloseCb );
    }
}

MultiFuture< TcpServerConn > TcpServer::listen( int port ) {
    handle = new uv_tcp_t;
    uv_tcp_init( node->parentFS->infraLoop(), handle );

    sockaddr_in addr;
    uv_ip4_addr( "127.0.0.1", port, &addr );
    if( 0 == uv_tcp_bind( handle, ( sockaddr* )&addr, 0 ) ) {
        if( 0 == uv_listen( ( uv_stream_t* )handle, 1024, acceptCb ) ) {
            MultiFuture< TcpServerConn > future( node );
            NodeQAccept* item = new NodeQAccept;
            item->id = future.infraGetId();
            item->node = node;
            handle->data = item;
            return future;
        }
    }
    uv_close( ( uv_handle_t* )handle, tcpCloseCb );
    return MultiFuture< TcpServerConn >();
}

TcpServer net::createServer( Node* node ) {
    return TcpServer( node );
}

static void writeCb( uv_write_t* wr, int status ) {
    uv_close( ( uv_handle_t* )wr->handle, tcpCloseCb );
    delete wr;
}

Future< TcpClientConn::WriteCompleted > TcpClientConn::write( Node* node, void* buff, size_t sz ) const {
    Future< WriteCompleted > future( node );
    uv_write_t* wr = new uv_write_t;
    uv_buf_t buf = uv_buf_init( ( char* )buff, sz );
    uv_write( wr, stream, &buf, 1, writeCb );
    return future;
}

static void tcpConnectedCb( uv_connect_t* req, int status ) {
    auto item = static_cast<NodeQConnect*>( req->data );
    if( status >= 0 ) {
        item->stream = req->handle;
        item->node->infraProcessTcpConnect( *item );
    }
    delete item;
    delete req;
}

Future< TcpClientConn > net::connect( Node* node, int port ) {
    Future< TcpClientConn > future( node );
    uv_tcp_t* client = new uv_tcp_t;
    uv_tcp_init( node->parentFS->infraLoop(), client );
    sockaddr_in addr;
    uv_ip4_addr( "127.0.0.1", port, &addr );
    uv_connect_t* req = new uv_connect_t;

    auto item = new NodeQConnect;
    item->id = future.infraGetId();
    item->node = node;
    req->data = item;

    uv_tcp_connect( req, client, ( sockaddr* )&addr, tcpConnectedCb );
    return future;
}
