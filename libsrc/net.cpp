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
#include "infra/nodecontainer.h"


using namespace autom;

TcpServer* net::createServer( Node* node ) {
    return new TcpServer( node );
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

void TcpSocket::close() const {
	zero->close();
}

MultiFuture< TcpSocket > TcpServer::listen( int port ) {
	if( !zero.listen( port ) )
		throw "ERROR";
	MultiFuture< TcpSocket > future( node );
    auto id = future.infraGetId();
    auto nd = node;
    zero.on( ID_CONNECT, [id, nd]( TcpZeroSocket * zs ) {
        NodeQAccept item;
        item.id = id;
        item.node = nd;
        item.sock = new TcpSocket;
        item.sock->zero = zs;
        item.sock->node = nd;
        nd->infraProcessTcpAccept( item );
    } );

    return future;
}

TcpSocket* net::connect( Node* node, int port ) {
	auto sock = new TcpSocket;
	sock->zero = connect( node->parentLoop, port );
//	sock->zero->on( TcpZeroSocket::ID_CONNECT, []() {
//	} );
	return sock;
}

std::exception* Buffer::fromNetwork( const NetworkBuffer& b ) {
    static int cnt = 0;
    if( ++cnt % 5 == 0 ) {
//        return new std::exception();
    }

    s = b;
    return nullptr;
}
