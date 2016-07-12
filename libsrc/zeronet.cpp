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

#include "../include/zeronet.h"
#include "infra/loopcontainer.h"


namespace autom {

struct ZeroQBuffer {
    NetworkBuffer b;
    const TcpZeroSocket* sock;
};

struct ZeroQConnect {
    TcpZeroSocket* sock;
};

struct ZeroQAccept {
    TcpZeroServer* server;
    TcpZeroSocket* sock;
};

TcpZeroServer* net::createServer( LoopContainer* loop ) {
    return new TcpZeroServer( loop );
}

static void tcpCloseCb( uv_handle_t* handle ) {
    delete ( uv_tcp_t* )handle;
}

static void allocCb( uv_handle_t* handle, size_t size, uv_buf_t* buff ) {
    buff->len = size;
    buff->base = new char[size];
}

static void readCb( uv_stream_t* stream, ssize_t nread, const uv_buf_t* buff ) {
    auto item = static_cast<ZeroQBuffer*>( stream->data );
    if( nread < 0 ) {
        item->sock->onClosed();
        uv_close( ( uv_handle_t* )stream, tcpCloseCb );
        delete item->sock;
        delete item;
    } else {
        item->b.assign( buff->base, nread );
        item->sock->onRead( &item->b );
    }
    delete[] buff->base;
}

static void writeCb( uv_write_t* wr, int status ) {
    delete wr;
}

void TcpZeroSocket::read() const {
    auto item = new ZeroQBuffer;
    item->sock = this;
    stream->data = item;
    uv_read_start( stream, allocCb, readCb );
}

void TcpZeroSocket::write( const void* buff, size_t sz ) const {
    uv_write_t* wr = new uv_write_t;
    uv_buf_t b = uv_buf_init( ( char* )buff, sz );
    uv_write( wr, stream, &b, 1, writeCb );
}

void TcpZeroSocket::close() const {
    uv_close( ( uv_handle_t * )stream, tcpCloseCb );
}

static void acceptCb( uv_stream_t* server, int status ) {
    uv_tcp_t* serverConn = new uv_tcp_t;
    uv_tcp_init( server->loop, serverConn );
    if( uv_accept( server, ( uv_stream_t * )serverConn ) == 0 ) {
        AASSERT4( server->data );
        auto item = static_cast<ZeroQAccept*>( server->data );
        item->sock = new TcpZeroSocket;
        item->sock->stream = ( uv_stream_t * )serverConn;
        item->server->onConnect( item->sock );
        item->sock->onConnected();
    } else {
        uv_close( ( uv_handle_t * )serverConn, tcpCloseCb );
        auto item = static_cast<ZeroQAccept*>( server->data );
        item->server->onError();
    }
}

bool TcpZeroServer::listen( int port ) {
    handle = new uv_tcp_t;
    uv_tcp_init( loop->infraLoop(), handle );

    sockaddr_in addr;
    uv_ip4_addr( "127.0.0.1", port, &addr );
    if( 0 == uv_tcp_bind( handle, ( sockaddr* )&addr, 0 ) ) {
        if( 0 == uv_listen( ( uv_stream_t* )handle, 1024, acceptCb ) ) {
            ZeroQAccept* item = new ZeroQAccept;
            item->server = this;
            handle->data = item;
            return true;
        }
    }
    uv_close( ( uv_handle_t* )handle, tcpCloseCb );
    return false;
}

static void tcpConnectedCb( uv_connect_t* req, int status ) {
    auto item = static_cast<ZeroQConnect*>( req->data );
    if( status >= 0 ) {
        item->sock->stream = req->handle;
        item->sock->onConnected();
    } else {
        item->sock->onError();
        uv_close( ( uv_handle_t* )req->handle, tcpCloseCb );
    }
    delete item;
    delete req;
}

TcpZeroSocket* net::connect( LoopContainer* loop, const char* addr, int port ) {
    uv_tcp_t* client = new uv_tcp_t;
    uv_tcp_init( loop->infraLoop(), client );
    sockaddr_in ip;
    uv_ip4_addr( addr, port, &ip );
    uv_connect_t* req = new uv_connect_t;

    auto item = new ZeroQConnect;
    item->sock = new TcpZeroSocket;
    req->data = item;

    uv_tcp_connect( req, client, ( sockaddr* )&ip, tcpConnectedCb );
    return item->sock;
}

}
