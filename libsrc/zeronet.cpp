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

#include "../include/aassert.h"
#include "../include/abuffer.h"
#include "../include/zeronet.h"
#include "infra/loopcontainer.h"
#include <map>

namespace autom {

class ListenerInterface;
class StreamInteface;

class StreamInteface {
  public:
    uv_stream_t* stream;

    std::function< void( void ) > onConnected;
    std::function< void( const NetworkBuffer* ) > onRead;
    std::function< void( void ) > onClosed;
    std::function< void( void ) > onError;

    StreamInteface() {
        onConnected = []() {};
        onRead = []( const NetworkBuffer* ) {};
        onClosed = []() {};
        onError = []() {};
        stream = nullptr;
    }
};

class SocketMap {
	std::map< Handle, StreamInteface > data;
	Handle nextHandle() const {
		static Handle next = 0;
		return ++next;
	}

public:
	StreamInteface* add( TcpZeroSocket* s );
	uv_stream_t* remove( Handle h );
	StreamInteface* find( Handle h );
};

StreamInteface* SocketMap::add( TcpZeroSocket* s ) {
    s->h = nextHandle();
    return &data.insert( std::map< Handle, StreamInteface >::value_type( s->h, StreamInteface() ) ).first->second;
}

StreamInteface* SocketMap::find( Handle h ) {
	auto it = data.find( h );
	if( it == data.end() )
		return nullptr;
	return &( it->second );
}

uv_stream_t* SocketMap::remove( Handle h ) {
	auto it = data.find( h );
	if( it == data.end() )
		return nullptr;
	auto p = it->second.stream;
	data.erase( it );
	return p;
}

static SocketMap sockets;

void TcpZeroSocket::on( int eventId, std::function< void( const NetworkBuffer* ) > fn ) const {
    auto sint = sockets.find( h );
    AASSERT4( sint );
    if( ID_DATA == eventId )
        sint->onRead = fn;
    else
        AASSERT4( false );
}

void TcpZeroSocket::on( int eventId, std::function< void( void ) > fn ) const {
    auto sint = sockets.find( h );
    AASSERT4( sint );
    if( ID_CLOSED == eventId )
        sint->onClosed = fn;
    else if( ID_ERROR == eventId )
        sint->onError = fn;
    else if( ID_CONNECT == eventId )
        sint->onConnected = fn;
    else
        AASSERT4( false );
}

static inline uv_stream_t* uv_tcp_to_stream( uv_tcp_t* t ) {
    return reinterpret_cast<uv_stream_t*>( t );
}

static inline uv_handle_t* uv_tcp_to_handle( uv_tcp_t* t ) {
    return reinterpret_cast<uv_handle_t*>( t );
}

static inline uv_handle_t* uv_stream_to_handle( uv_stream_t* t ) {
    return reinterpret_cast<uv_handle_t*>( t );
}

struct ZeroQBuffer {
    NetworkBuffer b;
    StreamInteface* sint;
};

struct ZeroQConnect {
    StreamInteface* sint;
};

struct ZeroQAccept {
    ListenerInterface* listener;
    StreamInteface* sint;
};

TcpZeroServer net::createServer( LoopContainer* loop ) {
    TcpZeroServer s( loop );
    return s;
}

static void tcpCloseCb( uv_handle_t* handle ) {
    delete reinterpret_cast<uv_tcp_t*>( handle );
}

static void allocCb( uv_handle_t* handle, size_t size, uv_buf_t* buff ) {
    buff->len = size;
    buff->base = new char[size];
}

static void readCb( uv_stream_t* stream, ssize_t nread, const uv_buf_t* buff ) {
    auto item = static_cast<ZeroQBuffer*>( stream->data );
    if( nread < 0 ) {
        item->sint->onClosed();
        uv_close( uv_stream_to_handle( stream ), tcpCloseCb );
        delete item;
        stream->data = nullptr;
    } else {
        item->b.assign( buff->base, nread );
        item->sint->onRead( &item->b );
    }
    delete[] buff->base;
}

static void writeCb( uv_write_t* wr, int status ) {
    delete wr;
}

void TcpZeroSocket::read() const {
    auto sint = sockets.find( h );
    AASSERT4( sint );
    if( !sint )
        return;
    auto item = new ZeroQBuffer;
    item->sint = sint;
    item->sint->stream->data = item;
    uv_read_start( item->sint->stream, allocCb, readCb );
}

void TcpZeroSocket::write( const void* buff, size_t sz ) const {
    auto sint = sockets.find( h );
    AASSERT4( sint );
    if( !sint )
        return;
    uv_write_t* wr = new uv_write_t;
    uv_buf_t b = uv_buf_init( ( char* )buff, sz );
    uv_write( wr, sint->stream, &b, 1, writeCb );
}

void TcpZeroSocket::close() const {
    auto stream = sockets.remove( h );
    if( !stream )
        return;
    uv_close( uv_stream_to_handle( stream ), tcpCloseCb );
}

class ListenerInterface {
  public:
    uv_tcp_t* listenerTcp;
    LoopContainer* loop;

