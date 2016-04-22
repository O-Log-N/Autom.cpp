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

#include "..\include\net.h"
#include "..\include\anode.h"
#include "infra\nodecontainer.h"


using namespace autom;

TcpServer net::createServer( Node* node ) {
	return TcpServer( node );
}

static void tcpCloseCb( uv_handle_t* handle ) {
	delete (uv_tcp_t*)handle;
}

static void readCb( uv_stream_t* stream, ssize_t nread, const uv_buf_t* buff ) {
	auto item = (NodeQBuffer*)stream->data;
	if( nread < 0 ) {
		item->node->infraProcessTcpClosed( *item );
		uv_close( (uv_handle_t*)stream, tcpCloseCb );
		delete item;
	} else {
		item->b.assign( buff->base, nread );
		item->node->infraProcessTcpRead( *item );
	}
	delete[] buff->base;
}

static void allocCb( uv_handle_t* handle, size_t size, uv_buf_t* buff ) {
	buff->len = size;
	buff->base = new char[size];
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
	( (NodeQBuffer*)( stream->data ) )->closeId = future.infraGetId();
	return future;
}

static void acceptCb( uv_stream_t* server, int status ) {
	uv_tcp_t* serverConn = new uv_tcp_t;
	uv_tcp_init( server->loop, serverConn );
	if( uv_accept( server, (uv_stream_t *)serverConn ) == 0 ) {
		AASSERT4( server->data );
		auto item = static_cast<NodeQAccept*>( server->data );
		item->stream = serverConn;
		item->node->infraProcessTcpAccept( *item );
	} else {
		uv_close( (uv_handle_t *)serverConn, tcpCloseCb );
	}
}

MultiFuture< TcpServerConn > TcpServer::listen( int port ) {
	handle = new uv_tcp_t;
	uv_tcp_init( node->parentFS->infraLoop(), handle );

	sockaddr_in addr;
	uv_ip4_addr( "127.0.0.1", port, &addr );
	if( 0 == uv_tcp_bind( handle, (sockaddr*)&addr, 0 ) ) {
		if( 0 == uv_listen( (uv_stream_t*)handle, 1024, acceptCb ) ) {
			MultiFuture< TcpServerConn > future( node );
			NodeQAccept* item = new NodeQAccept;
			item->id = future.infraGetId();
			item->node = node;
			handle->data = item;
			return future;
		}
	}
	uv_close( (uv_handle_t*)handle, tcpCloseCb );
	return MultiFuture< TcpServerConn >();
}

static void writeCb( uv_write_t* wr, int status ) {
	uv_close( (uv_handle_t*)wr->handle, tcpCloseCb );
	delete wr;
}

Future< TcpClientConn::WriteCompleted > TcpClientConn::write( Node* node, void* buff, size_t sz ) const {
	Future< WriteCompleted > future( node );
	uv_write_t* wr = new uv_write_t;
	uv_buf_t buf = uv_buf_init( (char*)buff, sz );
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

	uv_tcp_connect( req, client, (sockaddr*)&addr, tcpConnectedCb );
	return future;
}

void Buffer::fromNetwork( const NetworkBuffer& b ) {
	s = b;
}
