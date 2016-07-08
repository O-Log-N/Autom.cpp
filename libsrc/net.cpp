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

#include "../include/net.h"
#include "../include/anode.h"
#include "infra/nodecontainer.h"


using namespace autom;

TcpServer* net::createServer( Node* node ) {
    return new TcpServer( node );
}

static void tcpCloseCb( uv_handle_t* handle ) {
    delete ( uv_tcp_t* )handle;
}

static void allocCb( uv_handle_t* handle, size_t size, uv_buf_t* buff ) {
    buff->len = size;
    buff->base = new char[size];
}

static void writeCb( uv_write_t* wr, int status ) {
    delete wr;
}

MultiFuture< Buffer > TcpSocket::read() const {
    MultiFuture< Buffer > future( node );
    auto id = future.infraGetId();
    auto nd = node;
    zero->on( TcpZeroSocket::ID_DATA, [id, nd]( const NetworkBuffer * b ) {
        NodeQBuffer item;
        item.id = id;
        item.node = nd;
        item.b = *b;
        nd->infraProcessTcpRead( item );
    } );
    zero->on( TcpZeroSocket::ID_CLOSED, [id, nd]() {
        NodeQBuffer item;
        item.closeId = id;
        nd->infraProcessTcpClosed( item );
    } );
    zero->read();
    return future;
}

void TcpSocket::write( const void* buff, size_t sz ) const {
    zero->write( buff, sz );
}
/*
void TcpSocket::close() const {
    uv_close( ( uv_handle_t * )stream, tcpCloseCb );
}

static void acceptCb( uv_stream_t* server, int status ) {
    uv_tcp_t* serverConn = new uv_tcp_t;
    uv_tcp_init( server->loop, serverConn );
    if( uv_accept( server, ( uv_stream_t * )serverConn ) == 0 ) {
        AASSERT4( server->data );
        auto item = static_cast<NodeQAccept*>( server->data );
        item->sock = new TcpSocket;
        item->sock->stream = ( uv_stream_t * )serverConn;
//        item->node->infraProcessTcpAccept( *item );
    } else {
        uv_close( ( uv_handle_t * )serverConn, tcpCloseCb );
        auto item = static_cast<NodeQAccept*>( server->data );
    }
}
*/
MultiFuture< TcpSocket > TcpServer::listen( int port ) {
    MultiFuture< TcpSocket > future( node );
    auto id = future.infraGetId();
    auto nd = node;
//	auto srv = this;
    zero.on( ID_CONNECT, [id, nd]( TcpZeroSocket * zs ) {
        NodeQAccept item;
        item.id = id;
        item.node = nd;
//		item.server = srv;
        item.sock = new TcpSocket;
        item.sock->zero = zs;
        item.sock->node = nd;
        nd->infraProcessTcpAccept( item );
    } );

    if( ! zero.listen( port ) )
        throw "ERROR";
    return future;
}

static void tcpConnectedCb( uv_connect_t* req, int status ) {
    auto item = static_cast<NodeQConnect*>( req->data );
    if( status >= 0 ) {
//        item->stream = req->handle;
//        item->node->infraProcessTcpConnect( *item );
    } else {
        uv_close( ( uv_handle_t* )req->handle, tcpCloseCb );
    }
    delete item;
    delete req;
}
/*
TcpSocket* net::connect( Node* node, int port ) {
    uv_tcp_t* client = new uv_tcp_t;
    uv_tcp_init( node->parentLoop->infraLoop(), client );
    sockaddr_in addr;
    uv_ip4_addr( "127.0.0.1", port, &addr );
    uv_connect_t* req = new uv_connect_t;

    auto item = new NodeQConnect;
    item->sock = new TcpSocket;
	item->sock->zero = new TcpZeroSocket;
//    item->id = future.infraGetId();
//    item->node = node;
    req->data = item;

    uv_tcp_connect( req, client, ( sockaddr* )&addr, tcpConnectedCb );
    return item->sock;
}
*/
std::exception* Buffer::fromNetwork( const NetworkBuffer& b ) {
    static int cnt = 0;
    if( ++cnt % 5 == 0 ) {
//        return new std::exception();
    }

    s = b;
    return nullptr;
}