    std::function< void( TcpZeroSocket ) > onConnect;
    std::function< void( void ) > onError;

    explicit ListenerInterface( LoopContainer* loop_ ) {
        loop = loop_;
        listenerTcp = nullptr;
        onConnect = []( TcpZeroSocket ) {};
        onError = []() {};
    }
};

class ListenerMap {
	std::map< Handle, ListenerInterface > data;
	Handle nextHandle() const {
		static Handle next = 0;
		return ++next;
	}

public:
	void add( TcpZeroServer* s, LoopContainer* loop ) {
		s->h = nextHandle();
		data.insert( std::map< Handle, ListenerInterface >::value_type( s->h, ListenerInterface( loop ) ) );
	}
	void remove( Handle h ) {
		data.erase( h );
	}
	ListenerInterface* find( Handle h );
};

ListenerInterface* ListenerMap::find( Handle h ) {
	auto it = data.find( h );
	if( it == data.end() )
		return nullptr;
	return &( it->second );	
}

static ListenerMap servers;

static void acceptCb( uv_stream_t* server, int status ) {
    auto serverConn = new uv_tcp_t;
    uv_tcp_init( server->loop, serverConn );
    if( uv_accept( server, uv_tcp_to_stream( serverConn ) ) == 0 ) {
        AASSERT4( server->data );
        auto item = static_cast<ZeroQAccept*>( server->data );
        TcpZeroSocket newSock;
        item->sint = sockets.add( &newSock );
        item->sint->stream = uv_tcp_to_stream( serverConn );
        item->listener->onConnect( newSock );
        item->sint->onConnected();
    } else {
        uv_close( uv_tcp_to_handle( serverConn ), tcpCloseCb );
        auto item = static_cast<ZeroQAccept*>( server->data );
        item->listener->onError();
    }
}

TcpZeroServer::TcpZeroServer( LoopContainer* loop_ ) {
    servers.add( this, loop_ );
}

void TcpZeroServer::on( int eventId, std::function< void( TcpZeroSocket ) > fn ) {
    auto sint = servers.find( h );
    if( TcpZeroServer::ID_CONNECT == eventId )
        sint->onConnect = fn;
    else
        AASSERT4( 0 );
}

void TcpZeroServer::on( int eventId, std::function< void( void ) > fn ) {
    auto sint = servers.find( h );
    if( TcpZeroServer::ID_ERROR == eventId )
        sint->onError = fn;
    else
        AASSERT4( 0 );
}

bool TcpZeroServer::listen( int port ) {
    auto sint = servers.find( h );
    sint->listenerTcp = new uv_tcp_t;
    uv_tcp_init( sint->loop->infraLoop(), sint->listenerTcp );

    sockaddr_in addr;
    uv_ip4_addr( "127.0.0.1", port, &addr );
    if( 0 == uv_tcp_bind( sint->listenerTcp, ( sockaddr* )&addr, 0 ) ) {
        if( 0 == uv_listen( uv_tcp_to_stream( sint->listenerTcp ), 1024, acceptCb ) ) {
            auto item = new ZeroQAccept;
            item->listener = sint;
            sint->listenerTcp->data = item;
            return true;
        }
    }
    uv_close( uv_tcp_to_handle( sint->listenerTcp ), tcpCloseCb );
    return false;
}

static void tcpConnectedCb( uv_connect_t* req, int status ) {
    auto sint = static_cast<ZeroQConnect*>( req->data );
    if( status >= 0 ) {
        sint->sint->stream = req->handle;
        sint->sint->onConnected();
    } else {
        sint->sint->onError();
        uv_close( uv_stream_to_handle( req->handle ), tcpCloseCb );
    }
    delete sint;
    delete req;
}

TcpZeroSocket net::connect( LoopContainer* loop, const char* addr, int port ) {
    auto client = new uv_tcp_t;
    uv_tcp_init( loop->infraLoop(), client );
    sockaddr_in ip;
    uv_ip4_addr( addr, port, &ip );
    uv_connect_t* req = new uv_connect_t;

    auto item = new ZeroQConnect;
    TcpZeroSocket newSock;
    item->sint = sockets.add( &newSock );
    req->data = item;

    uv_tcp_connect( req, client, ( sockaddr* )&ip, tcpConnectedCb );
    return newSock;
}

}
