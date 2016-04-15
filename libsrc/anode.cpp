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

void Node::infraProcessEvent( const NodeQItem& item ) {
    auto it = futureMap.find( item.id );
    if( it != futureMap.end() ) {
        it->second->setResult( item.b );
        it->second->fn();

        it->second->fn = FutureFunction();
        //The line above effectively destroys existing lambda it->second.fn
        //  First, we CAN do it, as we don't need second.fn anymore at all
        //  Second, we SHOULD do it, to avoid cyclical references from lambda
        //    to our InfraFutures, which will prevent futureCleanup() from
        //    destroying InfraFuture - EVER

        futureCleanup();
    }
}

bool FS::isEmpty() const {
    for( auto it : nodes )
        if( !it->isEmpty() )
            return false;
    return true;
}

void FS::run() {
    uv_run( &uvLoop, UV_RUN_DEFAULT );
    debugDump( __LINE__ );
}

static void timerCloseCb( uv_handle_t* handle ) {
    delete ( uv_timer_t* )handle;
}

static void closeTcpCb( uv_handle_t* handle ) {
	delete (uv_tcp_t*)handle;
}

static void timerFn( uv_timer_t* handle ) {
    AASSERT4( handle->data );
    const NodeQItem* item = static_cast<NodeQItem*>( handle->data );
    item->node->infraProcessEvent( *item );
    delete item;
    handle->data = nullptr;
    uv_timer_stop( handle );
    uv_close( ( uv_handle_t* )handle, timerCloseCb );
}

Future< Buffer > FS::readFile( Node* node, const char* path ) {
    uv_timer_t* timer = new uv_timer_t;
    uv_timer_init( &node->parentFS->uvLoop, timer );

    Future< Buffer > future( node );

    NodeQItem* item = new NodeQItem;
    item->id = future.getId();
    item->b = NetworkBuffer( fmt::format( "Hello Future {}!", item->id ) );
    item->node = node;

    timer->data = item;
    uv_timer_start( timer, timerFn, 2000, 0 );
    return future;
}

static void readAllocCb( uv_handle_t* handle, size_t size, uv_buf_t* buff ) {
    buff->len = size;
    buff->base = new char[buff->len];
}

static void readCb( uv_stream_t* stream, ssize_t nread, const uv_buf_t* buff ) {
    if( nread < 0 ) {
        uv_read_stop( stream );
        return;
    }
    INFRATRACE0( "{}", buff->base );
}

static void acceptCb( uv_stream_t* server, int status ) {
    uv_tcp_t* serverConn = new uv_tcp_t;
    uv_tcp_init( server->loop, serverConn );
    if( uv_accept( server, ( uv_stream_t * )serverConn ) == 0 ) {
        uv_read_start( ( uv_stream_t * )serverConn, readAllocCb, readCb );
    } else {
        uv_close( ( uv_handle_t * )serverConn, closeTcpCb );
    }
}

bool FS::listen( Node* node, int port ) {
    uv_tcp_t* server = new uv_tcp_t;
    uv_tcp_init( &node->parentFS->uvLoop, server );
    sockaddr_in addr;
    uv_ip4_addr( "127.0.0.1", port, &addr );
    if( UV_EADDRINUSE == uv_tcp_bind( server, ( sockaddr* )&addr, 0 ) )
		goto busy;
    if( uv_listen( ( uv_stream_t * )server, 1024, acceptCb ) )
        goto busy;
	return true;

busy:
	uv_close( (uv_handle_t *)server, closeTcpCb );
	return false;
}

static void onWriteCompl( uv_write_t* wr, int status ) {
    delete wr;
}

static void onConnected( uv_connect_t* req, int status ) {
    const char toWirite[] = "bip bip";
    uv_buf_t buf = uv_buf_init( const_cast<char*>(toWirite), strlen( toWirite ) + 1 );
    uv_write_t* wr = new uv_write_t;
    uv_write( wr, req->handle, &buf, 1, onWriteCompl );
	delete req;
}

bool FS::connect( Node* node, int port ) {
    uv_tcp_t* client = new uv_tcp_t;
    uv_tcp_init( &node->parentFS->uvLoop, client );
    sockaddr_in addr;
    uv_ip4_addr( "127.0.0.1", port, &addr );
    uv_connect_t* connect = new uv_connect_t;
    uv_tcp_connect( connect, client, ( sockaddr* )&addr, onConnected );
    return true;
}
