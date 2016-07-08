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

#ifndef NET_H
#define NET_H

#include "future.h"
#include "abuffer.h"
#include "zeronet.h"

namespace autom {

class TcpSocket {
  public:
    TcpZeroSocket* zero;
    Node* node;

    void close() const;
    MultiFuture< Buffer > read() const;
    void write( const void* buff, size_t sz ) const;
};

class TcpServer {
    TcpZeroServer zero;
    Node* node;

  public:
    enum EventId { ID_ERROR = 1, ID_CONNECT = 2, };

    explicit TcpServer( Node* node_ ) : node( node_ ), zero( node_->parentLoop ) {}

    MultiFuture< TcpSocket > listen( int port );
};

namespace net {

TcpServer* createServer( Node* node );
TcpSocket* connect( Node* node, int port );

}

}

#endif
