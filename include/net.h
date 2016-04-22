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

#include "../3rdparty/libuv/include/uv.h"
#include "future.h"
#include "abuffer.h"
#include "anode.h"

namespace autom {

class Node;

struct NodeQBuffer : public NodeQItem {
    NetworkBuffer b;
    FutureId closeId;
};

struct NodeQAccept : public NodeQItem {
    uv_tcp_t* stream;
};

struct NodeQConnect : public NodeQItem {
    uv_stream_t* stream;
};

class TcpServerConn {
    friend class Node;
    class Disconnected {};
    uv_stream_t* stream;

  public:
    MultiFuture< Buffer > read( Node* ) const;
    Future< Buffer > write( Node*, void* buff, size_t sz ) const {};
    Future< Disconnected > end( Node* ) const;
};

class TcpClientConn {
    friend class Node;
    class WriteCompleted {};
    uv_stream_t* stream;

  public:
    Future< WriteCompleted > write( Node*, void* buff, size_t sz ) const;
};

class TcpServer {
    Node* node;
    uv_tcp_t* handle;

  public:
    explicit TcpServer( Node* node_ ) : node( node_ ) {}
    MultiFuture< TcpServerConn > listen( int port );
};

namespace net {

TcpServer createServer( Node* node );
Future< TcpClientConn > connect( Node* node, int port );

}

}

#endif


